from sqlalchemy import Column, Integer, String, Text, DateTime, JSON, ForeignKey
from sqlalchemy.sql import func
from sqlalchemy.orm import relationship

from app.database import Base


class Bean(Base):
    __tablename__ = "beans"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(100), nullable=False)
    age = Column(Integer, nullable=True)
    gender = Column(String(20), nullable=True)
    symptoms = Column(JSON, nullable=True)

    lifestyle = Column(JSON, nullable=True)
    past_conditions = Column(JSON, nullable=True)

    created_at = Column(DateTime(timezone=True), server_default=func.now())
    updated_at = Column(DateTime(timezone=True), server_default=func.now(), onupdate=func.now())

    diagnoses = relationship("Diagnosis", back_populates="person", cascade="all, delete-orphan")


class Diagnosis(Base):
    __tablename__ = "diagnoses"

    id = Column(Integer, primary_key=True, index=True)
    diagnosis_details = Column(Text, nullable=False)
    anamnesis = Column(Text, nullable=True)
    person_id = Column(Integer, ForeignKey("beans.id", ondelete="CASCADE"), nullable=False)

    created_at = Column(DateTime(timezone=True), server_default=func.now())

    person = relationship("Bean", back_populates="diagnoses")


class SymptomDisease(Base):
    __tablename__ = "symptom_diseases"

    id = Column(Integer, primary_key=True, index=True)
    symptom = Column(String(100), nullable=False, index=True)
    disease = Column(String(100), nullable=False, index=True)
    correlation_strength = Column(String(20), nullable=True)
    notes = Column(Text, nullable=True)

    created_at = Column(DateTime(timezone=True), server_default=func.now())
