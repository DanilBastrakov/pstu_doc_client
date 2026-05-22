#include "collapsiblepanel.h"
#include <QHBoxLayout>

CollapsiblePanel::CollapsiblePanel(const QString &title, QPushButton *button, QWidget *parent)
    : QWidget(parent), m_title(title)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);


    auto hBox = new QHBoxLayout();
    m_headerBtn = new QPushButton();
    m_headerBtn->setCursor(Qt::PointingHandCursor);
    hBox->addWidget(m_headerBtn);
    hBox->setSpacing(2);
    hBox->addWidget(button);
    mainLayout->addLayout(hBox);

    m_content = new QWidget();
    mainLayout->addWidget(m_content, 1);

    connect(m_headerBtn, &QPushButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
    });

    setCollapsed(false);
}

void CollapsiblePanel::setContentLayout(QVBoxLayout *layout) const {
    if (m_content->layout())
        delete m_content->layout();
    m_content->setLayout(layout);
}

QVBoxLayout *CollapsiblePanel::contentLayout() const {
    return qobject_cast<QVBoxLayout *>(m_content->layout());
}

void CollapsiblePanel::setCollapsed(bool collapsed) {
    m_collapsed = collapsed;
    m_content->setVisible(!collapsed);
    QString arrow = collapsed ? QString::fromUtf8("\u25B6 ") : QString::fromUtf8("\u25BC ");
    m_headerBtn->setText(arrow + m_title);
    emit toggled(collapsed);
}

bool CollapsiblePanel::isCollapsed() const {
    return m_collapsed;
}
