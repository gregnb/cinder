#ifndef STYLEDLINEEDIT_H
#define STYLEDLINEEDIT_H

#include <QColor>
#include <QFocusEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>

class StyledLineEdit : public QLineEdit {
  public:
    explicit StyledLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) { init(); }
    explicit StyledLineEdit(const QString& text, QWidget* parent = nullptr)
        : QLineEdit(text, parent) {
        init();
    }

    void setFrameFillColor(const QColor& color) {
        m_fillColor = color;
        update();
    }

    void setFrameBorderColor(const QColor& color) {
        m_borderColor = color;
        update();
    }

    void setFrameFocusBorderColor(const QColor& color) {
        m_focusBorderColor = color;
        update();
    }

    void setFrameRadius(qreal radius) {
        m_cornerRadius = radius;
        update();
    }

    void setFrameBorderWidth(qreal width) {
        m_borderWidth = width;
        update();
    }

  protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF borderRect = rect().adjusted(m_borderWidth / 2.0, m_borderWidth / 2.0,
                                                  -m_borderWidth / 2.0, -m_borderWidth / 2.0);

        QPainterPath path;
        path.addRoundedRect(borderRect, m_cornerRadius, m_cornerRadius);

        painter.setPen(Qt::NoPen);
        painter.setBrush(m_fillColor);
        painter.drawPath(path);

        if (m_borderWidth > 0.0) {
            QPen pen(hasFocus() ? m_focusBorderColor : m_borderColor, m_borderWidth);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }

        QLineEdit::paintEvent(event);
    }

    void focusInEvent(QFocusEvent* event) override {
        QLineEdit::focusInEvent(event);
        update();
    }

    void focusOutEvent(QFocusEvent* event) override {
        QLineEdit::focusOutEvent(event);
        update();
    }

  private:
    void init() {
        setFrame(false);
        setAttribute(Qt::WA_Hover, true);
        setAttribute(Qt::WA_StyledBackground, false);
        setStyleSheet(QStringLiteral(
            "QLineEdit { background: transparent; border: none; color: white; "
            "font-size: 15px; selection-background-color: rgba(100, 150, 255, 0.3); }"));
        setTextMargins(16, 0, 16, 0);
        setMinimumHeight(44);
    }

    QColor m_fillColor = QColor(30, 31, 55, 204);
    QColor m_borderColor = QColor(100, 100, 150, 77);
    QColor m_focusBorderColor = QColor(100, 150, 255, 128);
    qreal m_cornerRadius = 12.0;
    qreal m_borderWidth = 1.0;
};

#endif // STYLEDLINEEDIT_H
