from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    app_name: str = "PSTU Doc API"
    database_url: str = "postgresql+asyncpg://postgres:postgres@localhost:5432/pstu_doc"
    ollama_base_url: str = "http://localhost:11434"
    ai_service_url: str = "http://localhost:8001"
    jwt_secret_key: str = ""
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 60

    class Config:
        env_file = ".env"
        env_file_encoding = "utf-8"


settings = Settings()
