from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.config import settings
from app.database import init_db
from app.api import database_routes, ai_routes

app = FastAPI(title=settings.app_name)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(database_routes.router)
app.include_router(ai_routes.router)


@app.on_event("startup")
async def startup():
    await init_db()


@app.get("/")
def home():
    return {"message": "PSTU Doc API is running", "docs": "/docs"}
