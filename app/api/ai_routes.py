from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from starlette.responses import StreamingResponse

from app.auth import get_current_user
from app.database import get_db
from app.models import User
from app.schemas import AppendPromptRequest
from app.services import ai_service, chat_service, db_service

router = APIRouter(prefix="/api/ai", tags=["ai"])


@router.post("/append_and_generate/stream/{person_id}")
async def append_and_generate_stream(
    person_id: int,
    body: AppendPromptRequest,
    model: str = "mistral",
    language: str = "russian",
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):

    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")

    diagnoses = await db_service.get_person_diagnoses(db, person_id)

    person_data = {
        "id": person.id,
        "name": person.name,
        "age": person.age,
        "gender": person.gender,
        "symptoms": person.symptoms or [],
        "lifestyle": person.lifestyle,
        "past_conditions": person.past_conditions,
    }

    try:
        prompt = body.prompt
        if body.include_history:
            messages = await chat_service.get_messages(db, person_id, limit=10)
            history = ai_service.format_chat_history(messages)
            if history:
                prompt = history + "\n\n" + prompt

        final_prompt = ai_service.append_person_data(
            prompt=prompt,
            person_data=person_data,
            existing_diagnoses=diagnoses,
        )

        async def _format_sse():
            async for event_type, data in ai_service.generate_text_stream(final_prompt, model, language):
                yield f"event: {event_type}\ndata: {data}\n\n"
            yield "event: done\ndata: [DONE]\n\n"

        return StreamingResponse(
            _format_sse(),
            media_type="text/event-stream",
            headers={
                "Cache-Control": "no-cache",
                "Connection": "keep-alive",
                "X-Accel-Buffering": "no",
            },
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/append_and_generate_prediction/stream/{person_id}")
async def append_and_generate_prediction_stream(
    person_id: int,
    body: AppendPromptRequest,
    model: str = "mistral",
    language: str = "russian",
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):

    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")

    diagnoses = await db_service.get_person_diagnoses(db, person_id)

    person_data = {
        "id": person.id,
        "name": person.name,
        "age": person.age,
        "gender": person.gender,
        "symptoms": person.symptoms or [],
        "lifestyle": person.lifestyle,
        "past_conditions": person.past_conditions,
    }

    try:
        prompt = body.prompt
        if body.include_history:
            messages = await chat_service.get_messages(db, person_id, limit=10)
            history = ai_service.format_chat_history(messages)
            if history:
                prompt = history + "\n\n" + prompt

        final_prompt = ai_service.append_prediction_prompt(
            prompt=prompt,
            person_data=person_data,
            existing_diagnoses=diagnoses,
            language=language,
        )

        async def _format_sse():
            async for event_type, data in ai_service.generate_text_stream(final_prompt, model, language):
                yield f"event: {event_type}\ndata: {data}\n\n"
            yield "event: done\ndata: [DONE]\n\n"

        return StreamingResponse(
            _format_sse(),
            media_type="text/event-stream",
            headers={
                "Cache-Control": "no-cache",
                "Connection": "keep-alive",
                "X-Accel-Buffering": "no",
            },
        )
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/generate/stream")
async def generate_stream(
    prompt: str,
    model: str = "mistral",
    language: str = "russian",
    _: User = Depends(get_current_user),
):
    async def _format_sse():
        async for event_type, data in ai_service.generate_text_stream(prompt, model, language):
            yield f"event: {event_type}\ndata: {data}\n\n"
        yield "event: done\ndata: [DONE]\n\n"

    return StreamingResponse(
        _format_sse(),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no",
        },
    )
