#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>

class AuthDialog : public QDialog {
    Q_OBJECT
public:
    explicit AuthDialog(QWidget *parent = nullptr);
    QString token() const;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onToggleMode();

private:
    void setupUi();

    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_emailEdit;
    QLineEdit *m_fullNameEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_registerBtn;
    QPushButton *m_toggleBtn;
    QLabel *m_statusLabel;
    QWidget *m_registerFields;

    QNetworkAccessManager *m_nam;
    QString m_token;
    bool m_isRegisterMode = false;
};

#endif
