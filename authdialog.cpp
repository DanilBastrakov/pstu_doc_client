#include "authdialog.h"
#include "api_client.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>

AuthDialog::AuthDialog(ApiClient *api, QWidget *parent)
    : QDialog(parent)
    , m_api(api)
{
    setWindowTitle(QString::fromUtf8("Вход"));
    setFixedSize(400, 350);
    setupUi();
}

void AuthDialog::setupUi() {
    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel("MedDoc AI");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(title);

    auto *form = new QFormLayout();

    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText(QString::fromUtf8("Имя пользователя"));
    form->addRow(QString::fromUtf8("Имя пользователя:"), m_usernameEdit);

    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setPlaceholderText(QString::fromUtf8("Пароль"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addRow(QString::fromUtf8("Пароль:"), m_passwordEdit);

    m_registerFields = new QWidget();
    auto *regLayout = new QFormLayout(m_registerFields);
    regLayout->setContentsMargins(0, 0, 0, 0);

    m_registerFields->setVisible(false);
    form->addRow(m_registerFields);

    layout->addLayout(form);

    m_statusLabel = new QLabel();
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_loginBtn = new QPushButton(QString::fromUtf8("Войти"));
    m_registerBtn = new QPushButton(QString::fromUtf8("Регистрация"));
    m_registerBtn->setVisible(false);

    layout->addWidget(m_loginBtn);
    layout->addWidget(m_registerBtn);

    m_toggleBtn = new QPushButton(QString::fromUtf8("Зарегистрироваться"));
    m_toggleBtn->setFlat(true);
    layout->addWidget(m_toggleBtn);

    connect(m_loginBtn, &QPushButton::clicked, this, &AuthDialog::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &AuthDialog::onRegisterClicked);
    connect(m_toggleBtn, &QPushButton::clicked, this, &AuthDialog::onToggleMode);

    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &AuthDialog::onLoginClicked);
}

void AuthDialog::onToggleMode() {
    m_isRegisterMode = !m_isRegisterMode;
    m_registerFields->setVisible(m_isRegisterMode);
    m_loginBtn->setVisible(!m_isRegisterMode);
    m_registerBtn->setVisible(m_isRegisterMode);
    m_toggleBtn->setText(m_isRegisterMode ? QString::fromUtf8("Войти") : QString::fromUtf8("Зарегистрироваться"));
    setWindowTitle(m_isRegisterMode ? QString::fromUtf8("Регистрация") : QString::fromUtf8("Вход"));
    m_statusLabel->clear();
}

void AuthDialog::onLoginClicked() {
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("Заполните все поля"));
        m_statusLabel->setStyleSheet("color: red;");
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("Выполняется вход..."));
    m_statusLabel->setStyleSheet("color: gray;");

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;

    m_api->post("/api/auth/login", body, [this](const ApiResult &r) {
        if (!r.ok) {
            m_statusLabel->setText(r.error);
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
        m_api->setToken(r.data.object().value("access_token").toString());
        accept();
    }, false);
}

void AuthDialog::onRegisterClicked() {
    QString username = m_usernameEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("Заполните все поля"));
        m_statusLabel->setStyleSheet("color: red;");
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("Выполняется регистрация..."));
    m_statusLabel->setStyleSheet("color: gray;");

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;

    m_api->post("/api/auth/register", body, [this](const ApiResult &r) {
        if (!r.ok) {
            m_statusLabel->setText(r.error);
            m_statusLabel->setStyleSheet("color: red;");
            return;
        }
        onLoginClicked();
    }, false);
}
