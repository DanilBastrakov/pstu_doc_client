#include "mainwindow.h"
#include "authdialog.h"
#include "collapsiblepanel.h"
#include "persondialog.h"
#include "sharedialog.h"
#include <QSplitter>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QScrollBar>

static const QString BASE_URL = "http://localhost:8000";

MainWindow::MainWindow(const QString &token, QWidget *parent)
    : QMainWindow(parent), m_token(token)
{
    m_nam.setParent(this);

    setWindowTitle("pstu_doc_client");
    resize(900, 600);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Sidebar container
    auto *sidebar = new QWidget();
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // Persons panel
    m_personPanel = new CollapsiblePanel(QString::fromUtf8("Пациенты"));
    auto *personContent = new QVBoxLayout();
    personContent->setContentsMargins(0, 0, 0, 0);

    m_personsList = new QListWidget();
    connect(m_personsList, &QListWidget::currentItemChanged, this, &MainWindow::onPersonSelectionChanged);
    personContent->addWidget(m_personsList);

    auto *personBtnRow = new QHBoxLayout();
    auto *addPersonBtn = new QPushButton(QString::fromUtf8("Добавить пациента"));
    auto *sharePersonBtn = new QPushButton(QString::fromUtf8("Поделиться"));
    auto *removePersonBtn = new QPushButton(QString::fromUtf8("Удалить пациента"));
    personBtnRow->addWidget(addPersonBtn);
    personBtnRow->addWidget(sharePersonBtn);
    personBtnRow->addWidget(removePersonBtn);
    personContent->addLayout(personBtnRow);

    m_personPanel->setContentLayout(personContent);
    sidebarLayout->addWidget(m_personPanel);

    // Logout at bottom
    sidebarLayout->addStretch();
    auto *logoutBtn = new QPushButton(QString::fromUtf8("Выход"));
    sidebarLayout->addWidget(logoutBtn);

    splitter->addWidget(sidebar);

    // Chat area
    auto *mainArea = new QWidget();
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    auto *messagesWidget = new QWidget();
    m_messagesLayout = new QVBoxLayout(messagesWidget);
    m_messagesLayout->setAlignment(Qt::AlignTop);
    auto *messagesLabel = new QLabel(QString::fromUtf8("Сообщения чата будут здесь"));
    messagesLabel->setObjectName("placeholder");
    messagesLabel->setAlignment(Qt::AlignCenter);
    m_messagesLayout->addWidget(messagesLabel);
    m_scrollArea->setWidget(messagesWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    auto *inputRow = new QHBoxLayout();
    m_inputBar = new QLineEdit();
    m_inputBar->setPlaceholderText(QString::fromUtf8("Введите сообщение..."));
    m_inputBar->setEnabled(false);
    m_historyCheck = new QCheckBox(QString::fromUtf8("Включить историю"));
    m_historyCheck->setChecked(true);
    m_sendBtn = new QPushButton(QString::fromUtf8("Отправить"));
    m_sendBtn->setEnabled(false);
    m_predictBtn = new QPushButton(QString::fromUtf8("Прогноз"));
    m_predictBtn->setEnabled(false);
    inputRow->addWidget(m_inputBar);
    inputRow->addWidget(m_historyCheck);
    inputRow->addWidget(m_predictBtn);
    inputRow->addWidget(m_sendBtn);
    mainLayout->addLayout(inputRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_predictBtn, &QPushButton::clicked, this, &MainWindow::onPredictClicked);
    connect(m_inputBar, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    splitter->addWidget(mainArea);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 650});

    setCentralWidget(splitter);

    // Connections
    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        hide();
        AuthDialog dialog;
        if (dialog.exec() == QDialog::Accepted) {
            m_token = dialog.token();
            m_currentPersonId = -1;
            clearChatDisplay();
            m_inputBar->setEnabled(false);
            m_sendBtn->setEnabled(false);
            m_predictBtn->setEnabled(false);
            show();
            loadPersons();
        } else {
            close();
        }
    });

    connect(addPersonBtn, &QPushButton::clicked, this, &MainWindow::onAddPersonClicked);
    connect(sharePersonBtn, &QPushButton::clicked, this, &MainWindow::onSharePersonClicked);
    connect(removePersonBtn, &QPushButton::clicked, this, &MainWindow::onRemovePersonClicked);

    loadPersons();
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::loadPersons);
    m_refreshTimer->start(60000);
}

QString MainWindow::token() const { return m_token; }
void MainWindow::setToken(const QString &token) { m_token = token; }

void MainWindow::onAddPersonClicked() {
    PersonDialog dialog(m_token, this);
    if (dialog.exec() == QDialog::Accepted)
        loadPersons();
}

void MainWindow::loadPersons() {
    int prevPersonId = m_currentPersonId;

    QUrl url(BASE_URL + "/api/db/persons");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, prevPersonId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        QByteArray data = reply->readAll();
        QJsonArray arr = QJsonDocument::fromJson(data).array();

        m_personsList->blockSignals(true);
        m_personsList->clear();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            int id = obj.value("id").toInt();
            QString name = obj.value("name").toString();

            auto *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, id);
            m_personsList->addItem(item);

            if (id == prevPersonId)
                m_personsList->setCurrentItem(item);
        }
        m_personsList->blockSignals(false);

        if (prevPersonId >= 0 && !m_personsList->currentItem()) {
            m_currentPersonId = -1;
            clearChatDisplay();
            m_inputBar->setEnabled(false);
            m_sendBtn->setEnabled(false);
            m_predictBtn->setEnabled(false);
        }
    });
}

void MainWindow::onRemovePersonClicked() {
    auto *item = m_personsList->currentItem();
    if (!item) return;

    int personId = item->data(Qt::UserRole).toInt();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, QString::fromUtf8("Подтверждение удаления"),
        QString::fromUtf8("Удалить пациента \"%1\"?").arg(item->text()),
        QMessageBox::Yes | QMessageBox::No
    );
    if (reply != QMessageBox::Yes) return;

    QUrl url(BASE_URL + QString("/api/db/persons/%1").arg(personId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *replyNet = m_nam.deleteResource(req);
    connect(replyNet, &QNetworkReply::finished, this, [this, replyNet, item]() {
        replyNet->deleteLater();
        if (replyNet->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"), replyNet->errorString());
            return;
        }
        delete item;
    });
}

void MainWindow::onSharePersonClicked() {
    auto *item = m_personsList->currentItem();
    if (!item) {
        QMessageBox::warning(this, QString::fromUtf8("Ошибка"),
                             QString::fromUtf8("Выберите пациента"));
        return;
    }

    int personId = item->data(Qt::UserRole).toInt();
    ShareDialog dialog(personId, item->text(), m_token, this);
    if (dialog.exec() == QDialog::Accepted) {
        QMessageBox::information(this, QString::fromUtf8("Успешно"),
                                 QString::fromUtf8("Пациент передан"));
    }
}

void MainWindow::appendMessage(const QString &text, bool isUser) {
    QLayoutItem *first = m_messagesLayout->itemAt(0);
    if (first) {
        QWidget *w = first->widget();
        if (w && qobject_cast<QLabel *>(w) &&
            w->objectName() == "placeholder")
        {
            delete w;
        }
    }

    auto *bubble = new QLabel(text);
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(400);
    bubble->setContentsMargins(8, 6, 8, 6);

    if (isUser) {
        bubble->setStyleSheet(
            "background-color: #007AFF; color: white; "
            "border-radius: 10px; padding: 8px;");
        bubble->setAlignment(Qt::AlignRight);
    } else {
        bubble->setStyleSheet(
            "background-color: #E8E8E8; color: black; "
            "border-radius: 10px; padding: 8px;");
        bubble->setAlignment(Qt::AlignLeft);
    }

    auto *container = new QWidget();
    auto *containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(0, 2, 0, 2);
    if (isUser) {
        containerLayout->addStretch();
        containerLayout->addWidget(bubble);
    } else {
        containerLayout->addWidget(bubble);
        containerLayout->addStretch();
    }

    m_messagesLayout->addWidget(container);

    QScrollBar *sb = m_scrollArea->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::clearChatDisplay() {
    QLayoutItem *child;
    while ((child = m_messagesLayout->takeAt(0)) != nullptr) {
        if (child->widget())
            delete child->widget();
        delete child;
    }
    auto *placeholder = new QLabel(QString::fromUtf8("Сообщения чата будут здесь"));
    placeholder->setObjectName("placeholder");
    placeholder->setAlignment(Qt::AlignCenter);
    m_messagesLayout->addWidget(placeholder);
}

void MainWindow::onPersonSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous) {
    Q_UNUSED(previous);

    clearChatDisplay();

    if (!current) {
        m_currentPersonId = -1;
        m_inputBar->setEnabled(false);
        m_sendBtn->setEnabled(false);
        m_predictBtn->setEnabled(false);
        return;
    }

    m_currentPersonId = current->data(Qt::UserRole).toInt();
    m_inputBar->setEnabled(true);
    m_sendBtn->setEnabled(true);
    //m_predictBtn->setEnabled(true);

    // Fetch chat history for this person
    QUrl url(BASE_URL + QString("/api/chat/%1").arg(m_currentPersonId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    int personId = m_currentPersonId;
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, personId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;
        if (personId != m_currentPersonId)
            return;

        QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            QString role = obj.value("role").toString();
            QString content = obj.value("content").toString();
            if (!content.isEmpty())
                appendMessage(content, role == "user");
        }
    });
}

void MainWindow::onSendClicked() {
    QString text = m_inputBar->text().trimmed();
    if (text.isEmpty()) return;
    if (m_currentPersonId < 0) return;

    appendMessage(text, true);
    m_inputBar->clear();

    // Step 1: save message to chat history
    QUrl chatUrl(BASE_URL + QString("/api/chat/%1").arg(m_currentPersonId));
    QNetworkRequest chatReq(chatUrl);
    chatReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    chatReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QJsonObject chatBody;
    chatBody["role"] = "user";
    chatBody["content"] = text;

    QNetworkReply *chatReply = m_nam.post(chatReq, QJsonDocument(chatBody).toJson(QJsonDocument::Compact));
    connect(chatReply, &QNetworkReply::finished, this, [this, chatReply, text]() {
        chatReply->deleteLater();
        if (chatReply->error() != QNetworkReply::NoError)
            return;

        // Step 2: get enriched prompt
        bool includeHistory = m_historyCheck->isChecked();
        QString appendUrl = BASE_URL + QString("/api/ai/append_prompt/%1").arg(m_currentPersonId);
        if (includeHistory)
            appendUrl += "?include_history=true";

        QUrl url(appendUrl);
        QNetworkRequest appendReq(url);
        appendReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        appendReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

        QJsonObject appendBody;
        appendBody["prompt"] = text;

        QNetworkReply *appendReply = m_nam.post(appendReq, QJsonDocument(appendBody).toJson(QJsonDocument::Compact));
        connect(appendReply, &QNetworkReply::finished, this, [this, appendReply]() {
            appendReply->deleteLater();
            if (appendReply->error() != QNetworkReply::NoError)
                return;

            QJsonObject appendObj = QJsonDocument::fromJson(appendReply->readAll()).object();
            QString retrievedPrompt = appendObj.value("prompt").toString();
            if (retrievedPrompt.isEmpty())
                return;

            // Step 3: forward prompt to AI service
            QUrl aiUrl("http://localhost:8001/api/generate");
            QNetworkRequest aiReq(aiUrl);
            aiReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            QJsonObject aiBody;
            aiBody["prompt"] = retrievedPrompt;
            aiBody["model"] = "mistral";
            aiBody["language"] = "russian";
            aiBody["use_rag"] = true;

            QNetworkReply *aiReply = m_nam.post(aiReq, QJsonDocument(aiBody).toJson(QJsonDocument::Compact));
            connect(aiReply, &QNetworkReply::finished, this, [this, aiReply]() {
                aiReply->deleteLater();
                if (aiReply->error() != QNetworkReply::NoError)
                    return;

                QJsonObject obj = QJsonDocument::fromJson(aiReply->readAll()).object();
                QString response = obj.value("response").toString();
                if (!response.isEmpty()) {
                    QUrl chatUrl(BASE_URL + QString("/api/chat/%1").arg(m_currentPersonId));
                    QNetworkRequest chatReq(chatUrl);
                    chatReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                    chatReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
                    QJsonObject chatBody;
                    chatBody["role"] = "assistant";
                    chatBody["content"] = response;
                    QNetworkReply *chatReply = m_nam.post(chatReq, QJsonDocument(chatBody).toJson(QJsonDocument::Compact));
                    connect(chatReply, &QNetworkReply::finished, chatReply, &QNetworkReply::deleteLater);

                    appendMessage(response, false);
                }

            });
        });
    });
}

void MainWindow::onPredictClicked() {
    if (m_currentPersonId < 0) return;

    appendMessage(QString::fromUtf8("Запрос прогноза"), true);

    QUrl chatUrl(BASE_URL + QString("/api/chat/%1").arg(m_currentPersonId));
    QNetworkRequest chatReq(chatUrl);
    chatReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    chatReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QJsonObject chatBody;
    chatBody["role"] = "user";
    chatBody["content"] = QString::fromUtf8("Запрос прогноза");
    QNetworkReply *chatReply = m_nam.post(chatReq, QJsonDocument(chatBody).toJson(QJsonDocument::Compact));
    connect(chatReply, &QNetworkReply::finished, chatReply, &QNetworkReply::deleteLater);

    QUrl appendUrl(BASE_URL + QString("/api/ai/append_prediction/%1").arg(m_currentPersonId));
    QNetworkRequest appendReq(appendUrl);
    appendReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    appendReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QJsonObject appendBody;
    appendBody["prompt"] = QString::fromUtf8("Запрос прогноза");

    QNetworkReply *appendReply = m_nam.post(appendReq, QJsonDocument(appendBody).toJson(QJsonDocument::Compact));
    connect(appendReply, &QNetworkReply::finished, this, [this, appendReply]() {
        appendReply->deleteLater();
        if (appendReply->error() != QNetworkReply::NoError)
            return;

        QJsonObject appendObj = QJsonDocument::fromJson(appendReply->readAll()).object();
        QString retrievedPrompt = appendObj.value("prompt").toString();
        if (retrievedPrompt.isEmpty())
            return;

        QUrl aiUrl("http://localhost:8001/api/generate");
        QNetworkRequest aiReq(aiUrl);
        aiReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject aiBody;
        aiBody["prompt"] = retrievedPrompt;
        aiBody["model"] = "mistral";
        aiBody["language"] = "russian";
        aiBody["use_rag"] = true;

        QNetworkReply *aiReply = m_nam.post(aiReq, QJsonDocument(aiBody).toJson(QJsonDocument::Compact));
        connect(aiReply, &QNetworkReply::finished, this, [this, aiReply]() {
            aiReply->deleteLater();
            if (aiReply->error() != QNetworkReply::NoError)
                return;

            QJsonObject obj = QJsonDocument::fromJson(aiReply->readAll()).object();
            QString response = obj.value("response").toString();
            if (!response.isEmpty())
                appendMessage(response, false);
        });
    });
}


