#ifndef TPSBARCHART_H
#define TPSBARCHART_H

#include "services/model/NetworkStats.h"
#include <QLabel>
#include <QList>
#include <QWidget>

class TpsBarChart : public QWidget {
    Q_OBJECT

  public:
    explicit TpsBarChart(QWidget* parent = nullptr);
    ~TpsBarChart() override;

    void setData(const QList<TpsSample>& samples);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

  private:
    void computeYTicks();
    void computeXTicks();
    void showTooltip(const TpsSample& sample);
    void repositionTooltip();
    void hideTooltip();
    int chartLeft() const;
    int chartWidth() const;
    int chartHeight() const;
    int chartTop() const;

    QList<TpsSample> m_samples;

    // Y axis
    int m_leftMargin = 0;
    QList<double> m_yTicks;
    double m_yMax = 1;
    int m_yTickDecimals = 0;

    // X axis
    int m_bottomMargin = 0;
    QList<double> m_xTicks;
    QString m_xTickFormat;

    // Hover state
    bool m_hovered = false;
    int m_hoveredBar = -1;
    QPoint m_mousePos;

    // Floating tooltip
    QWidget* m_tooltip = nullptr;
    QLabel* m_tooltipLabel = nullptr;

    // Colors
    QColor m_voteTpsColor{16, 185, 129};     // green
    QColor m_nonVoteTpsColor{167, 139, 250}; // purple
};

#endif // TPSBARCHART_H
