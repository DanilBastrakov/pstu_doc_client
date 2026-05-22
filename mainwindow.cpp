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

// временный хардкод, т.к. денег на VPS нету...
static const QString BASE_URL = "http://localhost:8000";

MainWindow::MainWindow(const QString &token, QWidget *parent)
    : QMainWindow(parent), m_token(token)
{
    m_nam.setParent(this);

    setWindowTitle("MedDoc AI");
    resize(900, 600);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *sidebar = new QWidget();
    sidebar->setObjectName("sidebar");
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    auto *refreshPatientsBtn = new QPushButton(QString::fromUtf8("Обновить"));
    refreshPatientsBtn->setObjectName("refreshPatientBtn");
    connect(refreshPatientsBtn, &QPushButton::clicked, this, &MainWindow::loadPersons);

    m_personPanel = new CollapsiblePanel(QString::fromUtf8("Пациенты"), refreshPatientsBtn);
    auto *personContent = new QVBoxLayout();
    personContent->setContentsMargins(0, 0, 0, 0);

    m_personsList = new QListWidget();
    connect(m_personsList, &QListWidget::currentItemChanged, this, &MainWindow::onPersonSelectionChanged);
    personContent->addWidget(m_personsList, 1);
    auto *personBtnRow1 = new QHBoxLayout();
    personBtnRow1->setSpacing(button_spacing);
    auto *personBtnRow2 = new QHBoxLayout();
    personBtnRow2->setSpacing(button_spacing);

    auto *addPersonBtn = new QPushButton(QString::fromUtf8("Добавить пациента"));
    addPersonBtn->setObjectName("addPersonBtn");
    auto *sharePersonBtn = new QPushButton(QString::fromUtf8("Поделиться"));
    sharePersonBtn->setObjectName("sharePersonBtn");
    auto *editPersonBtn = new QPushButton(QString::fromUtf8("Редактировать"));
    editPersonBtn->setObjectName("editPersonBtn");
    auto *removePersonBtn = new QPushButton(QString::fromUtf8("Удалить пациента"));
    removePersonBtn->setObjectName("removePersonBtn");
    personBtnRow1->addWidget(addPersonBtn);
    personBtnRow1->addWidget(sharePersonBtn);
    personBtnRow2->addWidget(editPersonBtn);
    personBtnRow2->addWidget(removePersonBtn);

    m_personPanel->setContentLayout(personContent);
    sidebarLayout->addWidget(m_personPanel, 1);


    sidebarLayout->addLayout(personBtnRow1);
    sidebarLayout->setSpacing(button_spacing);
    sidebarLayout->addLayout(personBtnRow2);

    auto *logoutBtn = new QPushButton(QString::fromUtf8("Выход"));
    logoutBtn->setObjectName("logoutBtn");
    sidebarLayout->addWidget(logoutBtn);

    splitter->addWidget(sidebar);

    auto *mainArea = new QWidget();
    mainArea->setObjectName("mainArea");
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    auto *messagesWidget = new QWidget();
    messagesWidget->setObjectName("messagesWidget");
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
    m_predictBtn->setObjectName("predictBtn");
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
    splitter->setSizes({350, 550});

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
    connect(editPersonBtn, &QPushButton::clicked, this, &MainWindow::onRedactPersonClicked);
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

void MainWindow::onRedactPersonClicked() {
    auto *item = m_personsList->currentItem();
    if (!item) {
        QMessageBox::warning(this, QString::fromUtf8("Ошибка"),
                             QString::fromUtf8("Выберите пациента"));
        return;
    }

    int personId = item->data(Qt::UserRole).toInt();

    QUrl url(BASE_URL + QString("/api/db/persons/%1").arg(personId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, personId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"),
                                 QString::fromUtf8("Не удалось загрузить данные пациента"));
            return;
        }

        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        PersonDialog dialog(m_token, personId, obj, this);
        if (dialog.exec() == QDialog::Accepted)
            loadPersons();
    });
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
            "background-color: #2081c3; color: #f7f9f9; "
            "border-radius: 10px; padding: 8px;");
        bubble->setAlignment(Qt::AlignRight);
    } else {
        bubble->setStyleSheet(
            "background-color: #f7f9f9; color: #000000; "
            "border: 1px solid #bed8d4; border-radius: 10px; padding: 8px;");
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
    m_streamingBubble = nullptr;
    m_streamAccumulator.clear();
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

QLabel* MainWindow::createStreamingBubble() {
    QLayoutItem *first = m_messagesLayout->itemAt(0);
    if (first) {
        QWidget *w = first->widget();
        if (w && qobject_cast<QLabel *>(w) &&
            w->objectName() == "placeholder")
        {
            delete w;
        }
    }

    auto *bubble = new QLabel();
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(400);
    bubble->setContentsMargins(8, 6, 8, 6);
    bubble->setStyleSheet(
        "background-color: #f7f9f9; color: #000000; "
        "border: 1px solid #bed8d4; border-radius: 10px; padding: 8px;");
    bubble->setAlignment(Qt::AlignLeft);

    auto *container = new QWidget();
    auto *containerLayout = new QHBoxLayout(container);
    containerLayout->setContentsMargins(0, 2, 0, 2);
    containerLayout->addWidget(bubble);
    containerLayout->addStretch();

    m_messagesLayout->addWidget(container);

    QScrollBar *sb = m_scrollArea->verticalScrollBar();
    sb->setValue(sb->maximum());

    return bubble;
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
    m_predictBtn->setEnabled(true);

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

        QJsonObject body;
        body["prompt"] = text;
        body["include_history"] = m_historyCheck->isChecked();
        startAiStream(
            QString("/api/ai/append_and_generate/stream/%1").arg(m_currentPersonId),
            body);
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

    QJsonObject body;
    body["prompt"] = QString::fromUtf8("Запрос прогноза");
    startAiStream(
        QString("/api/ai/append_and_generate_prediction/stream/%1").arg(m_currentPersonId),
        body);
}

void MainWindow::startAiStream(const QString &path, const QJsonObject &body) {
    m_streamingBubble = createStreamingBubble();
    m_streamAccumulator.clear();
    m_streamPersonId = m_currentPersonId;

    QUrl url(BASE_URL + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (m_streamPersonId != m_currentPersonId) {
            reply->abort();
            reply->deleteLater();
            return;
        }

        m_streamAccumulator += QString::fromUtf8(reply->readAll()).remove('\r');

        int idx;
        while ((idx = m_streamAccumulator.indexOf("\n\n")) != -1) {
            QString event = m_streamAccumulator.left(idx);
            m_streamAccumulator = m_streamAccumulator.mid(idx + 2);

            QString dataValue;
            for (const QString &line : event.split('\n'))
                if (line.startsWith("data: "))
                    dataValue = line.mid(6);

            if (dataValue.trimmed() == "[DONE]" || dataValue.isEmpty())
                continue;

            QString chunk;
            if (dataValue.startsWith('{')) {
                QJsonDocument doc = QJsonDocument::fromJson(dataValue.toUtf8());
                if (doc.isObject())
                    chunk = doc.object().value("response").toString();
            } else {
                chunk = dataValue;
            }

            if (!chunk.isEmpty() && m_streamingBubble)
                m_streamingBubble->setText(m_streamingBubble->text() + chunk);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_streamingBubble = nullptr;
            m_streamAccumulator.clear();
            return;
        }
        if (!m_streamAccumulator.trimmed().isEmpty() &&
            m_streamAccumulator.trimmed() != "[DONE]" && m_streamingBubble)
        {
            QString dataValue;
            for (const QString &line : m_streamAccumulator.split('\n'))
                if (line.startsWith("data: "))
                    dataValue = line.mid(6);
            if (!dataValue.isEmpty() && dataValue != "[DONE]") {
                QString chunk;
                if (dataValue.startsWith('{')) {
                    QJsonDocument doc = QJsonDocument::fromJson(dataValue.toUtf8());
                    if (doc.isObject())
                        chunk = doc.object().value("response").toString();
                } else {
                    chunk = dataValue;
                }
                if (!chunk.isEmpty())
                    m_streamingBubble->setText(m_streamingBubble->text() + chunk);
            }
        }
        if (m_streamingBubble && !m_streamingBubble->text().isEmpty()) {
            QUrl chatUrl(BASE_URL + QString("/api/chat/%1").arg(m_currentPersonId));
            QNetworkRequest chatReq(chatUrl);
            chatReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            chatReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
            QJsonObject chatBody;
            chatBody["role"] = "assistant";
            chatBody["content"] = m_streamingBubble->text();
            QNetworkReply *chatReply = m_nam.post(chatReq, QJsonDocument(chatBody).toJson(QJsonDocument::Compact));
            connect(chatReply, &QNetworkReply::finished, chatReply, &QNetworkReply::deleteLater);
        }
        m_streamingBubble = nullptr;
        m_streamAccumulator.clear();
    });
}


