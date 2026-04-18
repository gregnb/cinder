#include "TpsBarChart.h"
#include <QDateTime>
#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <cmath>

static double niceNum(double range, bool round) {
    double exponent = std::floor(std::log10(range));
    double fraction = range / std::pow(10.0, exponent);
    double nice;
    if (round) {
        if (fraction < 1.5) {
            nice = 1;
        } else if (fraction < 3) {
            nice = 2;
        } else if (fraction < 7) {
            nice = 5;
        } else {
            nice = 10;
        }
    } else {
        if (fraction <= 1) {
            nice = 1;
        } else if (fraction <= 2) {
            nice = 2;
        } else if (fraction <= 5) {
            nice = 5;
        } else {
            nice = 10;
        }
    }
    return nice * std::pow(10.0, exponent);
}

TpsBarChart::TpsBarChart(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(80);

    // Floating tooltip — a top-level frameless widget that can extend beyond chart bounds
    m_tooltip = new QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
    m_tooltip->setAttribute(Qt::WA_TranslucentBackground);
    m_tooltip->setAttribute(Qt::WA_ShowWithoutActivating);
    m_tooltip->setStyleSheet("background: transparent;");

    m_tooltipLabel = new QLabel(m_tooltip);
    m_tooltipLabel->setStyleSheet("QLabel {"
                                  "  background: rgba(30, 25, 55, 230);"
                                  "  border: 1px solid rgba(100, 100, 150, 80);"
                                  "  border-radius: 8px;"
                                  "  padding: 8px 12px;"
                                  "  color: white;"
                                  "  font-size: 11px;"
                                  "}");

    auto* tooltipLayout = new QVBoxLayout(m_tooltip);
    tooltipLayout->setContentsMargins(0, 0, 0, 0);
    tooltipLayout->addWidget(m_tooltipLabel);
}

TpsBarChart::~TpsBarChart() { delete m_tooltip; }

void TpsBarChart::setData(const QList<TpsSample>& samples) {
    m_samples = samples;
    computeYTicks();
    computeXTicks();
    update();
}

int TpsBarChart::chartLeft() const { return m_leftMargin; }
int TpsBarChart::chartWidth() const { return width() - m_leftMargin - 8; }
int TpsBarChart::chartHeight() const { return height() - m_bottomMargin - 4; }
int TpsBarChart::chartTop() const { return 4; }

void TpsBarChart::computeYTicks() {
    m_yTicks.clear();
    if (m_samples.isEmpty()) {
        m_yMax = 1;
        m_leftMargin = 0;
        return;
    }

    double maxTps = 0;
    for (const auto& s : m_samples) {
        maxTps = qMax(maxTps, s.totalTps);
    }
    if (maxTps <= 0) {
        maxTps = 1;
    }

    double range = niceNum(maxTps, false);
    double tickSpacing = niceNum(range / 4, true);
    m_yMax = std::ceil(maxTps / tickSpacing) * tickSpacing;

    m_yTickDecimals = 0;
    if (tickSpacing < 1) {
        m_yTickDecimals = 1;
    }

    for (double v = 0; v <= m_yMax + tickSpacing * 0.5; v += tickSpacing) {
        m_yTicks.append(v);
    }

    // Compute left margin from widest label
    QFont tickFont = font();
    tickFont.setPixelSize(10);
    QFontMetrics fm(tickFont);
    int maxW = 0;
    for (double v : m_yTicks) {
        QString label;
        if (v >= 1000) {
            label = QString::number(static_cast<int>(v / 1000)) + "k";
        } else {
            label = QString::number(v, 'f', m_yTickDecimals);
        }
        maxW = qMax(maxW, fm.horizontalAdvance(label));
    }
    m_leftMargin = maxW + 8;
}

void TpsBarChart::computeXTicks() {
    m_xTicks.clear();
    m_bottomMargin = 0;
    if (m_samples.size() < 2) {
        return;
    }

    double tMin = m_samples.first().timestamp;
    double tMax = m_samples.last().timestamp;
    double span = tMax - tMin;
    if (span <= 0) {
        return;
    }

    m_bottomMargin = 20;

    double interval;
    if (span < 600) {
        interval = 120; // 2 min
        m_xTickFormat = "HH:mm";
    } else if (span < 3600) {
        interval = 600; // 10 min
        m_xTickFormat = "HH:mm";
    } else if (span < 6 * 3600) {
        interval = 3600;
        m_xTickFormat = "HH:mm";
    } else if (span < 2 * 86400) {
        interval = 4 * 3600;
        m_xTickFormat = "HH:mm";
    } else {
        interval = 86400;
        m_xTickFormat = "MMM d";
    }

    double first = std::ceil(tMin / interval) * interval;
    for (double t = first; t <= tMax; t += interval) {
        m_xTicks.append(t);
    }
    while (m_xTicks.size() > 6) {
        QList<double> reduced;
        for (int i = 0; i < m_xTicks.size(); i += 2) {
            reduced.append(m_xTicks[i]);
        }
        m_xTicks = reduced;
    }
}

void TpsBarChart::showTooltip(const TpsSample& s) {
    QLocale loc(QLocale::English);

    QString timeStr = QDateTime::fromSecsSinceEpoch(s.timestamp).toString("MMM d, HH:mm");
    QString totalStr = loc.toString(static_cast<int>(s.totalTps));
    double nonVotePct = s.totalTps > 0 ? s.nonVoteTps / s.totalTps * 100.0 : 0;
    double votePct = s.totalTps > 0 ? s.voteTps / s.totalTps * 100.0 : 0;
    QString nonVoteStr = loc.toString(static_cast<int>(s.nonVoteTps)) + " (" +
                         QString::number(nonVotePct, 'f', 1) + "%)";
    QString voteStr =
        loc.toString(static_cast<int>(s.voteTps)) + " (" + QString::number(votePct, 'f', 1) + "%)";

    // Rich text with color-coded lines
    QString html = QStringLiteral("<div style='white-space:nowrap;'>"
                                  "<b style='color:rgba(255,255,255,0.8);'>%1</b><br>"
                                  "<span style='color:rgba(255,255,255,0.5);'>Total TPS: </span>"
                                  "<b style='color:rgba(255,255,255,0.9);'>%2</b><br>"
                                  "<span style='color:rgba(255,255,255,0.5);'>True TPS: </span>"
                                  "<b style='color:#a78bfa;'>%3</b><br>"
                                  "<span style='color:rgba(255,255,255,0.5);'>Vote TPS: </span>"
                                  "<b style='color:#10b981;'>%4</b>"
                                  "</div>")
                       .arg(timeStr, totalStr, nonVoteStr, voteStr);

    m_tooltipLabel->setText(html);
    m_tooltip->adjustSize();

    // Position in screen coordinates: above the bar, centered on cursor
    QPoint globalMouse = mapToGlobal(m_mousePos);
    int tw = m_tooltip->width();
    int th = m_tooltip->height();

    int tx = globalMouse.x() - tw / 2;
    int ty = globalMouse.y() - th - 12;

    // If above would go off-screen top, show below cursor instead
    if (ty < 0) {
        ty = globalMouse.y() + 16;
    }

    m_tooltip->move(tx, ty);
    m_tooltip->show();
}

void TpsBarChart::hideTooltip() { m_tooltip->hide(); }

void TpsBarChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int cl = chartLeft();
    int cw = chartWidth();
    int ch = chartHeight();
    int ct = chartTop();

    if (m_samples.isEmpty() || cw <= 0 || ch <= 0) {
        return;
    }

    // Y-axis ticks and grid lines
    QFont tickFont = font();
    tickFont.setPixelSize(10);
    p.setFont(tickFont);

    for (double v : m_yTicks) {
        double yFrac = (m_yMax > 0) ? v / m_yMax : 0;
        int y = ct + ch - static_cast<int>(yFrac * ch);

        // Grid line
        p.setPen(QPen(QColor(255, 255, 255, 20), 1));
        p.drawLine(cl, y, cl + cw, y);

        // Label
        QString label;
        if (v >= 1000) {
            label = QString::number(static_cast<int>(v / 1000)) + "k";
        } else {
            label = QString::number(v, 'f', m_yTickDecimals);
        }
        p.setPen(QColor(255, 255, 255, 100));
        p.drawText(0, y - 6, cl - 4, 12, Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // X-axis time labels
    if (!m_xTicks.isEmpty() && m_samples.size() >= 2) {
        double tMin = m_samples.first().timestamp;
        double tMax = m_samples.last().timestamp;
        double tRange = tMax - tMin;

        p.setPen(QColor(255, 255, 255, 100));
        p.setFont(tickFont);
        QFontMetrics fm(tickFont);
        for (double t : m_xTicks) {
            double frac = (tRange > 0) ? (t - tMin) / tRange : 0;
            int x = cl + static_cast<int>(frac * cw);
            QString label =
                QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t)).toString(m_xTickFormat);
            int tw = fm.horizontalAdvance(label);
            p.drawText(x - tw / 2, ct + ch + 2, tw, 16, Qt::AlignCenter, label);
        }
    }

    // Draw bars
    int n = m_samples.size();
    double barWidth = static_cast<double>(cw) / n;
    double gap = qMax(1.0, barWidth * 0.15);
    double actualBarW = barWidth - gap;
    if (actualBarW < 1) {
        actualBarW = 1;
    }

    for (int i = 0; i < n; ++i) {
        const TpsSample& s = m_samples[i];
        double totalFrac = (m_yMax > 0) ? s.totalTps / m_yMax : 0;
        double voteFrac = (m_yMax > 0) ? s.voteTps / m_yMax : 0;

        int barX = cl + static_cast<int>(i * barWidth + gap / 2);
        int totalH = static_cast<int>(totalFrac * ch);
        int voteH = static_cast<int>(voteFrac * ch);
        int barBottom = ct + ch;

        // Vote TPS (bottom portion, green)
        if (voteH > 0) {
            QColor voteColor = m_voteTpsColor;
            if (m_hovered && m_hoveredBar == i) {
                voteColor = voteColor.lighter(130);
            }
            p.fillRect(QRectF(barX, barBottom - voteH, actualBarW, voteH), voteColor);
        }

        // Non-vote TPS (top portion, purple)
        int nonVoteH = totalH - voteH;
        if (nonVoteH > 0) {
            QColor nonVoteColor = m_nonVoteTpsColor;
            if (m_hovered && m_hoveredBar == i) {
                nonVoteColor = nonVoteColor.lighter(130);
            }
            p.fillRect(QRectF(barX, barBottom - totalH, actualBarW, nonVoteH), nonVoteColor);
        }
    }
}

void TpsBarChart::repositionTooltip() {
    if (!m_tooltip->isVisible()) {
        return;
    }
    QPoint globalMouse = mapToGlobal(m_mousePos);
    int tw = m_tooltip->width();
    int th = m_tooltip->height();
    int tx = globalMouse.x() - tw / 2;
    int ty = globalMouse.y() - th - 12;
    if (ty < 0) {
        ty = globalMouse.y() + 16;
    }
    m_tooltip->move(tx, ty);
}

void TpsBarChart::mouseMoveEvent(QMouseEvent* event) {
    m_mousePos = event->pos();
    int cl = chartLeft();
    int cw = chartWidth();
    int n = m_samples.size();

    if (n > 0 && cw > 0) {
        double barWidth = static_cast<double>(cw) / n;
        int idx = static_cast<int>((event->pos().x() - cl) / barWidth);
        if (idx >= 0 && idx < n) {
            bool changed = !m_hovered || m_hoveredBar != idx;
            m_hovered = true;
            m_hoveredBar = idx;
            if (changed) {
                showTooltip(m_samples[idx]);
                update();
            } else {
                // Same bar — just reposition tooltip, skip HTML rebuild
                repositionTooltip();
            }
            return;
        }
    }

    if (m_hovered) {
        m_hovered = false;
        m_hoveredBar = -1;
        hideTooltip();
        update();
    }
}

void TpsBarChart::leaveEvent(QEvent*) {
    m_hovered = false;
    m_hoveredBar = -1;
    hideTooltip();
    update();
}
