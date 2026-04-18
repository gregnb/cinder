#include "StyledCalendar.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHeaderView>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QTextCharFormat>
#include <QToolButton>
#include <QtSvg/QSvgRenderer>

// Colors matching Dropdown widget palette
static const QColor kBg(0x16, 0x1a, 0x30);       // #161a30
static const QColor kNavBg(0x1e, 0x1f, 0x37);    // rgba(30,31,55)
static const QColor kBorder(0x2a, 0x3a, 0x5a);   // #2a3a5a
static const QColor kHover(0x25, 0x30, 0x50);    // #253050
static const QColor kSelected(0x2a, 0x5a, 0x8a); // #2a5a8a (blue accent)
static const QColor kTextNormal(255, 255, 255, 190);
static const QColor kTextDim(255, 255, 255, 50);
static const QColor kAccent(0x3b, 0x82, 0xf6); // #3b82f6 (blue)
static const QColor kAccentBorder(100, 150, 255, 130);

StyledCalendar::StyledCalendar(QWidget* parent) : QCalendarWidget(parent) {
    setObjectName("styledCalendar");
    setGridVisible(false);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    // Force dark background via QPalette on the entire widget tree.
    // This is the nuclear option — QTableView reads QPalette::Base for its
    // cell background, and no amount of QSS on the viewport overrides it.
    QPalette pal = palette();
    pal.setColor(QPalette::Base, kBg);
    pal.setColor(QPalette::Window, kBg);
    pal.setColor(QPalette::AlternateBase, kBg);
    pal.setColor(QPalette::Text, kTextNormal);
    pal.setColor(QPalette::WindowText, Qt::white);
    pal.setColor(QPalette::Highlight, kSelected);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(pal);

    // ── Navigation bar ──
    if (auto* nav = findChild<QWidget*>("qt_calendar_navigationbar")) {
        nav->setObjectName("styledCalendarNavBar");
        nav->setAttribute(Qt::WA_StyledBackground, true);
        nav->setFixedHeight(38);
    }

    // Style month/year buttons
    for (auto* btn : findChildren<QToolButton*>()) {
        if (btn->objectName() != "qt_calendar_prevmonth" &&
            btn->objectName() != "qt_calendar_nextmonth") {
            btn->setProperty("uiClass", "styledCalendarNavButton");
        }
    }

    // Replace nav arrows with SVG icons
    auto loadArrowIcon = [](const QString& svgPath, const QColor& tint) {
        qreal dpr = qApp->devicePixelRatio();
        int px = static_cast<int>(14 * dpr);
        QSvgRenderer renderer(svgPath);
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            renderer.render(&p);
        }
        if (tint.isValid()) {
            QPainter p(&pm);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pm.rect(), tint);
        }
        pm.setDevicePixelRatio(dpr);
        return QIcon(pm);
    };

    QColor arrowTint(255, 255, 255, 160);
    if (auto* prev = findChild<QToolButton*>("qt_calendar_prevmonth")) {
        prev->setText({});
        prev->setProperty("uiClass", "styledCalendarArrowButton");
        prev->setIcon(loadArrowIcon(":/icons/tx-type/cal-prev.svg", arrowTint));
        prev->setIconSize(QSize(14, 14));
    }
    if (auto* next = findChild<QToolButton*>("qt_calendar_nextmonth")) {
        next->setText({});
        next->setProperty("uiClass", "styledCalendarArrowButton");
        next->setIcon(loadArrowIcon(":/icons/tx-type/cal-next.svg", arrowTint));
        next->setIconSize(QSize(14, 14));
    }

    // ── Day grid (QTableView) ──
    if (auto* view = findChild<QAbstractItemView*>()) {
        m_view = view;

        // Set palette on the view and its viewport too
        view->setPalette(pal);
        view->viewport()->setPalette(pal);
        view->setObjectName("styledCalendarView");
        view->viewport()->setObjectName("styledCalendarViewport");
        view->viewport()->setAttribute(Qt::WA_StyledBackground, true);

        // Hover tracking for per-cell highlight
        view->viewport()->setMouseTracking(true);
        view->viewport()->setCursor(Qt::PointingHandCursor);
        view->viewport()->installEventFilter(this);

        // Day-of-week header row
        if (auto* hdr = view->findChild<QHeaderView*>()) {
            QPalette hdrPal = hdr->palette();
            hdrPal.setColor(QPalette::Window, kNavBg);
            hdrPal.setColor(QPalette::Base, kNavBg);
            hdr->setPalette(hdrPal);
            hdr->setObjectName("styledCalendarHeader");
            hdr->viewport()->setObjectName("styledCalendarHeaderViewport");
            hdr->viewport()->setAttribute(Qt::WA_StyledBackground, true);
        }
    }

    // Remove weekend coloring — paintCell handles all colors
    QTextCharFormat fmt;
    fmt.setForeground(kTextNormal);
    setWeekdayTextFormat(Qt::Saturday, fmt);
    setWeekdayTextFormat(Qt::Sunday, fmt);

    // Outer border only — no background rule here to avoid cascade conflicts
}

void StyledCalendar::paintCell(QPainter* painter, const QRect& rect, QDate date) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    bool isToday = (date == QDate::currentDate());
    bool isSelected = (date == selectedDate());
    bool isCurrentMonth = (date.month() == monthShown() && date.year() == yearShown());

    // Fill entire cell with dark bg first — ensures no white leaks through
    painter->fillRect(rect, kBg);

    // Inset rect for the cell shape
    QRect cell = rect.adjusted(2, 2, -2, -2);

    bool isHovered = (date == m_hoveredDate);

    // ── Background ──
    if (isSelected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(kSelected);
        painter->drawRoundedRect(cell, 6, 6);
    } else if (isHovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(kHover);
        painter->drawRoundedRect(cell, 6, 6);
    } else if (isToday) {
        painter->setPen(QPen(kAccentBorder, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(cell, 6, 6);
    }

    // ── Text ──
    QColor textColor;
    if (isSelected) {
        textColor = Qt::white;
    } else if (!isCurrentMonth) {
        textColor = kTextDim;
    } else if (isToday) {
        textColor = kAccent;
    } else {
        textColor = kTextNormal;
    }

    painter->setPen(textColor);
    QFont font = painter->font();
    font.setPixelSize(12);
    font.setWeight((isToday || isSelected) ? QFont::DemiBold : QFont::Normal);
    painter->setFont(font);
    painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));

    painter->restore();
}

QDate StyledCalendar::dateForIndex(const QModelIndex& idx) const {
    QDate firstOfMonth(yearShown(), monthShown(), 1);
    int fdow = static_cast<int>(firstDayOfWeek()); // Qt::DayOfWeek: 1=Mon, 7=Sun
    int monthStartDow = firstOfMonth.dayOfWeek();
    int offset = (monthStartDow - fdow + 7) % 7;
    QDate gridStart = firstOfMonth.addDays(-offset);
    return gridStart.addDays(idx.row() * 7 + idx.column());
}

bool StyledCalendar::eventFilter(QObject* obj, QEvent* event) {
    if (m_view && obj == m_view->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            QModelIndex idx = m_view->indexAt(me->pos());
            QDate newDate = idx.isValid() ? dateForIndex(idx) : QDate();
            if (newDate != m_hoveredDate) {
                m_hoveredDate = newDate;
                m_view->viewport()->update();
            }
        } else if (event->type() == QEvent::Leave) {
            if (m_hoveredDate.isValid()) {
                m_hoveredDate = QDate();
                m_view->viewport()->update();
            }
        }
    }
    return QCalendarWidget::eventFilter(obj, event);
}
