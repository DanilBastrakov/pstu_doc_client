from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.auth import get_current_user
from app.database import get_db
from app.models import User
from app.schemas import AppendPromptRequest, DiseasePredictionReport
from app.services import ai_service, chat_service, db_service

router = APIRouter(prefix="/api/ai", tags=["ai"])


@router.post("/generate")
async def generate(prompt: str, model: str = "mistral", language: str = "russian", _: User = Depends(get_current_user)):
    try:
        result = await ai_service.generate_text(prompt, model, language)
        return {"response": result.get("response", "")}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/models")
async def models(_: User = Depends(get_current_user)):
    try:
        return await ai_service.list_models()
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/append_prompt/{person_id}")
async def append_prompt(
    person_id: int,
    body: AppendPromptRequest,
    include_history: bool = False,
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

    prompt = body.prompt
    if include_history:
        messages = await chat_service.get_messages(db, person_id, limit=10)
        history = ai_service.format_chat_history(messages)
        if history:
            prompt = history + "\n\n" + prompt

    result = ai_service.append_person_data(
        prompt=prompt,
        person_data=person_data,
        existing_diagnoses=diagnoses,
    )

    return {"prompt": result}


@router.post("/append_prediction/{person_id}")
async def append_prediction(
    person_id: int,
    body: AppendPromptRequest,
    language: str = "russian",
    include_history: bool = False,
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

    prompt = body.prompt
    if include_history:
        messages = await chat_service.get_messages(db, person_id, limit=10)
        history = ai_service.format_chat_history(messages)
        if history:
            prompt = history + "\n\n" + prompt

    result = ai_service.append_prediction_prompt(
        prompt=prompt,
        person_data=person_data,
        existing_diagnoses=diagnoses,
        language=language,
    )

    return {"prompt": result}


@router.post("/predict-diseases/{person_id}", response_model=DiseasePredictionReport)
async def predict_diseases(
    person_id: int,
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
        result = await ai_service.predict_diseases(
            person_data=person_data,
            existing_diagnoses=diagnoses,
            model=model,
            language=language,
        )
        return result
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI prediction failed: {str(e)}")
