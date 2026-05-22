#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

class ChatWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void appendMessage(const QString &text, bool isUser);
    void clear();

    void startStreaming();
    void appendStreamChunk(const QString &chunk);
    QString finalizeStreaming();
    bool isStreaming() const { return m_streamingBubble != nullptr; }

    void setInputEnabled(bool enabled);

signals:
    void sendRequested(const QString &text, bool includeHistory);
    void predictRequested();

private:
    void removePlaceholder();
    QWidget* createBubble(const QString &text, bool isUser);
    void scrollToBottom();

    QVBoxLayout *m_messagesLayout;
    QScrollArea *m_scrollArea;

    QLabel *m_streamingBubble = nullptr;

    QLineEdit *m_inputBar;
    QPushButton *m_sendBtn;
    QPushButton *m_predictBtn;
    QCheckBox *m_historyCheck;
};

#endif
