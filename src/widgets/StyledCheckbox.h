#ifndef STYLEDCHECKBOX_H
#define STYLEDCHECKBOX_H

#include <QCheckBox>
#include <QPainter>
#include <QPainterPath>

class StyledCheckbox : public QCheckBox {
  public:
    explicit StyledCheckbox(const QString& text = {}, QWidget* parent = nullptr)
        : QCheckBox(text, parent) {
        setObjectName("styledCheckbox");
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override {
        QFont checkboxFont = font();
        checkboxFont.setPointSize(effectiveFontSize());
        const QFontMetrics fm(checkboxFont);
        const int indicator = 18;
        const int gap = 10;
        const int leftPad = 2;
        const int rightPad = 2;
        const int height = qMax(indicator, fm.height()) + 6;
        const int width = leftPad + indicator + gap + fm.horizontalAdvance(text()) + rightPad;
        return {width, height};
    }

    QSize minimumSizeHint() const override { return sizeHint(); }

  protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        const bool hovered = underMouse();
        const bool checked = isChecked();

        QColor textColor = effectiveTextColor();

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont checkboxFont = font();
        checkboxFont.setPointSize(effectiveFontSize());
        painter.setFont(checkboxFont);

        constexpr qreal indicatorSize = 18.0;
        constexpr qreal indicatorRadius = 4.0;
        constexpr qreal borderWidth = 2.0;
        constexpr int gap = 10;

        constexpr qreal leftPad = 2.0;
        constexpr qreal rightPad = 2.0;
        const QRectF indicatorRect(leftPad, (height() - indicatorSize) / 2.0, indicatorSize,
                                   indicatorSize);
        QPainterPath indicatorPath;
        indicatorPath.addRoundedRect(indicatorRect, indicatorRadius, indicatorRadius);

        QColor background = checked ? QColor("#3b82f6") : QColor(20, 22, 40, 153);
        QColor border = checked
                            ? QColor("#3b82f6")
                            : (hovered ? QColor(100, 100, 150, 204) : QColor(100, 100, 150, 128));

        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawPath(indicatorPath);

        QPen borderPen(border, borderWidth);
        borderPen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(indicatorPath);

        if (checked) {
            QPen checkPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(checkPen);
            QPainterPath checkPath;
            checkPath.moveTo(indicatorRect.left() + 4.5, indicatorRect.center().y());
            checkPath.lineTo(indicatorRect.left() + 7.7, indicatorRect.bottom() - 5.2);
            checkPath.lineTo(indicatorRect.right() - 4.0, indicatorRect.top() + 5.0);
            painter.drawPath(checkPath);
        }

        painter.setPen(textColor);
        const QRect textRect(static_cast<int>(indicatorRect.right()) + gap, 0,
                             width() - static_cast<int>(indicatorRect.right()) - gap -
                                 static_cast<int>(rightPad),
                             height());
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }

  private:
    int effectiveFontSize() const {
        const QString uiClass = property("uiClass").toString();
        if (uiClass == QStringLiteral("check13Dim") || uiClass == QStringLiteral("check13") ||
            uiClass == QStringLiteral("check13Strong")) {
            return 13;
        }
        return 15;
    }

    QColor effectiveTextColor() const {
        const QString uiClass = property("uiClass").toString();
        if (uiClass == QStringLiteral("check13Dim")) {
            return QColor(255, 255, 255, 179);
        }
        if (uiClass == QStringLiteral("check13Strong")) {
            return QColor(255, 255, 255, 242);
        }
        return QColor(255, 255, 255, 217);
    }
};

#endif // STYLEDCHECKBOX_H
