#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QScrollArea>
#include <QPushButton>
#include <QCheckBox>

class CollapsiblePanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &token, QWidget *parent = nullptr);
    QString token() const;
    void setToken(const QString &token);

private slots:
    void onAddPersonClicked();
    void onRemovePersonClicked();
    void onSharePersonClicked();
    void onRedactPersonClicked();
    void onSendClicked();
    void onPredictClicked();
    void onPersonSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous);

private:
    void setupUi();
    void loadPersons();
    void appendMessage(const QString &text, bool isUser);
    void clearChatDisplay();

    QString m_token;
    int m_currentPersonId = -1;
    QNetworkAccessManager m_nam;

    QTimer *m_refreshTimer;

    // persons panel
    CollapsiblePanel *m_personPanel;
    QListWidget *m_personsList;

    // chat area
    QLineEdit *m_inputBar;
    QPushButton *m_sendBtn;
    QPushButton *m_predictBtn;
    QCheckBox *m_historyCheck;
    QVBoxLayout *m_messagesLayout;
    QScrollArea *m_scrollArea;
};

#endif
