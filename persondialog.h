#ifndef PERSONDIALOG_H
#define PERSONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QJsonObject>

class ApiClient;

class PersonDialog : public QDialog {
    Q_OBJECT
public:
    explicit PersonDialog(ApiClient *api, QWidget *parent = nullptr);
    explicit PersonDialog(ApiClient *api, int personId, const QJsonObject &personData, QWidget *parent = nullptr);

private slots:
    void onSaveClicked();

private:
    void setupUi();
    void populateFields(const QJsonObject &obj);

    ApiClient *m_api;

    bool m_editMode = false;
    int m_editPersonId = -1;

    QLineEdit *m_nameEdit;
    QSpinBox *m_ageSpin;
    QComboBox *m_genderCombo;

    QListWidget *m_symptomsList;
    QLineEdit *m_symptomInput;

    QComboBox *m_smokingCombo;
    QComboBox *m_alcoholCombo;
    QComboBox *m_exerciseCombo;

    QListWidget *m_conditionsList;
    QLineEdit *m_conditionInput;
    QListWidget *m_medicationsList;
    QLineEdit *m_medicationInput;

    QLabel *m_statusLabel;

    QString m_createdPersonName;
    int m_createdPersonId = 0;

public:
    QString createdPersonName() const { return m_createdPersonName; }
    int createdPersonId() const { return m_createdPersonId; }
};

#endif
