#include "mainwindow.h"
#include "dialogs/authdialog.h"
#include "dialogs/persondialog.h"
#include "dialogs/sharedialog.h"
#include "api_client.h"
#include "widgets/chatwidget.h"
#include "widgets/personswidget.h"
#include <QSplitter>
#include <QPushButton>
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QVBoxLayout>

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

    m_personsWidget = new PersonsWidget(m_api);
    sidebarLayout->addWidget(m_personsWidget, 1);

    auto *logoutBtn = new QPushButton(QString::fromUtf8("Выход"));
    logoutBtn->setObjectName("logoutBtn");
    sidebarLayout->addWidget(logoutBtn);

    splitter->addWidget(sidebar);

    m_chatWidget = new ChatWidget();
    splitter->addWidget(m_chatWidget);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({350, 550});

    setCentralWidget(splitter);

    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        hide();
        AuthDialog dialog(m_api);
        if (dialog.exec() == QDialog::Accepted) {
            m_currentPersonId = -1;
            m_chatWidget->clear();
            m_chatWidget->setInputEnabled(false);
            show();
            m_personsWidget->load();
        } else {
            close();
        }
    });

    connect(m_personsWidget, &PersonsWidget::addPersonClicked,
            this, &MainWindow::onAddPersonClicked);
    connect(m_personsWidget, &PersonsWidget::editPersonClicked,
            this, &MainWindow::onRedactPersonClicked);
    connect(m_personsWidget, &PersonsWidget::sharePersonClicked,
            this, &MainWindow::onSharePersonClicked);
    connect(m_personsWidget, &PersonsWidget::removePersonClicked,
            this, &MainWindow::onRemovePersonClicked);
    connect(m_personsWidget, &PersonsWidget::personSelected,
            this, &MainWindow::onPersonSelected);
    connect(m_personsWidget, &PersonsWidget::personDeselected,
            this, &MainWindow::onPersonDeselected);

    connect(m_chatWidget, &ChatWidget::sendRequested,
            this, &MainWindow::onSendClicked);
    connect(m_chatWidget, &ChatWidget::predictRequested,
            this, &MainWindow::onPredictClicked);

    m_personsWidget->load();
}

void MainWindow::onAddPersonClicked() {
    PersonDialog dialog(m_api, this);
    if (dialog.exec() == QDialog::Accepted)
        m_personsWidget->load();
}

void MainWindow::onRemovePersonClicked(int personId, const QString &personName) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, QString::fromUtf8("Подтверждение удаления"),
        QString::fromUtf8("Удалить пациента \"%1\"?").arg(personName),
        QMessageBox::Yes | QMessageBox::No
    );
    if (reply != QMessageBox::Yes) return;

    m_api->remove(QString("/api/db/persons/%1").arg(personId), [this](const ApiResult &r) {
        if (!r.ok) {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"), r.error);
            return;
        }
        m_personsWidget->load();
    });
}

void MainWindow::onSharePersonClicked(int personId, const QString &personName) {
    ShareDialog dialog(personId, personName, m_api, this);
    if (dialog.exec() == QDialog::Accepted) {
        QMessageBox::information(this, QString::fromUtf8("Успешно"),
                                 QString::fromUtf8("Пациент передан"));
    }
}

void MainWindow::onRedactPersonClicked(int personId) {
    m_api->get(QString("/api/db/persons/%1").arg(personId), [this, personId](const ApiResult &r) {
        if (!r.ok) {
            QMessageBox::warning(this, QString::fromUtf8("Ошибка"),
                                 QString::fromUtf8("Не удалось загрузить данные пациента"));
            return;
        }

        QJsonObject obj = r.data.object();
        PersonDialog dialog(m_api, personId, obj, this);
        if (dialog.exec() == QDialog::Accepted)
            m_personsWidget->load();
    });
}

void MainWindow::onPersonSelected(int personId, const QString &personName) {
    Q_UNUSED(personName);
    m_currentPersonId = personId;
    m_chatWidget->clear();
    m_chatWidget->setInputEnabled(true);

    m_api->get(QString("/api/chat/%1").arg(personId), [this, personId](const ApiResult &r) {
        if (!r.ok) return;
        if (personId != m_currentPersonId) return;

        QJsonArray arr = r.data.array();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            QString role = obj.value("role").toString();
            QString content = obj.value("content").toString();
            if (!content.isEmpty())
                m_chatWidget->appendMessage(content, role == "user");
        }
    });
}

void MainWindow::onPersonDeselected() {
    m_currentPersonId = -1;
    m_chatWidget->clear();
    m_chatWidget->setInputEnabled(false);
}

void MainWindow::onSendClicked(const QString &text, bool includeHistory) {
    if (m_currentPersonId < 0) return;

    QJsonObject chatBody;
    chatBody["role"] = "user";
    chatBody["content"] = text;

    m_api->post(QString("/api/chat/%1").arg(m_currentPersonId), chatBody, [this, text, includeHistory](const ApiResult &r) {
        if (!r.ok) return;

        QJsonObject body;
        body["prompt"] = text;
        body["include_history"] = includeHistory;
        startAiStream(
            QString("/api/ai/append_and_generate/stream/%1").arg(m_currentPersonId),
            body);
    });
}

void MainWindow::onPredictClicked() {
    if (m_currentPersonId < 0) return;

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
    m_chatWidget->startStreaming();
    m_streamPersonId = m_currentPersonId;

    m_api->postStream(path, body,
        [this](const QString &chunk) {
            m_chatWidget->appendStreamChunk(chunk);
        },
        [this](const StreamResult &r) {
            QString fullText = m_chatWidget->finalizeStreaming();
            if (r.ok && !fullText.isEmpty()) {
                QJsonObject chatBody;
                chatBody["role"] = "assistant";
                chatBody["content"] = fullText;
                m_api->post(QString("/api/chat/%1").arg(m_currentPersonId),
                            chatBody, [](const ApiResult &) {});
            }
        },
        [this]() { return m_streamPersonId == m_currentPersonId; }
    );
}
