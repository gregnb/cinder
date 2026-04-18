#include "AvatarCache.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

AvatarCache::AvatarCache(QObject* parent) : QObject(parent) {}

// ── Path helpers ──────────────────────────────────────────────────────

QString AvatarCache::cacheDir() const {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + "/avatars";
}

QString AvatarCache::urlHash(const QString& url) const {
    QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString AvatarCache::cacheFilePath(const QString& url) const {
    return cacheDir() + "/" + urlHash(url) + ".png";
}

bool AvatarCache::isCachedAndFresh(const QString& url) const {
    QFileInfo fi(cacheFilePath(url));
    if (!fi.exists() || fi.size() == 0) {
        return false;
    }
    qint64 age = fi.lastModified().secsTo(QDateTime::currentDateTime());
    return age < STALE_SECONDS;
}

QPixmap AvatarCache::loadFromDisk(const QString& url) const {
    QPixmap pm;
    pm.load(cacheFilePath(url));
    return pm;
}

// ── Public API ────────────────────────────────────────────────────────

QPixmap AvatarCache::get(const QString& url) {
    if (url.isEmpty()) {
        return {};
    }

    // 1. In-memory cache
    auto it = m_memoryCache.constFind(url);
    if (it != m_memoryCache.constEnd()) {
        touchLru(url);
        if (!isCachedAndFresh(url)) {
            enqueueDownload(url); // stale — re-download in background
        }
        return it.value();
    }

    // 2. Disk cache
    QPixmap pm = loadFromDisk(url);
    if (!pm.isNull()) {
        evictLru();
        m_memoryCache[url] = pm;
        m_lruOrder.append(url);
        if (!isCachedAndFresh(url)) {
            enqueueDownload(url);
        }
        return pm;
    }

    // 3. Not cached — download
    enqueueDownload(url);
    return {};
}

void AvatarCache::prefetch(const QStringList& urls) {
    for (const QString& url : urls) {
        if (!url.isEmpty() && !isCachedAndFresh(url)) {
            enqueueDownload(url);
        }
    }
}

// ── Download queue ────────────────────────────────────────────────────

void AvatarCache::enqueueDownload(const QString& url) {
    if (url.isEmpty() || m_inFlight.contains(url) || m_queued.contains(url)) {
        return;
    }
    m_queued.insert(url);
    m_downloadQueue.enqueue(url);
    processQueue();
}

void AvatarCache::processQueue() {
    while (m_inFlight.size() < MAX_CONCURRENT && !m_downloadQueue.isEmpty()) {
        QString url = m_downloadQueue.dequeue();
        m_queued.remove(url);
        startDownload(url);
    }
}

void AvatarCache::startDownload(const QString& url) {
    m_inFlight.insert(url);

    QUrl qurl(url);
    QNetworkRequest request{qurl};
    request.setTransferTimeout(DOWNLOAD_TIMEOUT_MS);
    request.setRawHeader("User-Agent", "Cinder/0.1.0");

    QNetworkReply* reply = m_nam.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        m_inFlight.remove(url);

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[AvatarCache] Download failed:" << url << reply->errorString();
            processQueue();
            return;
        }

        QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if (!contentType.startsWith("image/")) {
            qWarning() << "[AvatarCache] Non-image content type:" << contentType << "for" << url;
            processQueue();
            return;
        }

        QByteArray data = reply->readAll();
        if (data.size() > MAX_FILE_SIZE) {
            qWarning() << "[AvatarCache] Too large:" << data.size() << "bytes for" << url;
            processQueue();
            return;
        }

        QPixmap pm;
        if (!pm.loadFromData(data)) {
            qWarning() << "[AvatarCache] Invalid image data for" << url;
            processQueue();
            return;
        }

        // Scale down to max 96px — covers 48px logical × 2x Retina
        if (pm.width() > MAX_CACHED_PX || pm.height() > MAX_CACHED_PX) {
            pm = pm.scaled(MAX_CACHED_PX, MAX_CACHED_PX, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
        }

        QDir().mkpath(cacheDir());
        QString path = cacheFilePath(url);
        if (!pm.save(path, "PNG")) {
            qWarning() << "[AvatarCache] Failed to save:" << path;
            processQueue();
            return;
        }

        evictLru();
        m_memoryCache[url] = pm;
        m_lruOrder.append(url);
        emit avatarReady(url);
        processQueue();
    });
}

// ── LRU eviction ─────────────────────────────────────────────────────

void AvatarCache::touchLru(const QString& url) {
    m_lruOrder.removeOne(url);
    m_lruOrder.append(url);
}

void AvatarCache::evictLru() {
    while (m_memoryCache.size() >= MAX_MEMORY_ENTRIES && !m_lruOrder.isEmpty()) {
        QString oldest = m_lruOrder.takeFirst();
        m_memoryCache.remove(oldest);
    }
}

// ── Rounded-rect clip utility ─────────────────────────────────────────

QPixmap AvatarCache::roundedRectClip(const QPixmap& source, int size, qreal dpr, int radius) {
    if (source.isNull()) {
        return {};
    }

    if (radius <= 0) {
        radius = qMax(1, size / 5); // ~20% corner radius
    }

    int pxSize = static_cast<int>(size * dpr);
    QPixmap scaled =
        source.scaled(pxSize, pxSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);

    QPixmap result(pxSize, pxSize);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, radius, radius);
    painter.setClipPath(path);

    qreal logicalW = scaled.width() / dpr;
    qreal logicalH = scaled.height() / dpr;
    qreal xOff = (logicalW - size) / 2.0;
    qreal yOff = (logicalH - size) / 2.0;
    painter.drawPixmap(QPointF(-xOff, -yOff), scaled);

    return result;
}

// ── Circle clip utility ───────────────────────────────────────────────

QPixmap AvatarCache::circleClip(const QPixmap& source, int diameter, qreal dpr) {
    if (source.isNull()) {
        return {};
    }

    int pxSize = static_cast<int>(diameter * dpr);
    QPixmap scaled =
        source.scaled(pxSize, pxSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);

    QPixmap result(pxSize, pxSize);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // All coordinates in logical units (painter auto-scales to physical)
    QPainterPath path;
    path.addEllipse(0, 0, diameter, diameter);
    painter.setClipPath(path);

    // Center the source in case it's not perfectly square
    qreal logicalW = scaled.width() / dpr;
    qreal logicalH = scaled.height() / dpr;
    qreal xOff = (logicalW - diameter) / 2.0;
    qreal yOff = (logicalH - diameter) / 2.0;
    painter.drawPixmap(QPointF(-xOff, -yOff), scaled);

    return result;
}
