#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonObject>

class ApiClient;
class ChatWidget;
class PersonsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ApiClient *api, QWidget *parent = nullptr);

private slots:
    void onAddPersonClicked();
    void onRemovePersonClicked(int personId, const QString &personName);
    void onSharePersonClicked(int personId, const QString &personName);
    void onRedactPersonClicked(int personId);
    void onSendClicked(const QString &text, bool includeHistory);
    void onPredictClicked();
    void onPersonSelected(int personId, const QString &personName);
    void onPersonDeselected();

private:
    void startAiStream(const QString &path, const QJsonObject &body);

    ApiClient *m_api;
    ChatWidget *m_chatWidget;
    PersonsWidget *m_personsWidget;
    int m_currentPersonId = -1;
    int m_streamPersonId = -1;
};

#endif
