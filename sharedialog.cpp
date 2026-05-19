#include "sharedialog.h"

#include <iostream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QMessageBox>

// временный хардкод, т.к. денег на VPS нету...
static const QString BASE_URL = "http://localhost:8000";

ShareDialog::ShareDialog(int personId, const QString &personName,
                         const QString &token, QWidget *parent)
    : QDialog(parent), m_personId(personId), m_personName(personName), m_token(token),
      m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle(QString::fromUtf8("Поделиться пациентом"));
    resize(350, 200);
    setupUi();
}

void ShareDialog::setupUi() {
    auto *layout = new QVBoxLayout(this);

    auto *infoLabel = new QLabel(
        QString::fromUtf8("Пациент: %1").arg(m_personName));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    m_userIdInput = new QLineEdit();
    m_userIdInput->setPlaceholderText(QString::fromUtf8("Имя пользователя"));
    layout->addWidget(m_userIdInput);

    m_statusLabel = new QLabel();
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *btnRow = new QHBoxLayout();
    auto *shareBtn = new QPushButton(QString::fromUtf8("Поделиться"));
    auto *cancelBtn = new QPushButton(QString::fromUtf8("Отмена"));
    btnRow->addWidget(shareBtn);
    btnRow->addWidget(cancelBtn);
    layout->addLayout(btnRow);

    connect(shareBtn, &QPushButton::clicked, this, &ShareDialog::onShareClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ShareDialog::onShareClicked() {
    QString userIdText = m_userIdInput->text().trimmed();
    if (userIdText.isEmpty()) {
        m_statusLabel->setText(QString::fromUtf8("Введите ID пользователя"));
        m_statusLabel->setStyleSheet("color: red;");
        return;
    }

    m_statusLabel->setText(QString::fromUtf8("Отправка..."));
    m_statusLabel->setStyleSheet("color: gray;");

    QUrl url(BASE_URL + QString("/api/db/persons/%1/share").arg(m_personId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QJsonObject body;
    body["username"] = userIdText;

    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());
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
        accept();
    });
}
