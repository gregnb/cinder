#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QPainter>
#include <QWidget>

class ToggleSwitch : public QWidget {
    Q_OBJECT
  public:
    explicit ToggleSwitch(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(44, 24);
        setCursor(Qt::PointingHandCursor);
    }

    void setChecked(bool on) {
        if (m_checked == on) {
            return;
        }
        m_checked = on;
        update();
    }

    bool isChecked() const { return m_checked; }

  signals:
    void toggled(bool checked);

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Track — muted teal when on, subtle gray when off
        QColor trackColor = m_checked ? QColor(16, 185, 129, 180) // successGreen at 70%
                                      : QColor(100, 100, 150, 50);
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(0, 0, width(), height(), 12, 12);

        // Knob position
        int knobSize = 18;
        int knobY = (height() - knobSize) / 2;
        int knobX = m_checked ? (width() - knobSize - 3) : 3;

        // Knob shadow
        p.setBrush(QColor(0, 0, 0, 30));
        p.drawEllipse(knobX, knobY + 1, knobSize, knobSize);

        // Knob fill — bright white when on, dimmer when off
        p.setBrush(m_checked ? QColor(255, 255, 255, 240) : QColor(200, 200, 210, 180));
        p.drawEllipse(knobX, knobY, knobSize, knobSize);
    }

    void mousePressEvent(QMouseEvent*) override {
        m_checked = !m_checked;
        update();
        emit toggled(m_checked);
    }

  private:
    bool m_checked = false;
};

#endif // TOGGLESWITCH_H
