#ifndef CONTENTBORDERWIDGET_H
#define CONTENTBORDERWIDGET_H

#include "Theme.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

// Custom widget that paints an L-shaped border (left + top) with a rounded corner
class ContentBorderWidget : public QWidget {
  public:
    using QWidget::QWidget;

  protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QPen pen(Theme::borderQColor);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        qreal r = static_cast<qreal>(Theme::contentBorderRadius);
        QPainterPath path;
        path.moveTo(0.5, height());
        path.lineTo(0.5, r + 0.5);
        path.arcTo(0.5, 0.5, r * 2, r * 2, 180, -90);
        path.lineTo(width(), 0.5);

        painter.drawPath(path);
    }
};

#endif // CONTENTBORDERWIDGET_H
