#ifndef COPYBUTTON_H
#define COPYBUTTON_H

#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTimer>

namespace CopyButton {

    inline QPixmap renderIcon(const QString& path, int logicalSize, qreal opacity = 1.0) {
        qreal dpr = qApp->devicePixelRatio();
        int px = qRound(logicalSize * dpr);
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QSvgRenderer renderer(path);
        QPainter p(&pm);
        p.setOpacity(opacity);
        renderer.render(&p);
        pm.setDevicePixelRatio(dpr);
        return pm;
    }

    // Apply the standard copy SVG icon + checkmark feedback to any QPushButton.
    // Call after creating the button and setting its size/style.
    inline void applyIcon(QPushButton* btn, int iconSize = 14) {
        btn->setIcon(QIcon(renderIcon(":/icons/ui/copy.svg", iconSize, 0.55)));
        btn->setIconSize(QSize(iconSize, iconSize));
    }

    // Wire up a copy button: copies text to clipboard and shows checkmark feedback.
    inline void wire(QPushButton* btn, const QString& textToCopy, QObject* parent,
                     int iconSize = 14) {
        QObject::connect(btn, &QPushButton::clicked, parent, [btn, textToCopy, iconSize]() {
            QApplication::clipboard()->setText(textToCopy);
            btn->setIcon(QIcon(renderIcon(":/icons/ui/checkmark.svg", iconSize)));
            auto* timer = new QTimer(btn);
            timer->setSingleShot(true);
            timer->setInterval(1500);
            QObject::connect(timer, &QTimer::timeout, btn, [btn, iconSize, timer]() {
                btn->setIcon(QIcon(renderIcon(":/icons/ui/copy.svg", iconSize, 0.55)));
                timer->deleteLater();
            });
            timer->start();
        });
    }

    // Convenience: set icon + wire copy in one call.
    inline void setup(QPushButton* btn, const QString& textToCopy, QObject* parent,
                      int iconSize = 14) {
        applyIcon(btn, iconSize);
        wire(btn, textToCopy, parent, iconSize);
    }

} // namespace CopyButton

#endif // COPYBUTTON_H
