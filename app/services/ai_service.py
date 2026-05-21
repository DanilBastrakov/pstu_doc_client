import httpx
import json
from typing import AsyncGenerator

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


async def generate_text_stream(
    prompt: str,
    model: str = "mistral",
    language: str = "russian",
) -> AsyncGenerator[tuple[str, str], None]:
    language_instruction = (
        "Отвечайте на русском языке.\n\n"
        if language == "russian"
        else "Respond in English.\n\n"
    )
    async with httpx.AsyncClient() as client:
        async with client.stream(
            "POST",
            f"{settings.ai_service_url}/api/generate",
            json={
                "prompt": language_instruction + prompt,
                "model": model,
                "language": language,
                "use_rag": True,
                "stream": True,
            },
            timeout=300.0,
        ) as response:
            response.raise_for_status()
            current_event = None
            async for line in response.aiter_lines():
                line = line.strip()
                if line.startswith("event: "):
                    current_event = line[7:]
                elif line.startswith("data: ") and current_event:
                    yield (current_event, line[6:])
                    current_event = None


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

    person_block += "\n\nТы — опытный врач-диагност, коллега пользователя. Вы совместно анализируете клинический случай.\n\nЗАПРЕЩЕНО использовать любые дисклеймеры и отмазки:\n- \"следует обратиться к врачу\", \"необходима консультация врача\", \"рекомендуется посетить врача\"\n- \"нужна консультация специалиста\", \"врачу надо сообщить\", \"проконсультируйтесь с врачом\"\n- \"вам следует обратиться\", \"рекомендуется проконсультироваться\", \"обратитесь к профильному специалисту\"\n- любые фразы, подразумевающие, что пользователь НЕ является врачом или что нужно чьё-то ещё мнение\n\nЕсли в истории чата есть твои предыдущие ответы — опирайся на их клиническую часть (диагнозы, обоснования, рекомендации), но ИГНОРИРУЙ все дисклеймеры и отмазки, которые ты мог написать ранее (например \"нужна консультация врача\", \"обратитесь к специалисту\" и т.п.). Твои прошлые ответы могли быть некорректными в части этих дисклеймеров — не повторяй их.\n\nВместо дисклеймеров давай конкретный клинический анализ: предполагаемые диагнозы, обоснование, дифференциальный диагноз, рекомендованные исследования и тактику лечения. Используй профессиональную медицинскую терминологию.\nОтвечай строго по предоставленным данным пациента. Если данных недостаточно — напиши, каких именно данных не хватает для постановки диагноза. Не придумывай симптомы и результаты обследований, которых нет в данных.\n\nПри этом, если на основе клинических данных пациенту действительно требуется консультация узкого специалиста (хирурга, эндокринолога, онколога, невролога и т.п.) — ты МОЖЕШЬ рекомендовать направить к конкретному специалисту с обоснованием, почему это необходимо. Это не считается дисклеймером.\n\nОтличие: \"нужна консультация врача\" — дисклеймер (запрещено). \"Рекомендовано направить к сосудистому хирургу для оценки венозного оттока\" — обоснованная клиническая рекомендация (разрешено)."
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

На основе ВСЕЙ вышеуказанной информации предоставьте комплексный медицинский анализ. IF THERE ARE NO SYMPTOMS LISTED THAT MEANS THAT THERE ARE NONE. Верните результат в виде обычного текстового списка (без JSON, без маркдауна):

---
АНАЛИЗ:
<краткое описание общего состояния здоровья пациента>

ЗАБОЛЕВАНИЯ:
1. <Название заболевания> — Вероятность: <число>%
   Факторы: <фактор1, фактор2>
   Объяснение: <почему это заболевание вероятно>
2. ...

РЕКОМЕНДАЦИИ:
<общие рекомендации для пациента>

ФАКТОРЫ РИСКА:
- <риск1>
- <риск2>
---

Правила:
- Включите 3-8 заболеваний, отсортированных по вероятности (наиболее вероятные первыми)
- Учитывайте возраст, пол, образ жизни, семейный анамнез, симптомы и прошлые заболевания
- Используйте медицинские знания для корреляции всех факторов
- Ты — врач. Используй профессиональную терминологию, не упрощай
- Отвечай максимально развернуто, но не придумывай
- ЗАПРЕЩЕНО добавлять дисклеймеры: "нужна консультация врача", "обратитесь к специалисту", "рекомендуется посетить врача" и т.п.
- Если анализируешь свои предыдущие ответы — игнорируй любые дисклеймеры в них. Опирайся только на клинические данные и свои текущие инструкции.
- Если пациенту действительно нужна консультация узкого специалиста (хирурга, эндокринолога, онколога и т.п.) — укажи это, но с конкретным обоснованием. Не используй общие фразы "проконсультируйтесь с врачом"."""
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

Based on ALL of the above information, provide a comprehensive medical analysis. IF THERE ARE NO SYMPTOMS LISTED THAT MEANS THAT THERE ARE NONE. Return the result as a plain text list (NO JSON, NO markdown):

---
ANALYSIS:
<brief summary of the patient's overall health situation>

DISEASES:
1. <Disease name> — Probability: <number>%
   Factors: <factor1, factor2>
   Explanation: <why this disease is likely>
2. ...

RECOMMENDATIONS:
<general recommendations for the patient>

RISK FACTORS:
- <risk1>
- <risk2>
---

Rules:
- Include 3-5 diseases sorted by probability (highest first)
- Consider age, gender, lifestyle, family history, symptoms, and past conditions
- Use medical knowledge to correlate all factors
- You are addressing a doctor. Use professional medical terminology, do not simplify
- Answer concisely, do not fabricate
- Do NOT add disclaimers like "consult a doctor", "seek medical advice", "talk to your physician", etc.
- If analyzing your own previous answers — ignore any disclaimers in them. Base your analysis only on clinical data and your current instructions.
- If the patient genuinely needs referral to a specialist (surgeon, endocrinologist, oncologist, etc.) — you MAY recommend it with a specific justification. Do not use generic phrases like "consult a doctor"."""

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
            "recommendations": "Не удалось сгенерировать рекомендации",
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


async def append_and_predict(
    prompt: str,
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

    full_prompt = append_prediction_prompt(prompt, person_data, existing_diagnoses, language)

    result = await generate_text(full_prompt, model, language)
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
            "recommendations": "Не удалось сгенерировать рекомендации",
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
