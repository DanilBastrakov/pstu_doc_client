from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func

from app.models import Bean, Diagnosis, SymptomDisease
from app.schemas import PersonCreate, PersonUpdate, SymptomDiseaseCreate


async def create_person(db: AsyncSession, person: PersonCreate) -> Bean:
    lifestyle_dict = person.lifestyle.model_dump() if person.lifestyle else None
    past_dict = person.past_conditions.model_dump() if person.past_conditions else None
    db_person = Bean(
        name=person.name,
        age=person.age,
        gender=person.gender,
        symptoms=person.symptoms,
        lifestyle=lifestyle_dict,
        past_conditions=past_dict,
    )
    db.add(db_person)
    await db.commit()
    await db.refresh(db_person)
    return db_person


async def get_person(db: AsyncSession, person_id: int):
    result = await db.execute(
        select(Bean).where(Bean.id == person_id)
    )
    return result.scalar_one_or_none()


async def get_persons(db: AsyncSession, skip: int = 0, limit: int = 100):
    result = await db.execute(select(Bean).offset(skip).limit(limit))
    return result.scalars().all()


async def update_person(db: AsyncSession, person_id: int, updates: PersonUpdate):
    person = await get_person(db, person_id)
    if not person:
        return None
    update_data = updates.model_dump(exclude_unset=True)
    if "lifestyle" in update_data and update_data["lifestyle"] is not None:
        update_data["lifestyle"] = update_data["lifestyle"]
    if "past_conditions" in update_data and update_data["past_conditions"] is not None:
        update_data["past_conditions"] = update_data["past_conditions"]
    for key, value in update_data.items():
        setattr(person, key, value)
    await db.commit()
    await db.refresh(person)
    return person


async def delete_person(db: AsyncSession, person_id: int) -> bool:
    person = await get_person(db, person_id)
    if person:
        await db.delete(person)
        await db.commit()
        return True
    return False


async def get_person_diagnoses(db: AsyncSession, person_id: int):
    result = await db.execute(
        select(Diagnosis).where(Diagnosis.person_id == person_id)
    )
    return result.scalars().all()


async def create_diagnosis(db: AsyncSession, person_id: int, diagnosis_details: str, anamnesis: str = None) -> Diagnosis:
    diagnosis = Diagnosis(
        diagnosis_details=diagnosis_details,
        anamnesis=anamnesis,
        person_id=person_id,
    )
    db.add(diagnosis)
    await db.commit()
    await db.refresh(diagnosis)
    return diagnosis


async def get_all_symptom_diseases(db: AsyncSession):
    result = await db.execute(select(SymptomDisease))
    return result.scalars().all()


async def get_diseases_for_symptoms(db: AsyncSession, symptoms: list[str]):
    if not symptoms:
        return []
    result = await db.execute(
        select(SymptomDisease).where(
            SymptomDisease.symptom.in_(symptoms)
        )
    )
    return result.scalars().all()


async def create_symptom_disease(db: AsyncSession, sd: SymptomDiseaseCreate) -> SymptomDisease:
    db_sd = SymptomDisease(
        symptom=sd.symptom,
        disease=sd.disease,
        correlation_strength=sd.correlation_strength,
        notes=sd.notes,
    )
    db.add(db_sd)
    await db.commit()
    await db.refresh(db_sd)
    return db_sd


async def delete_symptom_disease(db: AsyncSession, sd_id: int) -> bool:
    result = await db.execute(select(SymptomDisease).where(SymptomDisease.id == sd_id))
    sd = result.scalar_one_or_none()
    if sd:
        await db.delete(sd)
        await db.commit()
        return True
    return False
