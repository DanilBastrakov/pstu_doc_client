from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.auth import get_current_user
from app.database import get_db
from app.models import User
from app.schemas import (
    PersonCreate, PersonUpdate, PersonResponse,
    DiagnosisCreate, DiagnosisResponse,
    SymptomDiseaseCreate, SymptomDiseaseResponse,
    ShareRequest,
)
from app.services import db_service

router = APIRouter(prefix="/api/db", tags=["database"])


@router.post("/persons", response_model=PersonResponse)
async def create_person(person: PersonCreate, db: AsyncSession = Depends(get_db), current_user: User = Depends(get_current_user)):
    return await db_service.create_person(db, person, current_user.id)


@router.get("/persons", response_model=list[PersonResponse])
async def list_persons(skip: int = 0, limit: int = 100, db: AsyncSession = Depends(get_db), current_user: User = Depends(get_current_user)):
    return await db_service.get_persons(db, current_user.id, skip, limit)


@router.get("/persons/{person_id}", response_model=PersonResponse)
async def get_person(person_id: int, db: AsyncSession = Depends(get_db), current_user: User = Depends(get_current_user)):
    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return person


@router.put("/persons/{person_id}", response_model=PersonResponse)
async def update_person(person_id: int, updates: PersonUpdate, db: AsyncSession = Depends(get_db), current_user: User = Depends(get_current_user)):
    person = await db_service.update_person(db, person_id, updates, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return person


@router.delete("/persons/{person_id}")
async def delete_person(person_id: int, db: AsyncSession = Depends(get_db), current_user: User = Depends(get_current_user)):
    if not await db_service.delete_person(db, person_id, current_user.id):
        raise HTTPException(status_code=404, detail="Person not found")
    return {"message": "Person deleted"}


@router.get("/persons/{person_id}/diagnoses", response_model=list[DiagnosisResponse])
async def get_person_diagnoses(person_id: int, db: AsyncSession = Depends(get_db), current_user: User = Depends(get_current_user)):
    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return await db_service.get_person_diagnoses(db, person_id)


@router.post("/persons/{person_id}/diagnoses", response_model=DiagnosisResponse)
async def create_diagnosis(
    person_id: int,
    diagnosis: DiagnosisCreate,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return await db_service.create_diagnosis(db, person_id, diagnosis.diagnosis_details, diagnosis.anamnesis)


@router.post("/persons/{person_id}/share")
async def share_person(
    person_id: int,
    body: ShareRequest,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    if not await db_service.share_person(db, person_id, current_user.id, body.username):
        raise HTTPException(status_code=404, detail="Person or target user not found")
    return {"message": "Person shared"}


@router.post("/symptom-diseases", response_model=SymptomDiseaseResponse)
async def create_symptom_disease(sd: SymptomDiseaseCreate, db: AsyncSession = Depends(get_db), _: User = Depends(get_current_user)):
    return await db_service.create_symptom_disease(db, sd)


@router.get("/symptom-diseases", response_model=list[SymptomDiseaseResponse])
async def list_symptom_diseases(db: AsyncSession = Depends(get_db), _: User = Depends(get_current_user)):
    return await db_service.get_all_symptom_diseases(db)


@router.delete("/symptom-diseases/{sd_id}")
async def delete_symptom_disease(sd_id: int, db: AsyncSession = Depends(get_db), _: User = Depends(get_current_user)):
    if not await db_service.delete_symptom_disease(db, sd_id):
        raise HTTPException(status_code=404, detail="Correlation not found")
    return {"message": "Correlation deleted"}
