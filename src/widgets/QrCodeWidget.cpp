#include "QrCodeWidget.h"
#include <QPainter>
#include <qrencode.h>

QrCodeWidget::QrCodeWidget(QWidget* parent) : QWidget(parent) { setMinimumSize(180, 180); }

void QrCodeWidget::setData(const QString& text) {
    if (m_text == text) {
        return;
    }
    m_text = text;
    regenerate();
    update();
}

void QrCodeWidget::setModuleColor(const QColor& color) {
    m_moduleColor = color;
    update();
}

void QrCodeWidget::setBackgroundColor(const QColor& color) {
    m_bgColor = color;
    update();
}

void QrCodeWidget::regenerate() {
    m_modules = 0;
    m_grid.clear();

    if (m_text.isEmpty()) {
        return;
    }

    QRcode* qr = QRcode_encodeString(m_text.toUtf8().constData(),
                                     0,            // version (auto)
                                     QR_ECLEVEL_M, // error correction: medium
                                     QR_MODE_8,    // 8-bit mode
                                     1             // case-sensitive
    );

    if (!qr) {
        return;
    }

    m_modules = qr->width;
    m_grid.resize(m_modules * m_modules);

    for (int y = 0; y < m_modules; ++y) {
        for (int x = 0; x < m_modules; ++x) {
            m_grid[y * m_modules + x] = (qr->data[y * m_modules + x] & 0x01);
        }
    }

    QRcode_free(qr);
}

void QrCodeWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int side = qMin(w, h);

    // Center the QR code
    int xOff = (w - side) / 2;
    int yOff = (h - side) / 2;

    // Draw rounded background
    QRect bgRect(xOff, yOff, side, side);
    p.setPen(Qt::NoPen);
    p.setBrush(m_bgColor);
    p.drawRoundedRect(bgRect, 16, 16);

    if (m_modules == 0) {
        return;
    }

    // Draw QR modules with quiet zone margin
    int margin = 16;
    int drawArea = side - margin * 2;
    double scale = static_cast<double>(drawArea) / m_modules;

    p.setBrush(m_moduleColor);
    for (int y = 0; y < m_modules; ++y) {
        for (int x = 0; x < m_modules; ++x) {
            if (m_grid[y * m_modules + x]) {
                double px = xOff + margin + x * scale;
                double py = yOff + margin + y * scale;
                // Slight overlap (+0.5) to avoid hairline gaps between modules
                p.drawRect(QRectF(px, py, scale + 0.5, scale + 0.5));
            }
        }
    }
}
