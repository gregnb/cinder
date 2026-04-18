#include "widgets/UploadWidget.h"
#include "Theme.h"

#include <QApplication>
#include <QFileDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

UploadWidget::UploadWidget(Shape shape, int size, QWidget* parent)
    : QWidget(parent), m_shape(shape), m_size(size) {
    setFixedSize(size, size);
    setCursor(Qt::PointingHandCursor);
}

void UploadWidget::setPlaceholderText(const QString& text) { m_placeholder = text; }
void UploadWidget::setFileFilter(const QString& filter) { m_filter = filter; }
void UploadWidget::setMaxResolution(int px) { m_maxResolution = px; }
QString UploadWidget::imagePath() const { return m_imagePath; }
QPixmap UploadWidget::pixmap() const { return m_pixmap; }
bool UploadWidget::hasImage() const { return !m_pixmap.isNull(); }

void UploadWidget::setPixmap(const QPixmap& pm) {
    m_pixmap = pm;
    m_imagePath.clear();
    update();
}

void UploadWidget::setImagePath(const QString& path) {
    QPixmap pm(path);
    if (pm.isNull()) {
        return;
    }
    if (pm.width() > m_maxResolution || pm.height() > m_maxResolution) {
        pm = pm.scaled(m_maxResolution, m_maxResolution, Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);
    }
    m_pixmap = pm;
    m_imagePath = path;
    update();
}

void UploadWidget::clear() {
    m_pixmap = QPixmap();
    m_imagePath.clear();
    update();
    emit imageCleared();
}

// ── Paint ──────────────────────────────────────────────────────────

void UploadWidget::paintEvent(QPaintEvent*) {
    qreal dpr = qApp->devicePixelRatio();
    int w = width();
    int h = height();

    QPixmap buffer(qRound(w * dpr), qRound(h * dpr));
    buffer.setDevicePixelRatio(dpr);
    buffer.fill(Qt::transparent);

    QPainter p(&buffer);
    p.setRenderHint(QPainter::Antialiasing);

    // Build clip path
    QPainterPath clip;
    if (m_shape == Circle) {
        clip.addEllipse(0, 0, w, h);
    } else {
        clip.addRoundedRect(0, 0, w, h, Theme::smallRadius, Theme::smallRadius);
    }

    if (hasImage()) {
        // ── Filled state: shape-clipped image ──
        p.setClipPath(clip);

        // Scale-to-fill
        QPixmap scaled = m_pixmap.scaled(w * dpr, h * dpr, Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        int dx = (w - scaled.width() / dpr) / 2;
        int dy = (h - scaled.height() / dpr) / 2;
        p.drawPixmap(dx, dy, scaled);

        // Hover overlay
        if (m_hovered) {
            p.setBrush(QColor(0, 0, 0, 140));
            p.setPen(Qt::NoPen);
            p.drawPath(clip);

            p.setPen(QColor(255, 255, 255, 200));
            QFont f;
            f.setFamily("Exo 2");
            f.setPixelSize(12);
            f.setWeight(QFont::DemiBold);
            p.setFont(f);
            p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, tr("Change"));
        }
    } else {
        // ── Empty state: dashed border + placeholder ──
        p.fillPath(clip, QColor(22, 24, 42, 100)); // subtle fill

        // Dashed border
        QPen pen(Theme::borderQColor, 2, Qt::DashLine);
        pen.setDashPattern({4, 3});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        if (m_shape == Circle) {
            p.drawEllipse(1, 1, w - 2, h - 2);
        } else {
            p.drawRoundedRect(1, 1, w - 2, h - 2, Theme::smallRadius, Theme::smallRadius);
        }

        // Placeholder text
        p.setPen(QColor(255, 255, 255, 100));
        QFont f;
        f.setFamily("Exo 2");
        f.setPixelSize(m_size > 60 ? 22 : 18);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, m_placeholder);
    }

    p.end();

    QPainter wp(this);
    wp.drawPixmap(0, 0, buffer);
}

// ── Mouse / Hover ──────────────────────────────────────────────────

void UploadWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("Select Image"), QString(), m_filter);
    if (path.isEmpty()) {
        return;
    }

    setImagePath(path);
    emit imageSelected(path);
}

void UploadWidget::enterEvent(QEnterEvent*) {
    m_hovered = true;
    update();
}

void UploadWidget::leaveEvent(QEvent*) {
    m_hovered = false;
    update();
}
