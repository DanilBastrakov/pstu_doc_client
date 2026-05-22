#include "mainwindow.h"
#include "authdialog.h"
#include "collapsiblepanel.h"
#include "persondialog.h"
#include "sharedialog.h"
#include "api_client.h"
#include <QSplitter>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QScrollBar>

MainWindow::MainWindow(ApiClient *api, QWidget *parent)
    : QMainWindow(parent), m_api(api)
{
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

    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        hide();
        AuthDialog dialog(m_api);
        if (dialog.exec() == QDialog::Accepted) {
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

void MainWindow::onAddPersonClicked() {
    PersonDialog dialog(m_api, this);
    if (dialog.exec() == QDialog::Accepted)
        loadPersons();
}

void MainWindow::loadPersons() {
    int prevPersonId = m_currentPersonId;

    m_api->get("/api/db/persons", [this, prevPersonId](const ApiResult &r) {
        if (!r.ok) return;

        QJsonArray arr = r.data.array();

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

    m_api->remove(QString("/api/db/persons/%1").arg(personId), [this, item](const ApiResult &r) {
        if (!r.ok) {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"), r.error);
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
    ShareDialog dialog(personId, item->text(), m_api, this);
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

    m_api->get(QString("/api/db/persons/%1").arg(personId), [this, personId](const ApiResult &r) {
        if (!r.ok) {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"),
                                 QString::fromUtf8("Не удалось загрузить данные пациента"));
            return;
        }

        QJsonObject obj = r.data.object();
        PersonDialog dialog(m_api, personId, obj, this);
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

    int personId = m_currentPersonId;
    m_api->get(QString("/api/chat/%1").arg(personId), [this, personId](const ApiResult &r) {
        if (!r.ok) return;
        if (personId != m_currentPersonId) return;

        QJsonArray arr = r.data.array();
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

    QJsonObject chatBody;
    chatBody["role"] = "user";
    chatBody["content"] = text;

    m_api->post(QString("/api/chat/%1").arg(m_currentPersonId), chatBody, [this, text](const ApiResult &r) {
        if (!r.ok) return;

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

    QJsonObject chatBody;
    chatBody["role"] = "user";
    chatBody["content"] = QString::fromUtf8("Запрос прогноза");
    m_api->post(QString("/api/chat/%1").arg(m_currentPersonId), chatBody, [](const ApiResult &) {});

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

    QNetworkReply *reply = m_api->postStream(path, body);

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
            QJsonObject chatBody;
            chatBody["role"] = "assistant";
            chatBody["content"] = m_streamingBubble->text();
            m_api->post(QString("/api/chat/%1").arg(m_currentPersonId), chatBody, [](const ApiResult &) {});
        }
        m_streamingBubble = nullptr;
        m_streamAccumulator.clear();
    });
}
