#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QNetworkAccessManager>

class ShareDialog : public QDialog {
    Q_OBJECT
public:
    ShareDialog(int personId, const QString &personName,
                const QString &token, QWidget *parent = nullptr);

private slots:
    void onShareClicked();

private:
    void setupUi();

    int m_personId;
    QString m_personName;
    QString m_token;
    QNetworkAccessManager *m_nam;

    QLineEdit *m_userIdInput;
    QLabel *m_statusLabel;
};

#endif
