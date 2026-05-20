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
os.environ["POSTHOG_DISABLED"] = "1"

import json
import httpx
import posthog
import uvicorn

# chromadb 0.5.5 calls capture() with args incompatible with posthog ≥7 — silence it
posthog.capture = lambda *a, **kw: None
from fastapi import FastAPI, HTTPException
from fastapi.responses import StreamingResponse
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

_client: httpx.AsyncClient | None = None


def _get_http_client() -> httpx.AsyncClient:
    global _client
    if _client is None:
        _client = httpx.AsyncClient(base_url=OLLAMA_BASE_URL, timeout=OLLAMA_TIMEOUT)
    return _client


async def warm_up():
    try:
        client = _get_http_client()
        await client.post(
            "/api/generate",
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


@app.on_event("shutdown")
async def shutdown():
    global _client
    if _client:
        await _client.aclose()
        _client = None


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

    async def event_stream():
        client = _get_http_client()
        models_to_try = [req.model]
        if req.model != LLM_MODEL_NAME:
            models_to_try.append(LLM_MODEL_NAME)

        for attempt_model in models_to_try:
            async with client.stream(
                "POST", "/api/generate",
                json={"model": attempt_model, "prompt": augmented_prompt, "stream": True},
            ) as resp:
                if resp.status_code != 200:
                    if attempt_model == models_to_try[-1]:
                        yield f"event: error\ndata: {json.dumps({'error': f'Ollama returned {resp.status_code}'})}\n\n"
                        yield "event: done\ndata: []\n\n"
                    continue
                async for line in resp.aiter_lines():
                    if line.strip():
                        data = json.loads(line)
                        event_type = "done" if data.get("done") else "token"
                        yield f"event: {event_type}\ndata: {json.dumps(data)}\n\n"
                        if data.get("done"):
                            return
                return

    return StreamingResponse(event_stream(), media_type="text/event-stream")


@app.get("/api/models")
async def models():
    client = _get_http_client()
    response = await client.get("/api/tags")
    response.raise_for_status()
    data = response.json()
    return [model["name"] for model in data.get("models", [])]


@app.post("/api/rag/ask")
async def ask(req: AskRequest):
    if not qa_chain:
        raise HTTPException(status_code=503, detail="RAG index not loaded or LLM unavailable")

    async def event_stream():
        sources = []
        async for chunk in qa_chain.astream({"query": req.question}):
            if "result" in chunk:
                yield f"event: token\ndata: {json.dumps({'token': chunk['result']})}\n\n"
            if "source_documents" in chunk:
                sources = chunk["source_documents"]
        if sources:
            yield f"event: sources\ndata: {json.dumps([{'source': d.metadata.get('source', ''), 'content': d.page_content[:500]} for d in sources])}\n\n"
        yield "event: done\ndata: []\n\n"

    return StreamingResponse(event_stream(), media_type="text/event-stream")


@app.get("/health")
async def health():
    return {
        "status": "ok",
        "rag_available": retriever is not None,
    }


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8001)
