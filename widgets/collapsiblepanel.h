#ifndef COLLAPSIBLEPANEL_H
#define COLLAPSIBLEPANEL_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

class CollapsiblePanel : public QWidget {
    Q_OBJECT
public:
    CollapsiblePanel(const QString &title, QPushButton *button, QWidget *parent = nullptr);

    void setContentLayout(QVBoxLayout *layout) const;
    QVBoxLayout *contentLayout() const;

    void setCollapsed(bool collapsed);
    bool isCollapsed() const;

signals:
    void toggled(bool collapsed);

private:
    QPushButton *m_headerBtn;
    QWidget *m_content;
    QString m_title;
    bool m_collapsed = false;
};

#endif
