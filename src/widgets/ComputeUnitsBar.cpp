#include "widgets/ComputeUnitsBar.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

ComputeUnitsBar::ComputeUnitsBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(28);
    setMinimumWidth(100);
    setMouseTracking(true);
}

ComputeUnitsBar::~ComputeUnitsBar() { delete m_popup; }

void ComputeUnitsBar::setSegments(const QList<Segment>& segments) {
    m_segments = segments;
    m_total = 0;
    for (const auto& s : m_segments) {
        m_total += s.units;
    }
    update();
}

int ComputeUnitsBar::total() const { return m_total; }

const QList<ComputeUnitsBar::Segment>& ComputeUnitsBar::segments() const { return m_segments; }

void ComputeUnitsBar::paintEvent(QPaintEvent*) {
    if (m_total == 0 || m_segments.isEmpty()) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int barH = height();
    int barW = width();
    int radius = 6;

    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, barW, barH), radius, radius);
    p.setClipPath(clip);

    double x = 0;
    for (int i = 0; i < m_segments.size(); ++i) {
        double segW = barW * m_segments[i].units / static_cast<double>(m_total);
        if (i == m_segments.size() - 1) {
            segW = barW - x;
        }
        QColor color = m_segments[i].color;
        if (i == m_hoveredIndex) {
            color = color.lighter(140);
        }
        p.fillRect(QRectF(x, 0, segW, barH), color);
        x += segW;
    }
}

void ComputeUnitsBar::mouseMoveEvent(QMouseEvent* event) {
    int idx = segmentAt(event->pos().x());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        update();
    }
    if (idx >= 0 && idx < m_segments.size()) {
        showPopup(event->globalPosition().toPoint(), m_segments[idx]);
    } else {
        hidePopup();
    }
}

void ComputeUnitsBar::leaveEvent(QEvent*) {
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        update();
    }
    hidePopup();
}

int ComputeUnitsBar::segmentAt(int x) const {
    if (m_total == 0) {
        return -1;
    }
    int barW = width();
    double px = 0;
    for (int i = 0; i < m_segments.size(); ++i) {
        double segW = barW * m_segments[i].units / static_cast<double>(m_total);
        if (i == m_segments.size() - 1) {
            segW = barW - px;
        }
        if (x >= px && x < px + segW) {
            return i;
        }
        px += segW;
    }
    return -1;
}

void ComputeUnitsBar::ensurePopup() {
    if (m_popup) {
        return;
    }

    m_popup =
        new QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_popup->setAttribute(Qt::WA_TranslucentBackground);
    m_popup->setAttribute(Qt::WA_ShowWithoutActivating);

    QVBoxLayout* outerLay = new QVBoxLayout(m_popup);
    outerLay->setContentsMargins(0, 0, 0, 0);

    QFrame* frame = new QFrame();
    frame->setObjectName("txaPopupFrame");

    QVBoxLayout* lay = new QVBoxLayout(frame);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(3);

    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    titleRow->setContentsMargins(0, 0, 0, 0);

    m_popupDot = new QLabel();
    m_popupDot->setFixedSize(12, 12);

    m_popupName = new QLabel();
    m_popupName->setObjectName("txaPopupName");

    titleRow->addWidget(m_popupDot);
    titleRow->addWidget(m_popupName);
    titleRow->addStretch();
    lay->addLayout(titleRow);

    m_popupValue = new QLabel();
    m_popupValue->setObjectName("txaPopupValue");
    lay->addWidget(m_popupValue);

    outerLay->addWidget(frame);
}

void ComputeUnitsBar::showPopup(const QPoint& globalPos, const Segment& seg) {
    ensurePopup();

    m_popupDot->setStyleSheet(
        QString("background: %1; border-radius: 3px; border: none;").arg(seg.color.name()));
    m_popupName->setText(seg.label);
    m_popupValue->setText(QLocale().toString(seg.units) + " CU consumed");

    m_popup->adjustSize();

    int tipW = m_popup->width();
    int tipH = m_popup->height();
    m_popup->move(globalPos.x() - tipW / 2, globalPos.y() - tipH - 14);
    m_popup->show();
}

void ComputeUnitsBar::hidePopup() {
    if (m_popup) {
        m_popup->hide();
    }
}
