#ifndef PILLBUTTONGROUP_H
#define PILLBUTTONGROUP_H

#include <QHBoxLayout>
#include <QList>
#include <QPushButton>
#include <QStyle>
#include <QWidget>

// Reusable horizontal group of mutually-exclusive pill buttons.
// Buttons are styled via object names so state stays aligned with the theme
// system instead of page-local stylesheet strings.

class PillButtonGroup : public QWidget {
    Q_OBJECT

  public:
    explicit PillButtonGroup(QWidget* parent = nullptr) : QWidget(parent) {
        m_layout = new QHBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(8);
    }

    // Add buttons by label. Call setActiveIndex() after adding all.
    void addButton(const QString& text, int stretch = 0) {
        QPushButton* btn = new QPushButton(text);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(38);

        int index = m_buttons.size();
        m_buttons.append(btn);
        m_layout->addWidget(btn, stretch);

        connect(btn, &QPushButton::clicked, this, [this, index]() { setActiveIndex(index); });
    }

    void setActiveIndex(int index) {
        if (index < 0 || index >= m_buttons.size()) {
            return;
        }

        m_activeIndex = index;

        for (int i = 0; i < m_buttons.size(); ++i) {
            m_buttons[i]->setChecked(i == index);
            m_buttons[i]->setObjectName(i == index ? m_activeObjectName : m_inactiveObjectName);
            m_buttons[i]->style()->unpolish(m_buttons[i]);
            m_buttons[i]->style()->polish(m_buttons[i]);
        }

        emit currentChanged(index);
    }

    int activeIndex() const { return m_activeIndex; }
    int count() const { return m_buttons.size(); }
    QPushButton* button(int index) const { return m_buttons.value(index); }

    void setObjectNames(const QString& inactive, const QString& active) {
        m_inactiveObjectName = inactive;
        m_activeObjectName = active;
    }

  signals:
    void currentChanged(int index);

  private:
    QHBoxLayout* m_layout = nullptr;
    QList<QPushButton*> m_buttons;
    int m_activeIndex = -1;

    QString m_inactiveObjectName = "speedButton";
    QString m_activeObjectName = "speedButtonActive";
};

#endif // PILLBUTTONGROUP_H
