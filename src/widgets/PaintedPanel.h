#ifndef PAINTEDPANEL_H
#define PAINTEDPANEL_H

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

class PaintedPanel : public QWidget {
  public:
    explicit PaintedPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_Hover, true);
        setAttribute(Qt::WA_StyledBackground, false);
    }

    void setFillColor(const QColor& color) {
        m_fillColor = color;
        update();
    }

    void setBorderColor(const QColor& color) {
        m_borderColor = color;
        update();
    }

    void setBorderWidth(qreal width) {
        m_borderWidth = width;
        update();
    }

    void setCornerRadius(qreal radius) {
        m_cornerRadius = radius;
        update();
    }

  protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_fillColor);

        const QRectF borderRect = rect().adjusted(m_borderWidth / 2.0, m_borderWidth / 2.0,
                                                  -m_borderWidth / 2.0, -m_borderWidth / 2.0);

        QPainterPath path;
        path.addRoundedRect(borderRect, m_cornerRadius, m_cornerRadius);
        painter.drawPath(path);

        if (m_borderWidth > 0.0) {
            QPen pen(m_borderColor, m_borderWidth);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(pen);
            painter.drawPath(path);
        }
    }

  private:
    QColor m_fillColor = QColor(16, 18, 36, 204);
    QColor m_borderColor = QColor(140, 100, 200, 115);
    qreal m_borderWidth = 1.0;
    qreal m_cornerRadius = 10.0;
};

#endif // PAINTEDPANEL_H
