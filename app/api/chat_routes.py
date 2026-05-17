from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List

from app.auth import get_current_user
from app.database import get_db
from app.models import User
from app.schemas import ChatMessageCreate, ChatMessageResponse
from app.services import chat_service, db_service

router = APIRouter(prefix="/api/chat", tags=["chat"])


@router.get("/{person_id}", response_model=List[ChatMessageResponse])
async def get_chat(
    person_id: int,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")

    return await chat_service.get_messages(db, person_id)


@router.post("/{person_id}", response_model=ChatMessageResponse)
async def create_chat_message(
    person_id: int,
    message: ChatMessageCreate,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")

    return await chat_service.create_message(db, person_id, current_user.id, message)


@router.delete("/{person_id}")
async def delete_chat(
    person_id: int,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
    person = await db_service.get_person_for_user(db, person_id, current_user.id)
    if not person:
        raise HTTPException(status_code=404, detail="Person not found")

    await chat_service.delete_messages(db, person_id)
    return {"message": "Chat history deleted"}
