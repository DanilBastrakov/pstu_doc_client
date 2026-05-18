#!/usr/bin/env python3

import os, re, sys, shutil
os.environ["CHROMA_TELEMETRY"] = "false"
os.environ["NO_CHROMA_TELEMETRY"] = "true"
if sys.stdin.encoding.lower() not in ("utf-8", "utf8"):
    sys.stdin.reconfigure(encoding="utf-8")
if sys.stdout.encoding.lower() not in ("utf-8", "utf8"):
    sys.stdout.reconfigure(encoding="utf-8")

from langchain_community.document_loaders import PDFPlumberLoader, TextLoader, DirectoryLoader
from langchain_text_splitters import SentenceTransformersTokenTextSplitter
from langchain_community.embeddings import HuggingFaceEmbeddings
from langchain_community.vectorstores import Chroma
from langchain_community.retrievers import BM25Retriever
from langchain_community.llms import Ollama
from langchain.chains import RetrievalQA
from langchain.prompts import PromptTemplate
from langchain_core.documents import Document
from langchain_core.retrievers import BaseRetriever
from langchain_core.callbacks import CallbackManagerForRetrieverRun, StreamingStdOutCallbackHandler


class E5Embeddings(HuggingFaceEmbeddings):
    """HuggingFaceEmbeddings wrapper that prepends the mandatory e5 prefixes.

    intfloat/multilingual-e5-* models were trained with:
      - "passage: <text>"  for documents stored in the index
      - "query: <text>"    for questions at retrieval time
    Omitting these prefixes degrades retrieval quality significantly.
    """

    def embed_documents(self, texts: list[str]) -> list[list[float]]:
        return super().embed_documents(["passage: " + t for t in texts])

    def embed_query(self, text: str) -> list[float]:
        return super().embed_query("query: " + text)

# ========== НАСТРОЙКИ (можно менять) ==========
DOCUMENTS_DIR = os.environ.get("DOCUMENTS_DIR", "./docs")   # папка с документами
CHROMA_PERSIST_DIR = "./chroma_db"                          # папка для векторной БД
# intfloat/multilingual-e5-small: 512-token limit, 384-dim, much better than MiniLM
# for Russian medical text.  e5 models REQUIRE "query:"/"passage:" prefixes.
TOKENS_PER_CHUNK = 460       # safe margin below 512-token hard limit
CHUNK_OVERLAP_TOKENS = 40    # overlap in tokens
EMBEDDING_MODEL_NAME = "intfloat/multilingual-e5-small"
LLM_MODEL_NAME = "mistral"                                  # модель Ollama (должна быть скачана)
# ==============================================

def load_documents(directory: str):
    """Рекурсивно загружает PDF (через pdfplumber) и TXT из папки."""
    loaders = {
        "**/*.pdf": PDFPlumberLoader,   # ← заменено на PDFPlumber
        "**/*.txt": TextLoader,
    }
    docs = []
    for glob_pattern, loader_cls in loaders.items():
        loader = DirectoryLoader(
            directory,
            glob=glob_pattern,
            loader_cls=loader_cls,
            recursive=True,
            show_progress=True,
        )
        docs.extend(loader.load())
    return docs

def _is_heading_line(line: str) -> bool:
    s = line.strip()
    if len(s) < 4:
        return False
    if re.match(r"^\d+(?:\.\d+)*[.)]?\s+\S", s):
        return True
    if re.match(r"^(приложение|список|термины и определения|критерии|диагностика|лечение|хирургическое лечение)\b", s.lower()):
        return True
    letters = re.findall(r"[A-ZА-ЯЁ]", s)
    if letters:
        upper_ratio = len(letters) / max(1, len(re.findall(r"[A-ZА-ЯЁa-zа-яё]", s)))
        if upper_ratio > 0.8 and 4 <= len(s) <= 120:
            return True
    return False

def _looks_like_toc(lines: list[str]) -> bool:
    if not lines:
        return False
    joined = "\n".join(lines).lower()
    if "оглавление" not in joined:
        return False
    heading_like = sum(1 for l in lines if _is_heading_line(l))
    short_lines = sum(1 for l in lines if len(l.strip()) <= 60)
    return heading_like >= max(3, len(lines) * 0.3) and short_lines >= len(lines) * 0.5

def _split_sections_with_pages(text: str):
    page_marker = re.compile(r"^\[\[PAGE:(\d+)\]\]$")
    sections = []
    current_title = "Без заголовка"
    current_lines: list[str] = []
    current_page = None
    section_page_start = None
    section_page_end = None

    def flush():
        nonlocal current_lines, current_title, section_page_start, section_page_end
        if not current_lines:
            return
        if _looks_like_toc(current_lines):
            current_lines = []
            section_page_start = None
            section_page_end = None
            return
        section_text = "\n".join(current_lines).strip()
        if section_text:
            sections.append((current_title, section_text, section_page_start, section_page_end))
        current_lines = []
        section_page_start = None
        section_page_end = None

    for raw_line in text.splitlines():
        m = page_marker.match(raw_line.strip())
        if m:
            current_page = int(m.group(1))
            if section_page_start is None:
                section_page_start = current_page
            section_page_end = current_page
            continue

        line = raw_line.rstrip()
        if _is_heading_line(line):
            flush()
            current_title = line.strip()
            current_lines = [line.strip()]
            if section_page_start is None:
                section_page_start = current_page
            if current_page is not None:
                section_page_end = current_page
            continue

        if line.strip() == "" and not current_lines:
            continue
        if section_page_start is None:
            section_page_start = current_page
        if current_page is not None:
            section_page_end = current_page
        current_lines.append(line)

    flush()
    return sections

def _sectionize_documents(docs: list[Document]) -> list[Document]:
    by_source: dict[str, list[tuple[int | None, str]]] = {}
    for doc in docs:
        source = doc.metadata.get("source", "unknown")
        page = doc.metadata.get("page")
        by_source.setdefault(source, []).append((page, doc.page_content))

    section_docs: list[Document] = []
    for source, pages in by_source.items():
        pages_sorted = sorted(pages, key=lambda p: (p[0] is None, p[0] if p[0] is not None else 0))
        combined_lines: list[str] = []
        for page, text in pages_sorted:
            if page is not None:
                combined_lines.append(f"[[PAGE:{page}]]")
            combined_lines.append(text)
        full_text = "\n".join(combined_lines)

        sections = _split_sections_with_pages(full_text)
        for title, section_text, page_start, page_end in sections:
            metadata = {
                "source": source,
                "section": title,
            }
            if page_start is not None:
                metadata["page_start"] = page_start
            if page_end is not None:
                metadata["page_end"] = page_end
            section_docs.append(Document(page_content=section_text, metadata=metadata))

    return section_docs

def build_index(docs_dir: str, persist_dir: str) -> tuple[Chroma | None, list[Document]]:
    """Создаёт и сохраняет векторное хранилище, возвращает (vectordb, chunks)."""
    print(f"Загружаем документы из {docs_dir}...")
    docs = load_documents(docs_dir)
    if not docs:
        print("Документы не найдены. Положите PDF или TXT в папку docs/")
        return None, []

    print(f"Загружено документов: {len(docs)}. Разбиваем на секции и чанки (гибрид)...")
    section_docs = _sectionize_documents(docs)
    # SentenceTransformersTokenTextSplitter splits by actual tokens of the embedding
    # model, preventing silent truncation at the model's 512-token hard limit.
    splitter = SentenceTransformersTokenTextSplitter(
        model_name=EMBEDDING_MODEL_NAME,
        tokens_per_chunk=TOKENS_PER_CHUNK,
        chunk_overlap=CHUNK_OVERLAP_TOKENS,
    )
    chunks = splitter.split_documents(section_docs)
    print(f"Чанков: {len(chunks)}")

    print("Загружаем эмбеддинги (первый запуск может скачать модель)...")
    embeddings = E5Embeddings(
        model_name=EMBEDDING_MODEL_NAME,
        model_kwargs={"device": "cpu"},
        encode_kwargs={"normalize_embeddings": True},
    )

    print(f"Строим векторную БД в {persist_dir}...")
    vectordb = Chroma.from_documents(
        documents=chunks,
        embedding=embeddings,
        persist_directory=persist_dir,
        collection_metadata={"hnsw:space": "cosine"},
    )
    print("Индекс готов.")
    return vectordb, chunks

def load_existing_index(persist_dir: str) -> tuple[Chroma | None, list[Document]]:
    """Загружает существующее векторное хранилище и извлекает хранящиеся документы."""
    if not os.path.exists(persist_dir):
        return None, []
    print(f"Загружаем индекс из {persist_dir}...")
    embeddings = E5Embeddings(
        model_name=EMBEDDING_MODEL_NAME,
        model_kwargs={"device": "cpu"},
        encode_kwargs={"normalize_embeddings": True},
    )
    vectordb = Chroma(
        persist_directory=persist_dir,
        embedding_function=embeddings,
        collection_metadata={"hnsw:space": "cosine"},
    )

    try:
        count = vectordb._collection.count()
        print(f"Индекс загружен, чанков: {count}")
        # Retrieve stored documents from Chroma for BM25 construction
        stored = vectordb.get(limit=count)
        chunks = [
            Document(page_content=t, metadata=m)
            for t, m in zip(stored["documents"], stored["metadatas"])
        ]
        print(f"Загружено документов для BM25: {len(chunks)}")
        return vectordb, chunks
    except Exception:
        print("Индекс повреждён, будет перестроен.")
        return None, []

def setup_rag_chain(vectordb, chunks: list[Document]):
    """Создаёт цепочку RetrievalQA с гибридным ретривером (BM25 + Chroma, RRF)."""
    print(f"Строим гибридный ретривер (BM25 + плотные вектора)...")

    bm25_retriever = BM25Retriever.from_documents(chunks)
    bm25_retriever.k = 20
    chroma_retriever = vectordb.as_retriever(search_kwargs={"k": 20})

    class HybridRetriever(BaseRetriever):
        """RRF-ретривер, комбинирующий BM25 и Chroma."""
        bm25_retriever: BM25Retriever
        chroma_retriever: BaseRetriever
        k_const: int = 60
        top_k: int = 4

        def __init__(self, bm25: BM25Retriever, chroma: "Chroma"):
            super().__init__(
                bm25_retriever=bm25,
                chroma_retriever=chroma.as_retriever(search_kwargs={"k": 20}),
            )

        def _get_relevant_documents(
            self, query: str, *, run_manager: CallbackManagerForRetrieverRun
        ) -> list[Document]:
            bm25_docs = self.bm25_retriever.invoke(query)
            chroma_docs = self.chroma_retriever.invoke(query)
            rrf: dict[str, list] = {}
            for rank, doc in enumerate(bm25_docs):
                rrf[doc.page_content[:200]] = [1.0 / (self.k_const + rank), doc]
            for rank, doc in enumerate(chroma_docs):
                key = doc.page_content[:200]
                if key in rrf:
                    rrf[key][0] += 1.0 / (self.k_const + rank)
                else:
                    rrf[key] = [1.0 / (self.k_const + rank), doc]
            sorted_docs = sorted(rrf.values(), key=lambda x: -x[0])
            return [doc for _, doc in sorted_docs[: self.top_k]]

    hybrid = HybridRetriever(bm25_retriever, vectordb)

    print(f"Подключаем LLM Ollama: {LLM_MODEL_NAME}...")
    try:
        llm = Ollama(model=LLM_MODEL_NAME, temperature=0.0, num_gpu=-1, num_thread=8, callbacks=[StreamingStdOutCallbackHandler()])
    except Exception as e:
        print(f"Ошибка соединения с Ollama. Убедитесь, что сервер запущен и модель "
              f"'{LLM_MODEL_NAME}' скачана (ollama pull {LLM_MODEL_NAME}).\n{e}")
        sys.exit(1)

    prompt_template = """Ты — врачебный ассистент, который отвечает строго по предоставленному контексту. Пользователь - врач, ты должен вернуть точную последовательность действий.
Если ответа нет в контексте, скажи, что не знаешь. Не придумывай.

Контекст:
{context}

Вопрос: {question}
Ответ:"""

    PROMPT = PromptTemplate(template=prompt_template, input_variables=["context", "question"])

    qa_chain = RetrievalQA.from_chain_type(
        llm=llm,
        chain_type="stuff",
        retriever=hybrid,
        return_source_documents=True,
        chain_type_kwargs={"prompt": PROMPT},
    )
    return qa_chain

def main():
    if not os.path.exists(CHROMA_PERSIST_DIR):
        print("Индекс не найден, создаём новый...")
        vectordb, chunks = build_index(DOCUMENTS_DIR, CHROMA_PERSIST_DIR)
        if not vectordb:
            sys.exit(1)
    else:
        vectordb, chunks = load_existing_index(CHROMA_PERSIST_DIR)
        if not vectordb:
            print("Перестраиваем индекс...")
            shutil.rmtree(CHROMA_PERSIST_DIR)
            vectordb, chunks = build_index(DOCUMENTS_DIR, CHROMA_PERSIST_DIR)
            if not vectordb:
                sys.exit(1)

    qa_chain = setup_rag_chain(vectordb, chunks)

    print("\n=== Локальный RAG готов ===")
    print(f"Папка с документами: {DOCUMENTS_DIR}")
    print(f"Эмбеддинг-модель:     {EMBEDDING_MODEL_NAME}")
    print(f"LLM:                  {LLM_MODEL_NAME}")
    print("Ретривер:             Гибридный (BM25 + Chroma, RRF)")
    print("Команды: 'exit' — выход, 'reindex' — переиндексация документов.\n")

    while True:
        query = input("Ваш вопрос: ")
        if query.lower() in ("exit", "quit"):
            break
        if query.lower() == "reindex":
            ans = input("Точно удалить текущий индекс и пересоздать? (y/n): ")
            if ans.lower() == "y":
                shutil.rmtree(CHROMA_PERSIST_DIR)
                vectordb, chunks = build_index(DOCUMENTS_DIR, CHROMA_PERSIST_DIR)
                qa_chain = setup_rag_chain(vectordb, chunks)
            continue
        if not query.strip():
            continue

        print("Ищем и генерируем ответ...")
        try:
            result = qa_chain.invoke({"query": query})
            print(f"\nОтвет: {result['result']}\n")
            sources = result.get("source_documents", [])
            if sources:
                print("Источники:")
                for i, doc in enumerate(sources[:3], 1):
                    src = doc.metadata.get("source", "неизвестный")
                    page_start = doc.metadata.get("page_start", None)
                    page_end = doc.metadata.get("page_end", None)
                    if page_start is not None:
                        if page_end and page_end != page_start:
                            page_info = f" (стр. {page_start+1}–{page_end+1})"
                        else:
                            page_info = f" (стр. {page_start+1})"
                    else:
                        page_info = ""
                    print(f"  {i}. {src}{page_info}")
                print()
        except Exception as e:
            print(f"Ошибка: {e}")

if __name__ == "__main__":
    main()
