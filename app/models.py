from sqlalchemy import Column, Integer, String, Text, DateTime, JSON, ForeignKey, Boolean, Table
from sqlalchemy.sql import func
from sqlalchemy.orm import relationship

from app.database import Base


person_doctors = Table(
    "person_doctors",
    Base.metadata,
    Column("person_id", Integer, ForeignKey("beans.id", ondelete="CASCADE"), primary_key=True),
    Column("user_id", Integer, ForeignKey("users.id", ondelete="CASCADE"), primary_key=True),
)


class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, index=True)
    username = Column(String(50), unique=True, nullable=False, index=True)
    email = Column(String(100), unique=True, nullable=False, index=True)
    hashed_password = Column(String(255), nullable=False)
    full_name = Column(String(100), nullable=False)
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    patients = relationship("Bean", secondary=person_doctors, back_populates="doctors")


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
    doctors = relationship("User", secondary=person_doctors, back_populates="patients")
    messages = relationship("ChatMessage", back_populates="person", cascade="all, delete-orphan", order_by="ChatMessage.created_at")


class Diagnosis(Base):
    __tablename__ = "diagnoses"

    id = Column(Integer, primary_key=True, index=True)
    diagnosis_details = Column(Text, nullable=False)
    anamnesis = Column(Text, nullable=True)
    person_id = Column(Integer, ForeignKey("beans.id", ondelete="CASCADE"), nullable=False)

    created_at = Column(DateTime(timezone=True), server_default=func.now())

    person = relationship("Bean", back_populates="diagnoses")


class ChatMessage(Base):
    __tablename__ = "chat_messages"

    id = Column(Integer, primary_key=True, index=True)
    person_id = Column(Integer, ForeignKey("beans.id", ondelete="CASCADE"), nullable=False, index=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    role = Column(String(20), nullable=False)
    content = Column(Text, nullable=False)
    model = Column(String(50), nullable=True)
    created_at = Column(DateTime(timezone=True), server_default=func.now())

    person = relationship("Bean", back_populates="messages")


class SymptomDisease(Base):
    __tablename__ = "symptom_diseases"

    id = Column(Integer, primary_key=True, index=True)
    symptom = Column(String(100), nullable=False, index=True)
    disease = Column(String(100), nullable=False, index=True)
    correlation_strength = Column(String(20), nullable=True)
    notes = Column(Text, nullable=True)

    created_at = Column(DateTime(timezone=True), server_default=func.now())
