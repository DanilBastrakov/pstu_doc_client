#ifndef PERSONSWIDGET_H
#define PERSONSWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTimer>

class ApiClient;
class CollapsiblePanel;

class PersonsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PersonsWidget(ApiClient *api, QWidget *parent = nullptr);

    int currentPersonId() const;
    QString currentPersonName() const;
    void load();

signals:
    void personSelected(int personId, const QString &personName);
    void personDeselected();
    void addPersonClicked();
    void editPersonClicked(int personId);
    void sharePersonClicked(int personId, const QString &personName);
    void removePersonClicked(int personId, const QString &personName);

private:
    void onSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous);

    ApiClient *m_api;
    CollapsiblePanel *m_panel;
    QListWidget *m_list;
    QTimer *m_refreshTimer;
};

#endif
