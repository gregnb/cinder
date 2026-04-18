#include "TabBar.h"
#include "Theme.h"
#include <QEvent>
#include <QMouseEvent>

TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Reserve space for the underline indicator
    setMinimumHeight(42);

    m_animX = new QPropertyAnimation(this, "indicatorX", this);
    m_animX->setDuration(200);
    m_animX->setEasingCurve(QEasingCurve::OutCubic);

    m_animW = new QPropertyAnimation(this, "indicatorWidth", this);
    m_animW->setDuration(200);
    m_animW->setEasingCurve(QEasingCurve::OutCubic);
}

void TabBar::addTab(const QString& text) {
    QLabel* tab = new QLabel(text);
    tab->setAlignment(Qt::AlignCenter);
    tab->setCursor(Qt::PointingHandCursor);
    tab->setContentsMargins(16, 8, 16, 10);
    tab->installEventFilter(this);

    m_tabs.append(tab);
    m_layout->addWidget(tab, m_stretch ? 1 : 0);
}

void TabBar::setTabText(int index, const QString& text) {
    if (index >= 0 && index < m_tabs.size()) {
        m_tabs[index]->setText(text);
    }
}

void TabBar::setActiveIndex(int index) {
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }

    m_activeIndex = index;
    updateStyles();
    animateIndicator(index);
    emit currentChanged(index);
}

void TabBar::setStretch(bool stretch) {
    m_stretch = stretch;
    for (int i = 0; i < m_tabs.size(); ++i) {
        m_layout->setStretch(i, stretch ? 1 : 0);
    }
}

void TabBar::updateStyles() {
    for (int i = 0; i < m_tabs.size(); ++i) {
        bool active = (i == m_activeIndex);
        bool hovered = (i == m_hoveredIndex && !active);

        QString color;
        QString weight;
        if (active) {
            color = Theme::textPrimary;
            weight = QStringLiteral("700");
        } else if (hovered) {
            color = QStringLiteral("rgba(255, 255, 255, 0.55)");
            weight = QStringLiteral("600");
        } else {
            color = QStringLiteral("rgba(255, 255, 255, 0.35)");
            weight = QStringLiteral("600");
        }

        m_tabs[i]->setStyleSheet(
            QString("color: %1; font-family: %2; font-size: 14px; font-weight: %3; "
                    "background: transparent; border: none;")
                .arg(color, Theme::fontFamily, weight));
    }
}

void TabBar::animateIndicator(int index) {
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }

    QLabel* tab = m_tabs[index];
    qreal targetX = tab->x();
    qreal targetW = tab->width();

    // First time — snap instead of animate
    if (m_indicatorW == 0) {
        m_indicatorX = targetX;
        m_indicatorW = targetW;
        update();
        return;
    }

    m_animX->stop();
    m_animX->setStartValue(m_indicatorX);
    m_animX->setEndValue(targetX);
    m_animX->start();

    m_animW->stop();
    m_animW->setStartValue(m_indicatorW);
    m_animW->setEndValue(targetW);
    m_animW->start();
}

void TabBar::setIndicatorX(qreal x) {
    m_indicatorX = x;
    update();
}

void TabBar::setIndicatorWidth(qreal w) {
    m_indicatorW = w;
    update();
}

void TabBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Re-snap indicator to active tab after layout settles geometry
    if (m_activeIndex >= 0 && m_activeIndex < m_tabs.size()) {
        QLabel* tab = m_tabs[m_activeIndex];
        m_indicatorX = tab->x();
        m_indicatorW = tab->width();
        m_animX->stop();
        m_animW->stop();
        update();
    }
}

void TabBar::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    if (m_activeIndex < 0 || m_indicatorW <= 0) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Subtle separator line across full width
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(100, 100, 150, 40));
    p.drawRect(QRectF(0, height() - 1, width(), 1));

    // Active indicator — rounded pill underline
    qreal indicatorH = 3;
    qreal inset = 12; // inset from tab edges for a shorter bar
    QRectF rect(m_indicatorX + inset, height() - indicatorH, m_indicatorW - inset * 2, indicatorH);

    QLinearGradient grad(rect.left(), 0, rect.right(), 0);
    grad.setColorAt(0, QColor("#3B82F6"));
    grad.setColorAt(1, QColor("#9945FF"));
    p.setBrush(grad);
    p.drawRoundedRect(rect, 1.5, 1.5);
}

bool TabBar::eventFilter(QObject* obj, QEvent* event) {
    int index = -1;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i] == obj) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return QWidget::eventFilter(obj, event);
    }

    switch (event->type()) {
        case QEvent::MouseButtonRelease:
            if (index != m_activeIndex) {
                setActiveIndex(index);
            }
            return true;

        case QEvent::Enter:
            m_hoveredIndex = index;
            updateStyles();
            return false;

        case QEvent::Leave:
            m_hoveredIndex = -1;
            updateStyles();
            return false;

        default:
            break;
    }

    return QWidget::eventFilter(obj, event);
}
