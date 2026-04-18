#ifndef PRICESERVICE_H
#define PRICESERVICE_H

#include "services/model/PriceData.h"
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QUrl>
#include <functional>

class PriceService : public QObject {
    Q_OBJECT

  public:
    explicit PriceService(QObject* parent = nullptr);

    // Fetch live prices for multiple tokens in one call.
    void fetchPrices(const QStringList& coingeckoIds);

    // Fetch historical daily prices for a single token over a date range.
    // Timestamps are unix seconds.
    void fetchHistoricalPrices(const QString& coingeckoId, qint64 fromTimestamp,
                               qint64 toTimestamp);

    // Look up CoinGecko ID for a mint (synchronous: DB cache → hardcoded seed).
    // Returns empty string if not cached yet (call resolveCoingeckoId to fetch).
    static QString cachedCoingeckoId(const QString& mint);

    // Resolve a mint to its CoinGecko ID via API. Result is cached in DB.
    // Emits coingeckoIdResolved when done.
    void resolveCoingeckoId(const QString& mint);

    // Resolve multiple mints. Emits allCoingeckoIdsResolved when all are done.
    void resolveCoingeckoIds(const QStringList& mints);

    // Number of requests waiting in queue
    int queueSize() const;

    void pause();
    void resume();
    void flushQueue();
    bool isPaused() const;

  signals:
    void pricesReady(const QList<TokenPrice>& prices);
    void historicalPricesReady(const QString& coingeckoId, const QList<HistoricalPrice>& prices);
    void coingeckoIdResolved(const QString& mint, const QString& coingeckoId);
    void allCoingeckoIdsResolved(const QMap<QString, QString>& mintToId);
    void requestFailed(const QString& endpoint, const QString& error);

  private:
    // ── Rate-limited request queue ───────────────────────────
    struct PendingRequest {
        QUrl url;
        QString label;
        std::function<void(const QJsonObject&)> onSuccess;
        std::function<void()> onNotFound;
    };

    void enqueue(const QUrl& url, const QString& label,
                 std::function<void(const QJsonObject&)> onSuccess,
                 std::function<void()> onNotFound = nullptr);
    void processQueue();
    void sendRequest(const PendingRequest& req);

    QQueue<PendingRequest> m_queue;
    QTimer m_throttleTimer;
    int m_activeRequests = 0;

    // Hardcoded seed for common tokens (avoids API calls on first run)
    static const QMap<QString, QString>& seedMap();

    QNetworkAccessManager m_nam;
    bool m_paused = false;
};

#endif // PRICESERVICE_H
