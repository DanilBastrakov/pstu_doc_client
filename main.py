import os
import re

import numpy as np
import ollama
import pdfplumber
from duckduckgo_search import DDGS


def load_documents(folder_path: str):
    documents = []
    for filename in os.listdir(folder_path):
        full_path = os.path.join(folder_path, filename)
        if filename.endswith(".txt"):
            with open(full_path, "r", encoding="utf-8") as f:
                text = f.read()
            documents.append({"text": text, "source": filename})
        elif filename.endswith(".pdf"):
            with pdfplumber.open(full_path) as pdf:
                text = "\n".join(page.extract_text() or "" for page in pdf.pages)
            documents.append({"text": text, "source": filename})
    return documents


def chunk_by_paragraphs(text: str, max_chars: int = 1000) -> list[str]:
    paragraphs = re.split(r'\n\s*\n', text)
    chunks = []
    current = ""
    for p in paragraphs:
        p = p.strip()
        if not p:
            continue
        if len(current) + len(p) < max_chars:
            current += p + "\n\n"
        else:
            if current:
                chunks.append(current.strip())
            current = p + "\n\n"
    if current:
        chunks.append(current.strip())
    return chunks


INDEX = []


def build_index(folder="./my_docs", max_chars=1000, embed_model="nomic-embed-text"):
    global INDEX
    docs = load_documents(folder)
    INDEX = []
    for doc in docs:
        chunks = chunk_by_paragraphs(doc["text"], max_chars)
        for chunk in chunks:
            resp = ollama.embeddings(model=embed_model, prompt=chunk)
            vector = np.array(resp["embedding"], dtype=np.float32)
            INDEX.append({"vector": vector, "text": chunk, "source": doc["source"]})
    print(f"Indexed {len(INDEX)} chunks.")


# ---------- Retrieval ----------

def cosine_sim(a, b):
    return np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-10)


def retrieve(query: str, top_k=5, embed_model="nomic-embed-text") -> list[dict]:
    if not INDEX:
        return []
    q_emb = np.array(ollama.embeddings(model=embed_model, prompt=query)["embedding"], dtype=np.float32)
    scores = [(cosine_sim(q_emb, item["vector"]), i) for i, item in enumerate(INDEX)]
    scores.sort(key=lambda x: x[0], reverse=True)
    return [INDEX[i] for _, i in scores[:top_k]]


# ---------- Self-RAG: verification ----------

def extract_claims(text: str, llm_model="qwen2.5:7b") -> list[str]:
    if not text.strip():
        return []
    prompt = f"""Раздели текст ниже на отдельные фактические утверждения.
Каждое утверждение — законченная мысль, которую можно проверить по документам.
Верни список утверждений, каждое на новой строке, без нумерации.

Текст: {text}
Утверждения:"""
    resp = ollama.chat(model=llm_model, messages=[{"role": "user", "content": prompt}])
    lines = resp["message"]["content"].strip().split("\n")
    claims = []
    for line in lines:
        line = re.sub(r'^[\d\-•*."\']+', "", line).strip()
        if line and len(line) > 10:
            claims.append(line)
    return claims


def is_supported(claim: str, doc_texts: list[str], llm_model="qwen2.5:7b") -> bool:
    context = "\n".join(doc_texts[:3])
    prompt = f"""Подтверждается ли данное утверждение информацией из документов?
Отвечай только "да" или "нет".

Документы: {context}

Утверждение: {claim}
Ответ:"""
    resp = ollama.chat(model=llm_model, messages=[{"role": "user", "content": prompt}])
    return resp["message"]["content"].strip().lower().startswith("да")


def verification_step(generation: str, embed_model="nomic-embed-text", llm_model="qwen2.5:7b"):
    claims = extract_claims(generation, llm_model)
    if not claims:
        return generation, 0.0
    supported = []
    for claim in claims:
        docs = retrieve(claim, top_k=3, embed_model=embed_model)
        if docs:
            doc_texts = [d["text"] for d in docs]
            if is_supported(claim, doc_texts, llm_model):
                supported.append(claim)
        else:
            supported.append(claim)
    quality = len(supported) / len(claims)
    clean_text = ". ".join(supported)
    return clean_text, quality


# ---------- Web search ----------

def web_search(query: str, max_results=3) -> str:
    try:
        with DDGS() as ddgs:
            results = list(ddgs.text(query, max_results=max_results))
            if not results:
                return ""
            return "\n\n".join(r["body"] for r in results)
    except Exception:
        return ""


# ---------- Hybrid pipeline ----------

def hybrid_ask(query: str, llm_model="qwen2.5:7b", embed_model="nomic-embed-text") -> str:
    chunks = retrieve(query, top_k=5, embed_model=embed_model)
    context = "\n\n---\n\n".join(item["text"] for item in chunks)
    prompt = f"""Ответь на вопрос на русском языке на основе контекста ниже.

Контекст:
{context}

Вопрос: {query}
Ответ:"""
    raw_answer = ollama.chat(model=llm_model, messages=[{"role": "user", "content": prompt}])["message"]["content"]

    cleaned, confidence = verification_step(raw_answer, embed_model, llm_model)

    if confidence >= 0.7:
        return f"[✓ Уверенность {confidence:.0%}]\n{cleaned}"

    doc_context = "\n\n---\n\n".join(item["text"] for item in chunks)
    web_results = web_search(query)

    if web_results:
        prompt = f"""Дай полный ответ на вопрос, используя контекст и уже подтверждённые факты.

Контекст из локальных документов:
{doc_context}

Дополнительная информация из интернета (⚠️ Эти данные могут быть неточными или устаревшими):
{web_results}

Подтверждённые факты:
{cleaned}

Вопрос: {query}
Ответ:"""
        final_answer = ollama.chat(model=llm_model, messages=[{"role": "user", "content": prompt}])["message"]["content"]
        return f"[🔄 Скорректировано (уверенность была {confidence:.0%})]\n[⚠️ Часть данных из интернета]\n{final_answer}"

    if not doc_context.strip():
        return f"[⚠️ Уверенность {confidence:.0%}, нет источников для коррекции]\n{cleaned}"

    prompt = f"""Дай полный ответ на вопрос, используя контекст и уже подтверждённые факты.

Контекст из локальных документов:
{doc_context}

Подтверждённые факты:
{cleaned}

Вопрос: {query}
Ответ:"""
    final_answer = ollama.chat(model=llm_model, messages=[{"role": "user", "content": prompt}])["message"]["content"]
    return f"[🔄 Скорректировано (уверенность была {confidence:.0%})]\n{final_answer}"


if __name__ == "__main__":
    print("Строю индекс...")
    build_index(folder="./my_docs")

    print("\nГотово. Введите вопрос (или 'quit').")
    while True:
        q = input("\n> ")
        if q.lower() in ("quit", "exit"):
            break
        print("\n" + hybrid_ask(q))
