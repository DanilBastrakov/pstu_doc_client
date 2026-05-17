from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

from app.models import ChatMessage, Bean
from app.schemas import ChatMessageCreate


async def create_message(
    db: AsyncSession,
    person_id: int,
    user_id: int,
    message: ChatMessageCreate,
) -> ChatMessage:
    db_msg = ChatMessage(
        person_id=person_id,
        user_id=user_id,
        role=message.role,
        content=message.content,
        model=message.model,
    )
    db.add(db_msg)
    await db.commit()
    await db.refresh(db_msg)
    return db_msg


async def get_messages(db: AsyncSession, person_id: int, limit: int = 50):
    result = await db.execute(
        select(ChatMessage)
        .where(ChatMessage.person_id == person_id)
        .order_by(ChatMessage.created_at)
        .limit(limit)
    )
    return result.scalars().all()


async def delete_messages(db: AsyncSession, person_id: int):
    result = await db.execute(
        select(ChatMessage).where(ChatMessage.person_id == person_id)
    )
    for msg in result.scalars().all():
        await db.delete(msg)
    await db.commit()
