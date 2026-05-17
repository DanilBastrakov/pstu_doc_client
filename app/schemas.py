from pydantic import BaseModel
from datetime import datetime
from typing import Optional, List


class UserCreate(BaseModel):
    username: str
    email: str
    password: str
    full_name: str


class UserLogin(BaseModel):
    username: str
    password: str


class UserResponse(BaseModel):
    id: int
    username: str
    email: str
    full_name: str
    is_active: bool
    created_at: Optional[datetime] = None

    class Config:
        from_attributes = True


class Token(BaseModel):
    access_token: str
    token_type: str = "bearer"


class LifestyleData(BaseModel):
    smoking: Optional[str] = None
    alcohol: Optional[str] = None
    exercise: Optional[str] = None
    diet: Optional[str] = None
    sleep: Optional[str] = None
    stress: Optional[str] = None


class PastConditionsData(BaseModel):
    conditions: Optional[List[str]] = None
    surgeries: Optional[List[str]] = None
    medications: Optional[List[str]] = None
    allergies: Optional[List[str]] = None
    family_history: Optional[List[str]] = None


class PersonBase(BaseModel):
    name: str
    age: Optional[int] = None
    gender: Optional[str] = None
    symptoms: Optional[List[str]] = None
    lifestyle: Optional[LifestyleData] = None
    past_conditions: Optional[PastConditionsData] = None


class PersonCreate(PersonBase):
    pass


class PersonUpdate(BaseModel):
    name: Optional[str] = None
    age: Optional[int] = None
    gender: Optional[str] = None
    symptoms: Optional[List[str]] = None
    lifestyle: Optional[LifestyleData] = None
    past_conditions: Optional[PastConditionsData] = None


class PersonResponse(PersonBase):
    id: int
    created_at: Optional[datetime] = None
    updated_at: Optional[datetime] = None

    class Config:
        from_attributes = True


class DiagnosisBase(BaseModel):
    diagnosis_details: str
    anamnesis: Optional[str] = None
    person_id: int


class DiagnosisCreate(DiagnosisBase):
    pass


class DiagnosisUpdate(BaseModel):
    diagnosis_details: Optional[str] = None
    anamnesis: Optional[str] = None


class DiagnosisResponse(DiagnosisBase):
    id: int
    created_at: Optional[datetime] = None

    class Config:
        from_attributes = True


BeanCreate = PersonCreate
BeanResponse = PersonResponse


class SymptomDiseaseCreate(BaseModel):
    symptom: str
    disease: str
    correlation_strength: Optional[str] = None
    notes: Optional[str] = None


class SymptomDiseaseResponse(BaseModel):
    id: int
    symptom: str
    disease: str
    correlation_strength: Optional[str] = None
    notes: Optional[str] = None
    created_at: Optional[datetime] = None

    class Config:
        from_attributes = True


class ShareRequest(BaseModel):
    username: str


class AppendPromptRequest(BaseModel):
    prompt: str


class ChatMessageCreate(BaseModel):
    role: str
    content: str
    model: Optional[str] = None


class ChatMessageResponse(BaseModel):
    id: int
    person_id: int
    role: str
    content: str
    model: Optional[str] = None
    created_at: Optional[datetime] = None

    class Config:
        from_attributes = True


class DiseasePrediction(BaseModel):
    disease: str
    probability: float
    contributing_factors: List[str]
    explanation: str


class DiseasePredictionReport(BaseModel):
    person_id: int
    person_name: str
    analysis_summary: str
    predictions: List[DiseasePrediction]
    recommendations: str
    risk_factors_identified: List[str]
