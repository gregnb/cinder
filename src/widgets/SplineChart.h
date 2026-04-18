#ifndef SPLINECHART_H
#define SPLINECHART_H

#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QTimer>
#include <QWidget>
#include <functional>

class SplineChart : public QWidget {
    Q_OBJECT

  public:
    explicit SplineChart(QWidget* parent = nullptr);

    // Data
    void setData(const QList<QPointF>& points);
    int dataSize() const { return m_data.size(); }
    QPointF lastDataPoint() const { return m_data.isEmpty() ? QPointF() : m_data.last(); }

    // Appearance
    void setLineColor(const QColor& color);
    void setGlowColor(const QColor& color);
    void setFillGradient(const QList<QPair<qreal, QColor>>& stops);

    // Tooltip — receives (dataX, dataY), return the string to display
    void setValueFormatter(std::function<QString(double, double)> formatter);

    // Y-axis tick label formatter — receives tick value, returns display string
    // If not set, uses plain number formatting
    void setYLabelFormatter(std::function<QString(double)> formatter);

    // Force Y-axis to include this minimum value (e.g. 0)
    void setYFloor(double floor);

    // Show Y-axis tick labels and faint grid lines
    void setShowYAxis(bool show);

    // Show X-axis time labels along the bottom
    void setShowXAxis(bool show);

    // Clip bottom corners to match a parent card's border-radius
    void setBottomCornerRadius(int radius);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    void rebuildCache();
    void recomputeRange();
    void computeYTicks();
    void computeXTicks();
    int chartLeft() const;
    int chartWidth() const;
    int chartHeight() const;
    QPainterPath buildSplinePath(const QList<QPointF>& dataPoints, int w, int h) const;
    QImage blurImage(const QImage& src, int radius) const;
    double interpolateY(double x) const;
    QPointF dataToPixel(const QPointF& p, int w, int h) const;

    QList<QPointF> m_data;
    QImage m_glowCache;
    bool m_glowValid = false;
    QPainterPath m_cachedPath;
    bool m_pathValid = false;
    QTimer m_glowTimer;

    // Hover state
    bool m_hovered = false;
    double m_hoverDataX = 0;
    double m_hoverDataY = 0;

    // Data range (auto-computed from data with padding)
    double m_xMin = 0;
    double m_xMax = 1;
    double m_yMin = 0;
    double m_yMax = 1;

    // Y floor
    bool m_hasYFloor = false;
    double m_yFloor = 0;

    // Y axis
    bool m_showYAxis = false;
    int m_leftMargin = 0;
    QList<double> m_yTicks;
    int m_yTickDecimals = 2;

    // X axis
    bool m_showXAxis = false;
    int m_bottomMargin = 0;
    QList<double> m_xTicks;
    QString m_xTickFormat;

    // Margins
    int m_rightMargin = 12;
    // Clipping
    int m_bottomCornerRadius = 0;

    // Configurable appearance
    QColor m_lineColor{190, 160, 255};
    QColor m_glowColor{160, 120, 255, 200};
    QList<QPair<qreal, QColor>> m_fillStops;

    // Tooltip formatter
    std::function<QString(double, double)> m_valueFormatter;
    // Y-axis label formatter
    std::function<QString(double)> m_yLabelFormatter;
};

#endif // SPLINECHART_H
