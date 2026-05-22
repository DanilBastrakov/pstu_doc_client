#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>

class ApiClient;

class ShareDialog : public QDialog {
    Q_OBJECT
public:
    ShareDialog(int personId, const QString &personName,
                ApiClient *api, QWidget *parent = nullptr);

private slots:
    void onShareClicked();

private:
    void setupUi();

    int m_personId;
    QString m_personName;
    ApiClient *m_api;

    QLineEdit *m_userIdInput;
    QLabel *m_statusLabel;
};

#endif
