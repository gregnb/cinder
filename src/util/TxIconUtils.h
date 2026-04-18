#ifndef TXICONUTILS_H
#define TXICONUTILS_H

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QtSvg/QSvgRenderer>

// Load a transaction-type SVG icon from resources, rendered at the given
// logical size.  The pixmap is DPI-aware (devicePixelRatio is set).
// If `tint` is valid the white source icon is colorised to that color.
inline QPixmap txTypeIcon(const QString& name, int size, qreal dpr, const QColor& tint = QColor()) {
    int px = static_cast<int>(size * dpr);
    QSvgRenderer renderer(QString(":/icons/tx-type/%1.svg").arg(name));
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        renderer.render(&p);
    }
    if (tint.isValid()) {
        QPainter p(&pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), tint);
    }
    pm.setDevicePixelRatio(dpr);
    return pm;
}

#endif // TXICONUTILS_H
