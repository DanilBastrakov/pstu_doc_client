import os
import numpy as np
import ollama
from pypdf import PdfReader  # optional, remove if PDF not needed

def load_documents(folder_path: str):
    documents = []
    for filename in os.listdir(folder_path):
        full_path = os.path.join(folder_path, filename)
        if filename.endswith(".txt"):
            with open(full_path, "r", encoding="utf-8") as f:
                text = f.read()
            documents.append({"text": text, "source": filename})
        elif filename.endswith(".pdf"):
            reader = PdfReader(full_path)
            text = "\n".join(page.extract_text() for page in reader.pages if page.extract_text())
            documents.append({"text": text, "source": filename})
    return documents

def chunk_text(text: str, chunk_size: int = 300, overlap: int = 50) -> list[str]:
    words = text.split()
    chunks = []
    start = 0
    while start < len(words):
        end = start + chunk_size
        chunk = " ".join(words[start:end])
        chunks.append(chunk)
        start += (chunk_size - overlap)
    return chunks

INDEX = []

def build_index(folder="./my_docs", chunk_size=300, overlap=50, embed_model="nomic-embed-text"):
    global INDEX
    docs = load_documents(folder)
    INDEX = []
    for doc in docs:
        chunks = chunk_text(doc["text"], chunk_size, overlap)
        for chunk in chunks:
            resp = ollama.embeddings(model=embed_model, prompt=chunk)
            vector = np.array(resp["embedding"], dtype=np.float32)
            INDEX.append({"vector": vector, "text": chunk, "source": doc["source"]})
    print(f"Indexed {len(INDEX)} chunks.")

# ---------- Retrieval ----------
def cosine_sim(a, b):
    return np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-10)

def retrieve(query: str, top_k=3, embed_model="nomic-embed-text") -> str:
    if not INDEX:
        return ""
    q_emb = np.array(ollama.embeddings(model=embed_model, prompt=query)["embedding"], dtype=np.float32)
    scores = [(cosine_sim(q_emb, item["vector"]), i) for i, item in enumerate(INDEX)]
    scores.sort(key=lambda x: x[0], reverse=True)
    top = [INDEX[i]["text"] for _, i in scores[:top_k]]
    return "\n\n---\n\n".join(top)

# ---------- RAG Query ----------
def ask(query: str, llm_model="llama3.2"):
    context = retrieve(query)
    prompt = f"""Use the context below to answer the question. If you can't, say so.

Context:
{context}

Question: {query}
Answer:"""
    response = ollama.chat(model=llm_model, messages=[{"role": "user", "content": prompt}])
    return response["message"]["content"]

# ---------- Main ----------
if __name__ == "__main__":
    # 1. Build index from your document folder
    build_index(folder="./my_docs")   # put your .txt/.pdf files in ./my_docs

    # 2. Interactive Q&A
    print("\nRAG ready. Type a question (or 'quit').")
    while True:
        q = input("\n> ")
        if q.lower() in ("quit", "exit"):
            break
        print("\n" + ask(q))