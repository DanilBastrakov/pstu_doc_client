#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QScrollArea>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QJsonObject>

class CollapsiblePanel;
class ApiClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ApiClient *api, QWidget *parent = nullptr);

private slots:
    void onAddPersonClicked();
    void onRemovePersonClicked();
    void onSharePersonClicked();
    void onRedactPersonClicked();
    void onSendClicked();
    void onPredictClicked();
    void onPersonSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous);

private:
    void loadPersons();
    void appendMessage(const QString &text, bool isUser);
    void clearChatDisplay();
    QLabel* createStreamingBubble();
    void startAiStream(const QString &path, const QJsonObject &body);

    ApiClient *m_api;
    int m_currentPersonId = -1;
    int button_spacing = 2;

    QLabel *m_streamingBubble = nullptr;
    QString m_streamAccumulator;
    int m_streamPersonId = -1;

    QTimer *m_refreshTimer;

    CollapsiblePanel *m_personPanel;
    QListWidget *m_personsList;

    QLineEdit *m_inputBar;
    QPushButton *m_sendBtn;
    QPushButton *m_predictBtn;
    QCheckBox *m_historyCheck;
    QVBoxLayout *m_messagesLayout;
    QScrollArea *m_scrollArea;
};

#endif
