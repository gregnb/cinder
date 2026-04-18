#include "SplineChart.h"
#include "Theme.h"
#include <QDateTime>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <cmath>

SplineChart::SplineChart(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    setFixedHeight(130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Default fill gradient (purple, fading down)
    m_fillStops = {
        {0.0, QColor(130, 80, 230, 60)},
        {0.4, QColor(110, 60, 210, 35)},
        {0.7, QColor(90, 40, 180, 18)},
        {1.0, QColor(70, 30, 160, 8)},
    };

    // Default tooltip: just show Y with 2 decimals
    m_valueFormatter = [](double, double y) { return QString::number(y, 'f', 2); };

    // Defer glow rebuild so rapid resizes (sidebar animation) don't
    // trigger expensive blur on every frame
    m_glowTimer.setSingleShot(true);
    m_glowTimer.setInterval(150);
    connect(&m_glowTimer, &QTimer::timeout, this, [this]() {
        rebuildCache();
        update();
    });
}

// ── Public API ──────────────────────────────────────────────────

void SplineChart::setData(const QList<QPointF>& points) {
    m_data = points;
    recomputeRange();
    m_glowValid = false;
    m_pathValid = false;
    m_glowTimer.start();
    update();
}

void SplineChart::setLineColor(const QColor& color) {
    m_lineColor = color;
    m_glowValid = false;
    update();
}

void SplineChart::setGlowColor(const QColor& color) {
    m_glowColor = color;
    m_glowValid = false;
    m_glowTimer.start();
    update();
}

void SplineChart::setFillGradient(const QList<QPair<qreal, QColor>>& stops) {
    m_fillStops = stops;
    update();
}

void SplineChart::setValueFormatter(std::function<QString(double, double)> formatter) {
    m_valueFormatter = std::move(formatter);
}

void SplineChart::setYLabelFormatter(std::function<QString(double)> formatter) {
    m_yLabelFormatter = std::move(formatter);
}

void SplineChart::setYFloor(double floor) {
    m_hasYFloor = true;
    m_yFloor = floor;
}

void SplineChart::setShowYAxis(bool show) {
    m_showYAxis = show;
    computeYTicks();
    m_glowValid = false;
    m_glowTimer.start();
    update();
}

void SplineChart::setShowXAxis(bool show) {
    m_showXAxis = show;
    computeXTicks();
    m_glowValid = false;
    m_glowTimer.start();
    update();
}

void SplineChart::setBottomCornerRadius(int radius) {
    m_bottomCornerRadius = radius;
    update();
}

// ── Data range ──────────────────────────────────────────────────

void SplineChart::recomputeRange() {
    if (m_data.isEmpty()) {
        return;
    }

    m_xMin = m_data.first().x();
    m_xMax = m_data.first().x();
    m_yMin = m_data.first().y();
    m_yMax = m_data.first().y();

    for (const auto& pt : m_data) {
        m_xMin = qMin(m_xMin, pt.x());
        m_xMax = qMax(m_xMax, pt.x());
        m_yMin = qMin(m_yMin, pt.y());
        m_yMax = qMax(m_yMax, pt.y());
    }

    // If a Y floor is set, ensure it's included in the range
    if (m_hasYFloor && m_yMin > m_yFloor) {
        m_yMin = m_yFloor;
    }

    // Asymmetric vertical padding — more space below the line
    // so the curve sits in the upper portion of the widget
    double range = m_yMax - m_yMin;
    m_yMax += range * 0.05;
    m_yMin -= range * 0.55;

    // Guard against flat data
    if (qFuzzyCompare(m_xMax, m_xMin)) {
        m_xMax = m_xMin + 1.0;
    }
    if (qFuzzyCompare(m_yMax, m_yMin)) {
        m_yMax = m_yMin + 1.0;
    }

    computeYTicks();
    computeXTicks();
}

// ── Y-axis ticks ────────────────────────────────────────────────

static double niceNum(double range, bool round) {
    double exponent = floor(log10(range));
    double fraction = range / pow(10, exponent);
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
    return nice * pow(10, exponent);
}

void SplineChart::computeYTicks() {
    m_yTicks.clear();
    m_leftMargin = 0;
    if (!m_showYAxis || m_data.size() < 2) {
        return;
    }

    // Compute raw data range (before padding)
    double dataYMin = m_data.first().y();
    double dataYMax = m_data.first().y();
    for (const auto& pt : m_data) {
        dataYMin = qMin(dataYMin, pt.y());
        dataYMax = qMax(dataYMax, pt.y());
    }
    if (m_hasYFloor && dataYMin > m_yFloor) {
        dataYMin = m_yFloor;
    }

    double range = dataYMax - dataYMin;
    if (range < 1e-15) {
        range = qAbs(dataYMax) > 0 ? qAbs(dataYMax) * 0.1 : 1.0;
    }

    double tickSpacing = niceNum(range / 3.0, true);
    double tickMin = floor(dataYMin / tickSpacing) * tickSpacing;
    double tickMax = ceil(dataYMax / tickSpacing) * tickSpacing;

    for (double v = tickMin; v <= tickMax + tickSpacing * 0.01; v += tickSpacing) {
        if (v >= m_yMin && v <= m_yMax) {
            m_yTicks.append(v);
        }
    }

    // Decimal places from tick spacing
    m_yTickDecimals = qMax(0, static_cast<int>(-floor(log10(tickSpacing))) + 0);

    // Compute left margin from widest label
    QFont font(Theme::fontFamily, 9);
    QFontMetrics fm(font);
    int maxW = 0;
    for (double v : m_yTicks) {
        QString label =
            m_yLabelFormatter ? m_yLabelFormatter(v) : QString::number(v, 'f', m_yTickDecimals);
        maxW = qMax(maxW, fm.horizontalAdvance(label));
    }
    m_leftMargin = maxW + 10; // 10px padding right of labels
}

int SplineChart::chartLeft() const { return m_showYAxis ? m_leftMargin : 0; }

int SplineChart::chartWidth() const { return width() - chartLeft() - m_rightMargin; }

int SplineChart::chartHeight() const { return height() - m_bottomMargin; }

void SplineChart::computeXTicks() {
    m_xTicks.clear();
    m_bottomMargin = 0;
    if (!m_showXAxis || m_data.size() < 2) {
        return;
    }

    double span = m_xMax - m_xMin;
    if (span <= 0) {
        return;
    }

    // Pick tick interval and format based on time span
    double tickInterval;
    if (span < 3600 * 6) {
        tickInterval = 3600;
        m_xTickFormat = "HH:mm";
    } else if (span < 86400 * 2) {
        tickInterval = 3600 * 4;
        m_xTickFormat = "HH:mm";
    } else if (span < 86400 * 14) {
        tickInterval = 86400;
        m_xTickFormat = "MMM d";
    } else if (span < 86400 * 90) {
        tickInterval = 86400 * 7;
        m_xTickFormat = "MMM d";
    } else {
        tickInterval = 86400 * 30;
        m_xTickFormat = "MMM";
    }

    // Generate ticks at round boundaries
    double start = ceil(m_xMin / tickInterval) * tickInterval;
    for (double t = start; t <= m_xMax; t += tickInterval) {
        m_xTicks.append(t);
    }

    // Thin out if too many labels
    while (m_xTicks.size() > 7) {
        QList<double> reduced;
        for (int i = 0; i < m_xTicks.size(); i += 2) {
            reduced.append(m_xTicks[i]);
        }
        m_xTicks = reduced;
    }

    QFont font(Theme::fontFamily, 9);
    QFontMetrics fm(font);
    m_bottomMargin = fm.height() + 6;
}

// ── Coordinate mapping ──────────────────────────────────────────

QPointF SplineChart::dataToPixel(const QPointF& p, int w, int h) const {
    double px = (p.x() - m_xMin) / (m_xMax - m_xMin) * w;
    double py = h - (p.y() - m_yMin) / (m_yMax - m_yMin) * h;
    return {px, py};
}

// ── Catmull-Rom → QPainterPath ──────────────────────────────────

QPainterPath SplineChart::buildSplinePath(const QList<QPointF>& pts, int w, int h) const {
    if (pts.size() < 2) {
        return {};
    }

    QPainterPath path;
    path.moveTo(dataToPixel(pts[0], w, h));

    for (int i = 0; i < pts.size() - 1; ++i) {
        QPointF P0 = dataToPixel(pts[qMax(0, i - 1)], w, h);
        QPointF P1 = dataToPixel(pts[i], w, h);
        QPointF P2 = dataToPixel(pts[qMin(i + 1, pts.size() - 1)], w, h);
        QPointF P3 = dataToPixel(pts[qMin(i + 2, pts.size() - 1)], w, h);

        double tension = 6.0;
        QPointF cp1 = P1 + (P2 - P0) / tension;
        QPointF cp2 = P2 - (P3 - P1) / tension;

        path.cubicTo(cp1, cp2, P2);
    }
    return path;
}

// ── Catmull-Rom interpolation at arbitrary x ─────────────────────

double SplineChart::interpolateY(double x) const {
    if (m_data.isEmpty()) {
        return 0;
    }
    if (x <= m_data.first().x()) {
        return m_data.first().y();
    }
    if (x >= m_data.last().x()) {
        return m_data.last().y();
    }

    // Find the segment
    int seg = 0;
    for (int i = 0; i < m_data.size() - 1; ++i) {
        if (x >= m_data[i].x() && x <= m_data[i + 1].x()) {
            seg = i;
            break;
        }
    }

    QPointF P0 = m_data[qMax(0, seg - 1)];
    QPointF P1 = m_data[seg];
    QPointF P2 = m_data[qMin(seg + 1, m_data.size() - 1)];
    QPointF P3 = m_data[qMin(seg + 2, m_data.size() - 1)];

    double t = (x - P1.x()) / (P2.x() - P1.x());

    // Catmull-Rom formula
    double t2 = t * t, t3 = t2 * t;
    double y = 0.5 * ((2.0 * P1.y()) + (-P0.y() + P2.y()) * t +
                      (2.0 * P0.y() - 5.0 * P1.y() + 4.0 * P2.y() - P3.y()) * t2 +
                      (-P0.y() + 3.0 * P1.y() - 3.0 * P2.y() + P3.y()) * t3);
    return y;
}

// ── Box blur (3-pass for gaussian approximation) ────────────────
// Uses direct scanline access for performance — avoids per-pixel
// QImage::pixel()/setPixel() overhead.

QImage SplineChart::blurImage(const QImage& src, int radius) const {
    if (radius < 1) {
        return src;
    }

    QImage img = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int w = img.width(), h = img.height();
    QImage buf(w, h, QImage::Format_ARGB32_Premultiplied);

    int diam = radius * 2 + 1;

    for (int pass = 0; pass < 3; ++pass) {
        // Horizontal pass: img → buf
        for (int y = 0; y < h; ++y) {
            const QRgb* srcLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            QRgb* dstLine = reinterpret_cast<QRgb*>(buf.scanLine(y));

            int r = 0, g = 0, b = 0, a = 0;

            // Seed the accumulator with the first (radius+1) pixels, mirroring at edges
            for (int dx = -radius; dx <= radius; ++dx) {
                int nx = qBound(0, dx, w - 1);
                QRgb px = srcLine[nx];
                a += qAlpha(px);
                r += qRed(px);
                g += qGreen(px);
                b += qBlue(px);
            }
            dstLine[0] = qRgba(r / diam, g / diam, b / diam, a / diam);

            // Slide the window across
            for (int x = 1; x < w; ++x) {
                int addIdx = qMin(x + radius, w - 1);
                int subIdx = qMax(x - radius - 1, 0);
                QRgb addPx = srcLine[addIdx];
                QRgb subPx = srcLine[subIdx];
                a += qAlpha(addPx) - qAlpha(subPx);
                r += qRed(addPx) - qRed(subPx);
                g += qGreen(addPx) - qGreen(subPx);
                b += qBlue(addPx) - qBlue(subPx);
                dstLine[x] = qRgba(r / diam, g / diam, b / diam, a / diam);
            }
        }

        // Vertical pass: buf → img
        for (int x = 0; x < w; ++x) {
            int r = 0, g = 0, b = 0, a = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                int ny = qBound(0, dy, h - 1);
                QRgb px = reinterpret_cast<const QRgb*>(buf.constScanLine(ny))[x];
                a += qAlpha(px);
                r += qRed(px);
                g += qGreen(px);
                b += qBlue(px);
            }
            reinterpret_cast<QRgb*>(img.scanLine(0))[x] =
                qRgba(r / diam, g / diam, b / diam, a / diam);

            for (int y = 1; y < h; ++y) {
                int addIdx = qMin(y + radius, h - 1);
                int subIdx = qMax(y - radius - 1, 0);
                QRgb addPx = reinterpret_cast<const QRgb*>(buf.constScanLine(addIdx))[x];
                QRgb subPx = reinterpret_cast<const QRgb*>(buf.constScanLine(subIdx))[x];
                a += qAlpha(addPx) - qAlpha(subPx);
                r += qRed(addPx) - qRed(subPx);
                g += qGreen(addPx) - qGreen(subPx);
                b += qBlue(addPx) - qBlue(subPx);
                reinterpret_cast<QRgb*>(img.scanLine(y))[x] =
                    qRgba(r / diam, g / diam, b / diam, a / diam);
            }
        }
    }
    return img;
}

// ── Cache the glow image (expensive, only on resize) ────────────

void SplineChart::rebuildCache() {
    int cw = chartWidth(), h = chartHeight();
    if (cw <= 0 || h <= 0) {
        return;
    }

    // Render glow at 1x resolution — the blur makes any extra resolution
    // invisible, and this avoids 4x pixel cost on Retina displays.
    double dpr = devicePixelRatioF();
    QPainterPath path = buildSplinePath(m_data, cw, h);

    QImage lineImg(cw, h, QImage::Format_ARGB32_Premultiplied);
    lineImg.fill(Qt::transparent);
    {
        QPainter p(&lineImg);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(m_glowColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }

    m_glowCache = blurImage(lineImg, 8);
    m_glowCache.setDevicePixelRatio(1.0);
    // Scale to logical size so drawImage renders at correct position
    // regardless of device pixel ratio
    Q_UNUSED(dpr);
    m_glowValid = true;
}

// ── Mouse tracking ──────────────────────────────────────────────

void SplineChart::mouseMoveEvent(QMouseEvent* event) {
    int cl = chartLeft();
    int cw = chartWidth();
    int mx = event->pos().x() - cl;

    if (mx < 0 || mx > cw) {
        m_hovered = false;
        update();
        return;
    }

    // Map pixel X to data X
    double dataX = m_xMin + (static_cast<double>(mx) / cw) * (m_xMax - m_xMin);
    dataX = qBound(m_xMin, dataX, m_xMax);

    m_hovered = true;
    m_hoverDataX = dataX;
    m_hoverDataY = interpolateY(dataX);
    update();
}

void SplineChart::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_hovered = false;
    update();
}

// ── Paint ───────────────────────────────────────────────────────

void SplineChart::paintEvent(QPaintEvent*) {
    int w = width(), h = height();
    int cl = chartLeft();
    int cw = chartWidth();
    int ch = chartHeight();
    if (m_data.size() < 2 || cw <= 0 || ch <= 0) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Clip to rounded bottom corners if set (matches parent card border-radius)
    if (m_bottomCornerRadius > 0) {
        int r = m_bottomCornerRadius;
        QPainterPath clip;
        clip.moveTo(0, 0);
        clip.lineTo(w, 0);
        clip.lineTo(w, h - r);
        clip.arcTo(w - 2 * r, h - 2 * r, 2 * r, 2 * r, 0, -90);
        clip.lineTo(r, h);
        clip.arcTo(0, h - 2 * r, 2 * r, 2 * r, 270, -90);
        clip.lineTo(0, 0);
        p.setClipPath(clip);
    }

    // Y-axis tick labels and grid lines (drawn in widget coordinates before translate)
    if (m_showYAxis && !m_yTicks.isEmpty()) {
        QFont tickFont(Theme::fontFamily, 9);
        p.setFont(tickFont);

        for (double v : m_yTicks) {
            double py = ch - (v - m_yMin) / (m_yMax - m_yMin) * ch;

            // Grid line spanning the chart area
            p.setPen(QPen(QColor(255, 255, 255, 18), 1, Qt::SolidLine));
            p.drawLine(QPointF(cl, py), QPointF(cl + cw, py));

            // Tick label
            QString label =
                m_yLabelFormatter ? m_yLabelFormatter(v) : QString::number(v, 'f', m_yTickDecimals);
            p.setPen(QColor(255, 255, 255, 90));
            QRectF labelRect(0, py - 8, cl - 4, 16);
            p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }

    // Translate painter into the chart drawing area
    p.save();
    p.translate(cl, 0);

    // Use cached path if available, rebuild only on data/dimension change
    if (!m_pathValid) {
        m_cachedPath = buildSplinePath(m_data, cw, ch);
        m_pathValid = true;
    }
    const QPainterPath& path = m_cachedPath;

    // 1. Gradient fill under the curve
    QPainterPath fillPath = path;
    fillPath.lineTo(cw, ch);
    fillPath.lineTo(0, ch);
    fillPath.closeSubpath();

    QLinearGradient grad(0, 0, 0, ch);
    for (const auto& stop : m_fillStops) {
        grad.setColorAt(stop.first, stop.second);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(grad));
    p.drawPath(fillPath);

    // 2. Blurred glow (cached — skipped during rapid resizes)
    if (m_glowValid) {
        p.drawImage(0, 0, m_glowCache);
    }

    // 3. Crisp line on top
    p.setPen(QPen(m_lineColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // 4. Hover indicator
    if (m_hovered) {
        QPointF dotPos = dataToPixel({m_hoverDataX, m_hoverDataY}, cw, ch);

        // Vertical crosshair line
        p.setPen(QPen(QColor(255, 255, 255, 40), 1, Qt::SolidLine));
        p.drawLine(QPointF(dotPos.x(), 0), QPointF(dotPos.x(), ch));

        // Dot on the line
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(m_lineColor.red(), m_lineColor.green(), m_lineColor.blue()));
        p.drawEllipse(dotPos, 5, 5);

        // Outer ring
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(m_glowColor.red(), m_glowColor.green(), m_glowColor.blue(), 120), 2));
        p.drawEllipse(dotPos, 8, 8);

        // Value label
        QString label = m_valueFormatter(m_hoverDataX, m_hoverDataY);
        QFont font(Theme::fontFamily, 11);
        font.setWeight(QFont::DemiBold);
        p.setFont(font);

        QFontMetrics fm(font);
        int labelW = fm.horizontalAdvance(label) + 16;
        int labelH = fm.height() + 8;
        int labelX = static_cast<int>(dotPos.x()) - labelW / 2;
        int labelY = static_cast<int>(dotPos.y()) - 30;

        // Clamp to chart area bounds
        labelX = qBound(2, labelX, cw - labelW - 2);
        labelY = qMax(2, labelY);

        // Tooltip background
        QRectF labelRect(labelX, labelY, labelW, labelH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 25, 50, 220));
        p.drawRoundedRect(labelRect, 6, 6);

        // Tooltip border
        p.setPen(QPen(QColor(m_glowColor.red(), m_glowColor.green(), m_glowColor.blue(), 100), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(labelRect, 6, 6);

        // Tooltip text
        p.setPen(QColor(220, 200, 255));
        p.drawText(labelRect, Qt::AlignCenter, label);
    }

    // 5. X-axis time labels (drawn in translated space, below chart area)
    if (m_showXAxis && !m_xTicks.isEmpty()) {
        QFont tickFont(Theme::fontFamily, 9);
        p.setFont(tickFont);
        p.setPen(QColor(255, 255, 255, 90));

        QFontMetrics fm(tickFont);
        for (double t : m_xTicks) {
            double px = (t - m_xMin) / (m_xMax - m_xMin) * cw;
            QString label =
                QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t)).toString(m_xTickFormat);
            int labelW = fm.horizontalAdvance(label);
            int labelX = static_cast<int>(px) - labelW / 2;
            labelX = qBound(0, labelX, cw - labelW);
            p.drawText(labelX, ch + fm.ascent() + 3, label);
        }
    }

    p.restore();
}

void SplineChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_glowValid = false;
    m_pathValid = false;
    m_glowTimer.start();
}
