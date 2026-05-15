from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.schemas import (
    PersonCreate, PersonUpdate, PersonResponse,
    DiagnosisCreate, DiagnosisResponse,
    SymptomDiseaseCreate, SymptomDiseaseResponse,
)
from app.services import db_service

router = APIRouter(prefix="/api/db", tags=["database"])


@router.post("/persons", response_model=PersonResponse)
async def create_person(person: PersonCreate, db: AsyncSession = Depends(get_db)):
    return await db_service.create_person(db, person)


@router.get("/persons", response_model=list[PersonResponse])
async def list_persons(skip: int = 0, limit: int = 100, db: AsyncSession = Depends(get_db)):
    return await db_service.get_persons(db, skip, limit)


@router.get("/persons/{person_id}", response_model=PersonResponse)
async def get_person(person_id: int, db: AsyncSession = Depends(get_db)):
    person = await db_service.get_person(db, person_id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return person


@router.put("/persons/{person_id}", response_model=PersonResponse)
async def update_person(person_id: int, updates: PersonUpdate, db: AsyncSession = Depends(get_db)):
    person = await db_service.update_person(db, person_id, updates)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return person


@router.delete("/persons/{person_id}")
async def delete_person(person_id: int, db: AsyncSession = Depends(get_db)):
    if not await db_service.delete_person(db, person_id):
        raise HTTPException(status_code=404, detail="Person not found")
    return {"message": "Person deleted"}


@router.get("/persons/{person_id}/diagnoses", response_model=list[DiagnosisResponse])
async def get_person_diagnoses(person_id: int, db: AsyncSession = Depends(get_db)):
    person = await db_service.get_person(db, person_id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return await db_service.get_person_diagnoses(db, person_id)


@router.post("/persons/{person_id}/diagnoses", response_model=DiagnosisResponse)
async def create_diagnosis(
    person_id: int,
    diagnosis: DiagnosisCreate,
    db: AsyncSession = Depends(get_db),
):
    person = await db_service.get_person(db, person_id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")
    return await db_service.create_diagnosis(db, person_id, diagnosis.diagnosis_details, diagnosis.anamnesis)


@router.post("/symptom-diseases", response_model=SymptomDiseaseResponse)
async def create_symptom_disease(sd: SymptomDiseaseCreate, db: AsyncSession = Depends(get_db)):
    return await db_service.create_symptom_disease(db, sd)


@router.get("/symptom-diseases", response_model=list[SymptomDiseaseResponse])
async def list_symptom_diseases(db: AsyncSession = Depends(get_db)):
    return await db_service.get_all_symptom_diseases(db)


@router.delete("/symptom-diseases/{sd_id}")
async def delete_symptom_disease(sd_id: int, db: AsyncSession = Depends(get_db)):
    if not await db_service.delete_symptom_disease(db, sd_id):
        raise HTTPException(status_code=404, detail="Correlation not found")
    return {"message": "Correlation deleted"}
