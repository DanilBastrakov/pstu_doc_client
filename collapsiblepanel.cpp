#include "collapsiblepanel.h"
#include <QHBoxLayout>

CollapsiblePanel::CollapsiblePanel(const QString &title, QWidget *parent)
    : QWidget(parent), m_title(title)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_headerBtn = new QPushButton();
    m_headerBtn->setStyleSheet(
        "QPushButton { text-align: left; padding: 8px; font-weight: bold;"
        "  border: none; border-bottom: 1px solid #ccc; background: #f0f0f0; }"
        "QPushButton:hover { background: #e0e0e0; }"
    );
    m_headerBtn->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(m_headerBtn);

    m_content = new QWidget();
    mainLayout->addWidget(m_content);

    connect(m_headerBtn, &QPushButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
    });

    setCollapsed(false);
}

void CollapsiblePanel::setContentLayout(QVBoxLayout *layout) {
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
