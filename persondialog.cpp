#include "persondialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QScrollArea>
#include <QMessageBox>

// временный хардкод, т.к. денег на VPS нету...
static const QString BASE_URL = "http://localhost:8000";

PersonDialog::PersonDialog(const QString &token, QWidget *parent)
    : QDialog(parent), m_token(token), m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle(QString::fromUtf8("Добавить пациента"));
    resize(500, 600);
    setupUi();
}

PersonDialog::PersonDialog(const QString &token, int personId, const QJsonObject &personData, QWidget *parent)
    : QDialog(parent), m_token(token), m_nam(new QNetworkAccessManager(this)),
      m_editMode(true), m_editPersonId(personId)
{
    setWindowTitle(QString::fromUtf8("Редактировать пациента"));
    resize(500, 600);
    setupUi();
    populateFields(personData);
}

void PersonDialog::setupUi() {
    auto *layout = new QVBoxLayout(this);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    auto *formWidget = new QWidget();
    auto *formLayout = new QVBoxLayout(formWidget);

    // Basic info
    auto *basicGroup = new QGroupBox(QString::fromUtf8("Основная информация"));
    auto *basicForm = new QFormLayout(basicGroup);

    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(QString::fromUtf8("Полное имя"));
    basicForm->addRow(QString::fromUtf8("Имя:"), m_nameEdit);

    m_ageSpin = new QSpinBox();
    m_ageSpin->setRange(0, 150);
    m_ageSpin->setValue(30);
    basicForm->addRow(QString::fromUtf8("Возраст:"), m_ageSpin);

    m_genderCombo = new QComboBox();
    m_genderCombo->addItems({
        QString::fromUtf8("мужской"),
        QString::fromUtf8("женский")
    });
    basicForm->addRow(QString::fromUtf8("Пол:"), m_genderCombo);

    formLayout->addWidget(basicGroup);

    // Symptoms
    auto *symptomsGroup = new QGroupBox(QString::fromUtf8("Симптомы"));
    auto *symptomsLayout = new QVBoxLayout(symptomsGroup);

    m_symptomsList = new QListWidget();
    symptomsLayout->addWidget(m_symptomsList);

    auto *symptomRow = new QHBoxLayout();
    m_symptomInput = new QLineEdit();
    m_symptomInput->setPlaceholderText(QString::fromUtf8("Добавить симптом..."));
    symptomRow->addWidget(m_symptomInput);
    auto *addSymptomBtn = new QPushButton(QString::fromUtf8("Добавить"));
    symptomRow->addWidget(addSymptomBtn);
    auto *removeSymptomBtn = new QPushButton(QString::fromUtf8("Удалить"));
    symptomRow->addWidget(removeSymptomBtn);
    symptomsLayout->addLayout(symptomRow);

    connect(addSymptomBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_symptomInput->text().trimmed();
        if (!text.isEmpty()) {
            m_symptomsList->addItem(text);
            m_symptomInput->clear();
        }
    });
    connect(removeSymptomBtn, &QPushButton::clicked, this, [this]() {
        delete m_symptomsList->currentItem();
    });

    formLayout->addWidget(symptomsGroup);

    // Lifestyle
    auto *lifestyleGroup = new QGroupBox(QString::fromUtf8("Образ жизни"));
    auto *lifestyleForm = new QFormLayout(lifestyleGroup);

    m_smokingCombo = new QComboBox();
    m_smokingCombo->addItems({QString::fromUtf8("да"), QString::fromUtf8("нет"), QString::fromUtf8("иногда")});
    lifestyleForm->addRow(QString::fromUtf8("Курение:"), m_smokingCombo);

    m_alcoholCombo = new QComboBox();
    m_alcoholCombo->addItems({QString::fromUtf8("да"), QString::fromUtf8("нет"), QString::fromUtf8("иногда")});
    lifestyleForm->addRow(QString::fromUtf8("Алкоголь:"), m_alcoholCombo);

    m_exerciseCombo = new QComboBox();
    m_exerciseCombo->addItems({
        QString::fromUtf8("легкая"),
        QString::fromUtf8("умеренная"),
        QString::fromUtf8("интенсивная")
    });
    lifestyleForm->addRow(QString::fromUtf8("Активность:"), m_exerciseCombo);

    formLayout->addWidget(lifestyleGroup);

    // Past conditions
    auto *conditionsGroup = new QGroupBox(QString::fromUtf8("История болезней"));
    auto *conditionsLayout = new QVBoxLayout(conditionsGroup);

    auto *condLabel = new QLabel(QString::fromUtf8("Заболевания:"));
    conditionsLayout->addWidget(condLabel);
    m_conditionsList = new QListWidget();
    conditionsLayout->addWidget(m_conditionsList);

    auto *condRow = new QHBoxLayout();
    m_conditionInput = new QLineEdit();
    m_conditionInput->setPlaceholderText(QString::fromUtf8("Добавить заболевание..."));
    condRow->addWidget(m_conditionInput);
    auto *addCondBtn = new QPushButton(QString::fromUtf8("Добавить"));
    condRow->addWidget(addCondBtn);
    auto *removeCondBtn = new QPushButton(QString::fromUtf8("Удалить"));
    condRow->addWidget(removeCondBtn);
    conditionsLayout->addLayout(condRow);

    connect(addCondBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_conditionInput->text().trimmed();
        if (!text.isEmpty()) {
            m_conditionsList->addItem(text);
            m_conditionInput->clear();
        }
    });
    connect(removeCondBtn, &QPushButton::clicked, this, [this]() {
        delete m_conditionsList->currentItem();
    });

    auto *medLabel = new QLabel(QString::fromUtf8("Лекарства:"));
    conditionsLayout->addWidget(medLabel);
    m_medicationsList = new QListWidget();
    conditionsLayout->addWidget(m_medicationsList);

    auto *medRow = new QHBoxLayout();
    m_medicationInput = new QLineEdit();
    m_medicationInput->setPlaceholderText(QString::fromUtf8("Добавить лекарство..."));
    medRow->addWidget(m_medicationInput);
    auto *addMedBtn = new QPushButton(QString::fromUtf8("Добавить"));
    medRow->addWidget(addMedBtn);
    auto *removeMedBtn = new QPushButton(QString::fromUtf8("Удалить"));
    medRow->addWidget(removeMedBtn);
    conditionsLayout->addLayout(medRow);

    connect(addMedBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_medicationInput->text().trimmed();
        if (!text.isEmpty()) {
            m_medicationsList->addItem(text);
            m_medicationInput->clear();
        }
    });
    connect(removeMedBtn, &QPushButton::clicked, this, [this]() {
        delete m_medicationsList->currentItem();
    });

    formLayout->addWidget(conditionsGroup);

    scrollArea->setWidget(formWidget);
    layout->addWidget(scrollArea, 1);

    m_statusLabel = new QLabel();
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *saveBtn = new QPushButton(QString::fromUtf8("Сохранить"));
    layout->addWidget(saveBtn);
    connect(saveBtn, &QPushButton::clicked, this, &PersonDialog::onSaveClicked);

    auto *cancelBtn = new QPushButton(QString::fromUtf8("Отмена"));
    layout->addWidget(cancelBtn);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void PersonDialog::populateFields(const QJsonObject &obj) {
    m_nameEdit->setText(obj.value("name").toString());
    m_ageSpin->setValue(obj.value("age").toInt());

    QString gender = obj.value("gender").toString();
    int genderIdx = m_genderCombo->findText(gender);
    if (genderIdx >= 0) m_genderCombo->setCurrentIndex(genderIdx);

    QJsonArray symptoms = obj.value("symptoms").toArray();
    for (const auto &s : symptoms)
        m_symptomsList->addItem(s.toString());

    QJsonObject lifestyle = obj.value("lifestyle").toObject();
    QString smoking = lifestyle.value("smoking").toString();
    int smokingIdx = m_smokingCombo->findText(smoking);
    if (smokingIdx >= 0) m_smokingCombo->setCurrentIndex(smokingIdx);

    QString alcohol = lifestyle.value("alcohol").toString();
    int alcoholIdx = m_alcoholCombo->findText(alcohol);
    if (alcoholIdx >= 0) m_alcoholCombo->setCurrentIndex(alcoholIdx);

    QString exercise = lifestyle.value("exercise").toString();
    int exerciseIdx = m_exerciseCombo->findText(exercise);
    if (exerciseIdx >= 0) m_exerciseCombo->setCurrentIndex(exerciseIdx);

    QJsonObject pastConditions = obj.value("past_conditions").toObject();
    QJsonArray conditions = pastConditions.value("conditions").toArray();
    for (const auto &c : conditions)
        m_conditionsList->addItem(c.toString());

    QJsonArray medications = pastConditions.value("medications").toArray();
    for (const auto &m : medications)
        m_medicationsList->addItem(m.toString());
}

void PersonDialog::onSaveClicked() {
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("Имя обязательно"));
        m_statusLabel->setStyleSheet("color: red;");
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("Сохранение..."));
    m_statusLabel->setStyleSheet("color: gray;");

    QJsonObject body;
    body["name"] = name;
    body["age"] = m_ageSpin->value();
    body["gender"] = m_genderCombo->currentText();

    QJsonArray symptoms;
    for (int i = 0; i < m_symptomsList->count(); ++i)
        symptoms.append(m_symptomsList->item(i)->text());
    body["symptoms"] = symptoms;

    QJsonObject lifestyle;
    lifestyle["smoking"] = m_smokingCombo->currentText();
    lifestyle["alcohol"] = m_alcoholCombo->currentText();
    lifestyle["exercise"] = m_exerciseCombo->currentText();
    body["lifestyle"] = lifestyle;

    QJsonArray conditions;
    for (int i = 0; i < m_conditionsList->count(); ++i)
        conditions.append(m_conditionsList->item(i)->text());
    QJsonArray medications;
    for (int i = 0; i < m_medicationsList->count(); ++i)
        medications.append(m_medicationsList->item(i)->text());
    QJsonObject pastConditions;
    pastConditions["conditions"] = conditions;
    pastConditions["medications"] = medications;
    body["past_conditions"] = pastConditions;

    QUrl url = m_editMode
        ? QUrl(BASE_URL + QString("/api/db/persons/%1").arg(m_editPersonId))
        : QUrl(BASE_URL + "/api/db/persons");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *reply = m_editMode
        ? m_nam->put(req, QJsonDocument(body).toJson())
        : m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            QJsonObject err = QJsonDocument::fromJson(data).object();
            QString msg = err.value("detail").toString();
            if (msg.isEmpty()) msg = reply->errorString();
            m_statusLabel->setText(msg);
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
        if (m_editMode) {
            accept();
        } else {
            QJsonObject obj = QJsonDocument::fromJson(data).object();
            m_createdPersonName = obj.value("name").toString();
            m_createdPersonId = obj.value("id").toInt();
            accept();
        }
    });
}
