from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.schemas import DiseasePredictionReport
from app.services import ai_service, db_service

router = APIRouter(prefix="/api/ai", tags=["ai"])


@router.post("/generate")
async def generate(prompt: str, model: str = "llama3", language: str = "russian"):
    try:
        result = await ai_service.generate_text(prompt, model, language)
        return {"response": result.get("response", "")}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/models")
async def models():
    try:
        return await ai_service.list_models()
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/predict-diseases/{person_id}", response_model=DiseasePredictionReport)
async def predict_diseases(
    person_id: int,
    model: str = "llama3",
    language: str = "russian",
    db: AsyncSession = Depends(get_db),
):
    person = await db_service.get_person(db, person_id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")

    diagnoses = await db_service.get_person_diagnoses(db, person_id)

    symptoms = person.symptoms or []
    correlations = await db_service.get_diseases_for_symptoms(db, symptoms)

    person_data = {
        "id": person.id,
        "name": person.name,
        "age": person.age,
        "gender": person.gender,
        "symptoms": symptoms,
        "lifestyle": person.lifestyle,
        "past_conditions": person.past_conditions,
    }

    try:
        result = await ai_service.predict_diseases(
            person_data=person_data,
            existing_diagnoses=diagnoses,
            symptom_correlations=correlations,
            model=model,
            language=language,
        )
        return result
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"AI prediction failed: {str(e)}")
