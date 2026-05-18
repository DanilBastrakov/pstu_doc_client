#!/usr/bin/env python3
import os
import shutil
import sys

# quick FastAPI endpoint for this AI thingamajig

if sys.stdin.encoding.lower() not in ("utf-8", "utf8"):
    sys.stdin.reconfigure(encoding="utf-8")
if sys.stdout.encoding.lower() not in ("utf-8", "utf8"):
    sys.stdout.reconfigure(encoding="utf-8")

os.environ["CHROMA_TELEMETRY"] = "false"
os.environ["NO_CHROMA_TELEMETRY"] = "true"

import httpx
import uvicorn
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from langchain_community.retrievers import BM25Retriever
from langchain_community.vectorstores import Chroma
from langchain_core.documents import Document
from langchain_core.retrievers import BaseRetriever
from langchain_core.callbacks import CallbackManagerForRetrieverRun

from main import (
    E5Embeddings,
    load_existing_index,
    build_index,
    setup_rag_chain,
    DOCUMENTS_DIR,
    CHROMA_PERSIST_DIR,
    EMBEDDING_MODEL_NAME,
    LLM_MODEL_NAME,
)



app = FastAPI(title="PSTU Doc AI Service")

vectordb = None
chunks = []
retriever = None
qa_chain = None

OLLAMA_BASE_URL = os.environ.get("OLLAMA_BASE_URL", "http://localhost:11434")
OLLAMA_TIMEOUT = int(os.environ.get("OLLAMA_TIMEOUT", "300"))


async def warm_up():
    try:
        async with httpx.AsyncClient(proxy=None) as client:
            await client.post(
                f"{OLLAMA_BASE_URL}/api/generate",
                json={"model": LLM_MODEL_NAME, "prompt": "ping", "stream": False},
                timeout=30.0,
            )
    except Exception:
        pass


class HybridRetriever(BaseRetriever):
    bm25_retriever: BM25Retriever
    chroma_retriever: BaseRetriever
    k_const: int = 60
    top_k: int = 4

    def __init__(self, bm25: BM25Retriever, chroma: Chroma):
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


class GenerateRequest(BaseModel):
    prompt: str
    model: str = LLM_MODEL_NAME
    language: str = "russian"
    use_rag: bool = True


class AskRequest(BaseModel):
    question: str
    top_k: int = 4


@app.on_event("startup")
async def startup():
    global vectordb, chunks, retriever, qa_chain

    if not os.path.exists(CHROMA_PERSIST_DIR):
        vectordb, chunks = build_index(DOCUMENTS_DIR, CHROMA_PERSIST_DIR)
    else:
        print(" ::: " + CHROMA_PERSIST_DIR)
        vectordb, chunks = load_existing_index(CHROMA_PERSIST_DIR)
        if not vectordb:
            shutil.rmtree(CHROMA_PERSIST_DIR)
            vectordb, chunks = build_index(DOCUMENTS_DIR, CHROMA_PERSIST_DIR)

    if vectordb and chunks:
        bm25 = BM25Retriever.from_documents(chunks)
        bm25.k = 20
        retriever = HybridRetriever(bm25, vectordb)

        try:
            qa_chain = setup_rag_chain(vectordb, chunks)
        except Exception:
            qa_chain = None

    await warm_up()


@app.post("/api/generate")
async def generate(req: GenerateRequest):
    augmented_prompt = req.prompt

    if req.use_rag and retriever:
        try:
            docs = retriever.invoke(req.prompt)
            if docs:
                context = "Контекст из медицинских документов:\n" + "\n\n---\n\n".join(
                    d.page_content for d in docs
                )
                augmented_prompt = f"{context}\n\n---\n\n{req.prompt}"
        except Exception as e:
                import traceback
                traceback.print_exc()
                raise HTTPException(status_code=500, detail=str(e))

    async with httpx.AsyncClient(proxy=None) as client:
        models_to_try = [req.model]
        if req.model != LLM_MODEL_NAME:
            models_to_try.append(LLM_MODEL_NAME)
        for attempt_model in models_to_try:
            response = await client.post(
                f"{OLLAMA_BASE_URL}/api/generate",
                json={
                    "model": attempt_model,
                    "prompt": augmented_prompt,
                    "stream": False,
                },
                timeout=OLLAMA_TIMEOUT,
            )
            if response.status_code == 200:
                return response.json()
        response.raise_for_status()


@app.get("/api/models")
async def models():
    async with httpx.AsyncClient() as client:
        response = await client.get(f"{OLLAMA_BASE_URL}/api/tags")
        response.raise_for_status()
        data = response.json()
        return [model["name"] for model in data.get("models", [])]


@app.post("/api/rag/ask")
async def ask(req: AskRequest):
    if not qa_chain:
        raise HTTPException(status_code=503, detail="RAG index not loaded or LLM unavailable")
    result = qa_chain.invoke({"query": req.question})
    return {
        "answer": result["result"],
        "sources": [
            {
                "source": doc.metadata.get("source", ""),
                "content": doc.page_content[:500],
            }
            for doc in result.get("source_documents", [])
        ],
    }


@app.get("/health")
async def health():
    return {
        "status": "ok",
        "rag_available": retriever is not None,
    }


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8001)
