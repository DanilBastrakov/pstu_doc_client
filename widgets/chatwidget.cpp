#include "chatwidget.h"
#include <QHBoxLayout>
#include <QScrollBar>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("mainArea");
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    auto *messagesWidget = new QWidget();
    messagesWidget->setObjectName("messagesWidget");
    m_messagesLayout = new QVBoxLayout(messagesWidget);
    m_messagesLayout->setAlignment(Qt::AlignTop);
    auto *placeholder = new QLabel(QString::fromUtf8("Сообщения чата будут здесь"));
    placeholder->setObjectName("placeholder");
    placeholder->setAlignment(Qt::AlignCenter);
    m_messagesLayout->addWidget(placeholder);
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

    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_inputBar->text().trimmed();
        if (text.isEmpty()) return;
        appendMessage(text, true);
        m_inputBar->clear();
        emit sendRequested(text, m_historyCheck->isChecked());
    });
    connect(m_predictBtn, &QPushButton::clicked, this, [this]() {
        appendMessage(QString::fromUtf8("Запрос прогноза"), true);
        emit predictRequested();
    });
    connect(m_inputBar, &QLineEdit::returnPressed, m_sendBtn, &QPushButton::click);
}

void ChatWidget::removePlaceholder() {
    QLayoutItem *first = m_messagesLayout->itemAt(0);
    if (!first) return;
    QWidget *w = first->widget();
    if (w && w->objectName() == "placeholder") {
        delete w;
    }
}

QWidget* ChatWidget::createBubble(const QString &text, bool isUser) {
    removePlaceholder();

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

    return container;
}

void ChatWidget::scrollToBottom() {
    QScrollBar *sb = m_scrollArea->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void ChatWidget::appendMessage(const QString &text, bool isUser) {
    QWidget *container = createBubble(text, isUser);
    m_messagesLayout->addWidget(container);
    scrollToBottom();
}

void ChatWidget::clear() {
    m_streamingBubble = nullptr;
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

void ChatWidget::startStreaming() {
    removePlaceholder();

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
    scrollToBottom();

    m_streamingBubble = bubble;
}

void ChatWidget::appendStreamChunk(const QString &chunk) {
    if (m_streamingBubble)
        m_streamingBubble->setText(m_streamingBubble->text() + chunk);
}

QString ChatWidget::finalizeStreaming() {
    if (!m_streamingBubble)
        return {};
    QString text = m_streamingBubble->text();
    m_streamingBubble = nullptr;
    return text;
}

void ChatWidget::setInputEnabled(bool enabled) {
    m_inputBar->setEnabled(enabled);
    m_sendBtn->setEnabled(enabled);
    m_predictBtn->setEnabled(enabled);
}
