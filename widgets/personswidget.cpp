#include "personswidget.h"
#include "../api_client.h"
#include "collapsiblepanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>

PersonsWidget::PersonsWidget(ApiClient *api, QWidget *parent)
    : QWidget(parent), m_api(api)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *refreshBtn = new QPushButton(QString::fromUtf8("Обновить"));
    refreshBtn->setObjectName("refreshPatientBtn");
    connect(refreshBtn, &QPushButton::clicked, this, &PersonsWidget::load);

    m_panel = new CollapsiblePanel(QString::fromUtf8("Пациенты"), refreshBtn);
    auto *personContent = new QVBoxLayout();
    personContent->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget();
    connect(m_list, &QListWidget::currentItemChanged, this, &PersonsWidget::onSelectionChanged);
    personContent->addWidget(m_list, 1);

    m_panel->setContentLayout(personContent);
    layout->addWidget(m_panel, 1);

    const int spacing = 2;
    auto *btnRow1 = new QHBoxLayout();
    btnRow1->setSpacing(spacing);
    auto *btnRow2 = new QHBoxLayout();
    btnRow2->setSpacing(spacing);

    auto *addBtn = new QPushButton(QString::fromUtf8("Добавить пациента"));
    addBtn->setObjectName("addPersonBtn");
    auto *shareBtn = new QPushButton(QString::fromUtf8("Поделиться"));
    shareBtn->setObjectName("sharePersonBtn");
    auto *editBtn = new QPushButton(QString::fromUtf8("Редактировать"));
    editBtn->setObjectName("editPersonBtn");
    auto *removeBtn = new QPushButton(QString::fromUtf8("Удалить пациента"));
    removeBtn->setObjectName("removePersonBtn");

    btnRow1->addWidget(addBtn);
    btnRow1->addWidget(shareBtn);
    btnRow2->addWidget(editBtn);
    btnRow2->addWidget(removeBtn);

    layout->addLayout(btnRow1);
    layout->setSpacing(spacing);
    layout->addLayout(btnRow2);

    connect(addBtn, &QPushButton::clicked, this, &PersonsWidget::addPersonClicked);
    connect(editBtn, &QPushButton::clicked, this, [this]() {
        int id = currentPersonId();
        if (id >= 0) emit editPersonClicked(id);
    });
    connect(shareBtn, &QPushButton::clicked, this, [this]() {
        int id = currentPersonId();
        if (id >= 0) emit sharePersonClicked(id, currentPersonName());
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        int id = currentPersonId();
        if (id >= 0) emit removePersonClicked(id, currentPersonName());
    });

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &PersonsWidget::load);
    m_refreshTimer->start(60000);
}

int PersonsWidget::currentPersonId() const {
    auto *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

QString PersonsWidget::currentPersonName() const {
    auto *item = m_list->currentItem();
    return item ? item->text() : QString();
}

void PersonsWidget::load() {
    int prevId = currentPersonId();

    m_api->get("/api/db/persons", [this, prevId](const ApiResult &r) {
        if (!r.ok) return;

        QJsonArray arr = r.data.array();
        m_list->blockSignals(true);
        m_list->clear();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            int id = obj.value("id").toInt();
            QString name = obj.value("name").toString();
            auto *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, id);
            m_list->addItem(item);
            if (id == prevId)
                m_list->setCurrentItem(item);
        }
        m_list->blockSignals(false);

        if (prevId >= 0 && currentPersonId() < 0)
            emit personDeselected();
    });
}

void PersonsWidget::onSelectionChanged(QListWidgetItem *current, QListWidgetItem *previous) {
    Q_UNUSED(previous);
    if (current)
        emit personSelected(current->data(Qt::UserRole).toInt(), current->text());
    else
        emit personDeselected();
}
