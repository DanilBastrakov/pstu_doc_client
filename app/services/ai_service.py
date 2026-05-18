import httpx
import json

from app.config import settings


async def generate_text(prompt: str, model: str = "mistral", language: str = "russian") -> dict:
    language_instruction = (
        "Отвечайте на русском языке.\n\n"
        if language == "russian"
        else "Respond in English.\n\n"
    )
    async with httpx.AsyncClient() as client:
        response = await client.post(
            f"{settings.ai_service_url}/api/generate",
            json={
                "prompt": language_instruction + prompt,
                "model": model,
                "language": language,
                "use_rag": True,
            },
            timeout=300.0,
        )
        response.raise_for_status()
        return response.json()


async def list_models() -> list[str]:
    async with httpx.AsyncClient() as client:
        response = await client.get(f"{settings.ai_service_url}/api/models")
        response.raise_for_status()
        return response.json()


def format_chat_history(messages: list) -> str:
    if not messages:
        return ""
    lines = ["===== CHAT HISTORY ====="]
    for m in messages:
        prefix = "User" if m.role == "user" else "Assistant"
        lines.append(f"{prefix}: {m.content}")
    lines.append("========================")
    return "\n\n".join(lines)


def append_prediction_prompt(prompt: str, person_data: dict, existing_diagnoses: list, language: str = "russian") -> str:
    return prompt + "\n\n" + build_disease_prompt(person_data, existing_diagnoses, language)


def append_person_data(prompt: str, person_data: dict, existing_diagnoses: list) -> str:
    symptoms = person_data.get("symptoms") or []
    if not isinstance(symptoms, list):
        symptoms = []
    lifestyle = person_data.get("lifestyle") or {}
    if not isinstance(lifestyle, dict):
        lifestyle = {}
    past_conditions = person_data.get("past_conditions") or {}
    if not isinstance(past_conditions, dict):
        past_conditions = {}
    past_conditions_list = past_conditions.get("conditions") or []
    family_history = past_conditions.get("family_history") or []

    diagnoses_text = ""
    if existing_diagnoses:
        diagnoses_text = "Diagnoses:\n"
        for d in existing_diagnoses:
            diagnoses_text += f"- {d.diagnosis_details}"
            if d.anamnesis:
                diagnoses_text += f" (anamnesis: {d.anamnesis})"
            diagnoses_text += "\n"

    person_block = f"""
===== PERSON DATA =====
Name: {person_data.get('name', 'Unknown')}
Age: {person_data.get('age', 'unknown')}
Gender: {person_data.get('gender', 'unknown')}
Symptoms: {', '.join(symptoms) if symptoms else 'none'}
Lifestyle:
  - Smoking: {lifestyle.get('smoking', 'unknown')}
  - Alcohol: {lifestyle.get('alcohol', 'unknown')}
  - Exercise: {lifestyle.get('exercise', 'unknown')}
  - Diet: {lifestyle.get('diet', 'unknown')}
  - Sleep: {lifestyle.get('sleep', 'unknown')}
  - Stress: {lifestyle.get('stress', 'unknown')}
Past Conditions:
  - Previous conditions: {', '.join(past_conditions_list) if past_conditions_list else 'none'}
  - Surgeries: {', '.join(past_conditions.get('surgeries') or []) if past_conditions.get('surgeries') else 'none'}
  - Medications: {', '.join(past_conditions.get('medications') or []) if past_conditions.get('medications') else 'none'}
  - Allergies: {', '.join(past_conditions.get('allergies') or []) if past_conditions.get('allergies') else 'none'}
  - Family history: {', '.join(family_history) if family_history else 'none'}
{diagnoses_text}
======================"""

    person_block += "\n\nТы общаешься с врачом. НЕ НАПРАВЛЯЙ ПОЛЬЗОВАТЕЛЯ К ВРАЧУ. Используй профессиональную медицинскую терминологию.\nОтвечай развернуто, твой ответ должен содержать ТОЛЬКО ОТВЕТ ДЛЯ ВРАЧА, без технического текста, но не придумывай."
    return prompt + person_block


def build_disease_prompt(
    person_data: dict,
    existing_diagnoses: list,
    language: str = "russian",
) -> str:
    symptoms = person_data.get("symptoms") or []
    lifestyle = person_data.get("lifestyle") or {}
    past_conditions = person_data.get("past_conditions") or {}

    past_conditions_list = past_conditions.get("conditions") or []
    family_history = past_conditions.get("family_history") or []

    existing_diagnoses_text = ""
    if existing_diagnoses:
        existing_diagnoses_text = "Имеющиеся диагнозы:\n"
        for d in existing_diagnoses:
            existing_diagnoses_text += f"- {d.diagnosis_details}"
            if d.anamnesis:
                existing_diagnoses_text += f" (анамнез: {d.anamnesis})"
            existing_diagnoses_text += "\n"

    if language == "russian":
        prompt = f"""Вы — медицинский диагностический ассистент. Проанализируйте данные пациента и предскажите наиболее вероятные заболевания.

ИНФОРМАЦИЯ О ПАЦИЕНТЕ:
- Имя: {person_data.get('name', 'Неизвестно')}
- Возраст: {person_data.get('age', 'неизвестен')}
- Пол: {person_data.get('gender', 'неизвестен')}
- Симптомы: {', '.join(symptoms) if symptoms else 'не указаны'}

ФАКТОРЫ ОБРАЗА ЖИЗНИ:
- Курение: {lifestyle.get('smoking', 'неизвестно')}
- Алкоголь: {lifestyle.get('alcohol', 'неизвестно')}
- Физическая активность: {lifestyle.get('exercise', 'неизвестно')}
- Питание: {lifestyle.get('diet', 'неизвестно')}
- Сон: {lifestyle.get('sleep', 'неизвестно')}
- Стресс: {lifestyle.get('stress', 'неизвестно')}

ПРОШЛЫЕ ЗАБОЛЕВАНИЯ:
- Предыдущие состояния: {', '.join(past_conditions_list) if past_conditions_list else 'отсутствуют'}
- Хирургические вмешательства: {', '.join(past_conditions.get('surgeries') or []) if past_conditions.get('surgeries') else 'отсутствуют'}
- Лекарственные препараты: {', '.join(past_conditions.get('medications') or []) if past_conditions.get('medications') else 'отсутствуют'}
- Аллергии: {', '.join(past_conditions.get('allergies') or []) if past_conditions.get('allergies') else 'отсутствуют'}
- Семейный анамнез: {', '.join(family_history) if family_history else 'отсутствует'}

{existing_diagnoses_text}

На основе ВСЕЙ вышеуказанной информации предоставьте комплексный медицинский анализ. Верните ТОЛЬКО корректный JSON-объект с точной структурой:

{{
  "analysis_summary": "Краткое описание общего состояния здоровья пациента",
  "predictions": [
    {{
      "disease": "Название заболевания",
      "probability": 0.85,
      "contributing_factors": ["фактор1", "фактор2"],
      "explanation": "Почему это заболевание вероятно"
    }}
  ],
  "recommendations": "Общие рекомендации для пациента",
  "risk_factors_identified": ["риск1", "риск2"]
}}

Правила:
- Включите 3-8 заболеваний, отсортированных по вероятности (наиболее вероятные первыми)
- Вероятность должна быть от 0.0 до 1.0
- Учитывайте возраст, пол, образ жизни, семейный анамнез, симптомы и прошлые заболевания
- Используйте медицинские знания для корреляции всех факторов
- Верните ТОЛЬКО корректный JSON, без разметки, без дополнительного текста
- Ты обращаешься к врачу. Используй профессиональную терминологию, не упрощай
- Отвечай максимально развернуто, но не придумывай"""
    else:
        prompt = f"""You are a medical diagnostic assistant. Analyze the patient data and predict the most likely diseases.

PATIENT INFORMATION:
- Name: {person_data.get('name', 'Unknown')}
- Age: {person_data.get('age', 'unknown')}
- Gender: {person_data.get('gender', 'unknown')}
- Symptoms: {', '.join(symptoms) if symptoms else 'none reported'}

LIFESTYLE FACTORS:
- Smoking: {lifestyle.get('smoking', 'unknown')}
- Alcohol: {lifestyle.get('alcohol', 'unknown')}
- Exercise: {lifestyle.get('exercise', 'unknown')}
- Diet: {lifestyle.get('diet', 'unknown')}
- Sleep: {lifestyle.get('sleep', 'unknown')}
- Stress: {lifestyle.get('stress', 'unknown')}

PAST CONDITIONS:
- Previous conditions: {', '.join(past_conditions_list) if past_conditions_list else 'none'}
- Surgeries: {', '.join(past_conditions.get('surgeries') or []) if past_conditions.get('surgeries') else 'none'}
- Medications: {', '.join(past_conditions.get('medications') or []) if past_conditions.get('medications') else 'none'}
- Allergies: {', '.join(past_conditions.get('allergies') or []) if past_conditions.get('allergies') else 'none'}
- Family history: {', '.join(family_history) if family_history else 'none'}

{existing_diagnoses_text}

Based on ALL of the above information, provide a comprehensive medical analysis. Return ONLY a valid JSON object with this exact structure:

{{
  "analysis_summary": "Brief summary of the patient's overall health situation",
  "predictions": [
    {{
      "disease": "Disease name",
      "probability": 0.85,
      "contributing_factors": ["factor1", "factor2"],
      "explanation": "Why this disease is likely"
    }}
  ],
  "recommendations": "General recommendations for the patient",
  "risk_factors_identified": ["risk1", "risk2"]
}}

Rules:
- Include 3-8 diseases sorted by probability (highest first)
- Probability should be between 0.0 and 1.0
- Consider age, gender, lifestyle, family history, symptoms, and past conditions
- Use medical knowledge to correlate all factors
- Return ONLY valid JSON, no markdown, no extra text
- You are addressing a doctor. Use professional medical terminology, do not simplify
- Answer concisely, do not fabricate"""

    return prompt


async def predict_diseases(
    person_data: dict,
    existing_diagnoses: list,
    model: str = "mistral",
    language: str = "russian",
) -> dict:
    symptoms = person_data.get("symptoms") or []
    if not isinstance(symptoms, list):
        symptoms = []
    lifestyle = person_data.get("lifestyle") or {}
    if not isinstance(lifestyle, dict):
        lifestyle = {}
    past_conditions = person_data.get("past_conditions") or {}
    if not isinstance(past_conditions, dict):
        past_conditions = {}

    risk_factors = []
    if lifestyle.get("smoking") and lifestyle.get("smoking") != "no":
        risk_factors.append(f"smoking: {lifestyle['smoking']}")
    if lifestyle.get("alcohol") and lifestyle.get("alcohol") != "no":
        risk_factors.append(f"alcohol: {lifestyle['alcohol']}")
    if lifestyle.get("stress") and lifestyle.get("stress") != "low":
        risk_factors.append(f"stress level: {lifestyle['stress']}")
    if lifestyle.get("exercise") and lifestyle.get("exercise") in ("none", "low"):
        risk_factors.append(f"low physical activity: {lifestyle['exercise']}")
    if lifestyle.get("diet") and lifestyle.get("diet") in ("poor", "unhealthy"):
        risk_factors.append(f"poor diet: {lifestyle['diet']}")
    if lifestyle.get("sleep") and lifestyle.get("sleep") in ("poor", "insufficient"):
        risk_factors.append(f"sleep issues: {lifestyle['sleep']}")

    past_conditions_list = past_conditions.get("conditions") or []
    family_history = past_conditions.get("family_history") or []
    if family_history:
        risk_factors.append(f"family history: {', '.join(family_history)}")
    if past_conditions_list:
        risk_factors.append(f"past conditions: {', '.join(past_conditions_list)}")

    prompt = build_disease_prompt(person_data, existing_diagnoses, language)

    result = await generate_text(prompt, model, language)
    response_text = result.get("response", "")

    try:
        start = response_text.find("{")
        end = response_text.rfind("}") + 1
        if start >= 0 and end > start:
            json_str = response_text[start:end]
            parsed = json.loads(json_str)
        else:
            parsed = json.loads(response_text)
    except (json.JSONDecodeError, ValueError):
        parsed = {
            "analysis_summary": "Failed to parse AI response",
            "predictions": [],
            "recommendations": "Consult with a medical professional",
            "risk_factors_identified": risk_factors,
        }

    return {
        "person_id": person_data.get("id"),
        "person_name": person_data.get("name", "Unknown"),
        "analysis_summary": parsed.get("analysis_summary", ""),
        "predictions": parsed.get("predictions", []),
        "recommendations": parsed.get("recommendations", ""),
        "risk_factors_identified": parsed.get("risk_factors_identified", risk_factors),
    }
