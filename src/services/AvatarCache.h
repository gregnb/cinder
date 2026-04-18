#ifndef AVATARCACHE_H
#define AVATARCACHE_H

#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPixmap>
#include <QQueue>
#include <QSet>

class AvatarCache : public QObject {
    Q_OBJECT

  public:
    explicit AvatarCache(QObject* parent = nullptr);

    // Returns a cached pixmap if available on disk. If not cached,
    // returns a null QPixmap and enqueues a background download.
    // When the download completes, avatarReady(url) is emitted.
    QPixmap get(const QString& url);

    // Bulk-request avatars for a set of URLs.  Skips empty URLs
    // and URLs that are already cached and fresh.
    void prefetch(const QStringList& urls);

    // Returns a circle-clipped, DPI-aware pixmap at the given logical diameter.
    static QPixmap circleClip(const QPixmap& source, int diameter, qreal dpr);

    // Returns a rounded-rect-clipped, DPI-aware pixmap at the given logical size.
    // If radius is 0, defaults to ~20% of size (Solscan-style).
    static QPixmap roundedRectClip(const QPixmap& source, int size, qreal dpr, int radius = 0);

  signals:
    void avatarReady(const QString& url);

  private:
    QString cacheDir() const;
    QString cacheFilePath(const QString& url) const;
    QString urlHash(const QString& url) const;
    bool isCachedAndFresh(const QString& url) const;
    QPixmap loadFromDisk(const QString& url) const;
    void enqueueDownload(const QString& url);
    void processQueue();
    void startDownload(const QString& url);
    void touchLru(const QString& url);
    void evictLru();

    QNetworkAccessManager m_nam;
    QMap<QString, QPixmap> m_memoryCache;
    QList<QString> m_lruOrder; // most-recently-used at back
    QQueue<QString> m_downloadQueue;
    QSet<QString> m_inFlight;
    QSet<QString> m_queued;

    static constexpr int MAX_CONCURRENT = 4;
    static constexpr int DOWNLOAD_TIMEOUT_MS = 5000;
    static constexpr qint64 MAX_FILE_SIZE = 1024 * 1024;   // 1 MB
    static constexpr qint64 STALE_SECONDS = 7 * 24 * 3600; // 7 days
    static constexpr int MAX_CACHED_PX = 96;               // 48px × 2x Retina
    static constexpr int MAX_MEMORY_ENTRIES = 150;
};

#endif // AVATARCACHE_H
