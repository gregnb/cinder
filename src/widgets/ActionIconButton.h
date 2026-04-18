#ifndef ACTIONICONBUTTON_H
#define ACTIONICONBUTTON_H

#include <QApplication>
#include <QColor>
#include <QEnterEvent>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPushButton>

class ActionIconButton : public QPushButton {
  public:
    ActionIconButton(const QString& iconPath, int size = 28, QWidget* parent = nullptr)
        : QPushButton(parent), m_size(size) {
        setFixedSize(size, size);
        setCursor(Qt::PointingHandCursor);
        setFlat(true);
        setStyleSheet("background: transparent; border: none;");
        setIconSize(QSize(size - 4, size - 4));

        qreal dpr = qApp->devicePixelRatio();
        int px_size = size * dpr;

        QPixmap px(iconPath);
        if (!px.isNull()) {
            px = px.scaled(px_size, px_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            px.setDevicePixelRatio(dpr);
        }
        m_normalIcon = QIcon(px);

        // Brighten for hover: boost RGB by ~50%
        QImage img = px.toImage();
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                QColor c = img.pixelColor(x, y);
                if (c.alpha() > 30) {
                    c = c.lighter(150);
                    img.setPixelColor(x, y, c);
                }
            }
        }
        QPixmap bright = QPixmap::fromImage(img);
        bright.setDevicePixelRatio(dpr);
        m_hoverIcon = QIcon(bright);

        setIcon(m_normalIcon);
    }

  protected:
    void enterEvent(QEnterEvent*) override { setIcon(m_hoverIcon); }
    void leaveEvent(QEvent*) override { setIcon(m_normalIcon); }

  private:
    int m_size;
    QIcon m_normalIcon;
    QIcon m_hoverIcon;
};

#endif // ACTIONICONBUTTON_H
