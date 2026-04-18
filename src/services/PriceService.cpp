#include "PriceService.h"
#include "db/TokenAccountDb.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <memory>

// ═════════════════════════════════════════════════════════════════
// CoinGecko API configuration — adjust these if limits change
// ═════════════════════════════════════════════════════════════════

static const QString COINGECKO_BASE_URL = "https://api.coingecko.com/api/v3";

// Free tier: 30 calls/min. We stay under to leave headroom.
static constexpr int RATE_LIMIT_PER_MINUTE = 25;

// Max concurrent in-flight requests (prevents burst overload)
static constexpr int MAX_CONCURRENT = 3;

// Derived: minimum ms between dequeuing requests
static constexpr int THROTTLE_INTERVAL_MS = (60 * 1000) / RATE_LIMIT_PER_MINUTE; // 2400ms

// ═════════════════════════════════════════════════════════════════

// ── Hardcoded seed for common tokens ─────────────────────────────

const QMap<QString, QString>& PriceService::seedMap() {
    static const QMap<QString, QString> map = {
        {"So11111111111111111111111111111111111111112", "solana"},
        {"EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", "usd-coin"},
        {"Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB", "tether"},
        {"DezXAZ8z7PnrnRJjz3wXBoRgixCa6xjnB7YaB1pPB263", "bonk"},
        {"jtojtomepa8beP8AuQc6eXt5FriJwfFMwQx2v2f9mCL", "jito-governance-token"},
        {"ATLASXmbPQxBUYbxPsV97usA3fPQYEqzQBUHgiFCUsXx", "star-atlas"},
    };
    return map;
}

// ── Constructor ───────────────────────────────────────────────────

PriceService::PriceService(QObject* parent) : QObject(parent) {
    m_throttleTimer.setInterval(THROTTLE_INTERVAL_MS);
    m_throttleTimer.setSingleShot(false);
    connect(&m_throttleTimer, &QTimer::timeout, this, &PriceService::processQueue);
}

// ── Rate-limited request queue ────────────────────────────────────

void PriceService::enqueue(const QUrl& url, const QString& label,
                           std::function<void(const QJsonObject&)> onSuccess,
                           std::function<void()> onNotFound) {
    m_queue.enqueue({url, label, std::move(onSuccess), std::move(onNotFound)});

    // If we're under the concurrency limit, send immediately
    if (m_activeRequests < MAX_CONCURRENT) {
        processQueue();
    }

    // Ensure the timer is running to drain the rest
    if (!m_throttleTimer.isActive() && !m_queue.isEmpty()) {
        m_throttleTimer.start();
    }
}

void PriceService::processQueue() {
    if (m_queue.isEmpty()) {
        m_throttleTimer.stop();
        return;
    }

    if (m_activeRequests >= MAX_CONCURRENT) {
        return; // wait for an in-flight request to complete
    }

    PendingRequest req = m_queue.dequeue();
    sendRequest(req);

    if (m_queue.isEmpty()) {
        m_throttleTimer.stop();
    }
}

void PriceService::sendRequest(const PendingRequest& req) {
    m_activeRequests++;

    QNetworkRequest request(req.url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_nam.get(request);

    connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, label = req.label, onSuccess = req.onSuccess, onNotFound = req.onNotFound]() {
            reply->deleteLater();
            m_activeRequests--;

            // Kick the queue — a slot opened up
            if (!m_queue.isEmpty() && !m_throttleTimer.isActive()) {
                m_throttleTimer.start();
            }

            int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Handle 404 specifically for contract lookups
            if (httpStatus == 404 && onNotFound) {
                onNotFound();
                return;
            }

            if (reply->error() != QNetworkReply::NoError) {
                emit requestFailed(
                    label, QString("HTTP %1: %2").arg(httpStatus).arg(reply->errorString()));
                return;
            }

            const QByteArray data = reply->readAll();
            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);

            if (parseErr.error != QJsonParseError::NoError) {
                emit requestFailed(label, "JSON parse error: " + parseErr.errorString());
                return;
            }

            onSuccess(doc.object());
        });
}

int PriceService::queueSize() const { return m_queue.size(); }

void PriceService::pause() {
    if (m_paused) {
        return;
    }
    m_paused = true;
    m_throttleTimer.stop();
    qDebug() << "[PriceService] Paused";
}

void PriceService::resume() {
    if (!m_paused) {
        return;
    }
    m_paused = false;
    if (!m_queue.isEmpty()) {
        m_throttleTimer.start();
    }
    qDebug() << "[PriceService] Resumed";
}

void PriceService::flushQueue() {
    int discarded = m_queue.size();
    m_queue.clear();
    m_throttleTimer.stop();
    if (discarded > 0) {
        qDebug() << "[PriceService] Flushed" << discarded << "queued requests";
    }
}

bool PriceService::isPaused() const { return m_paused; }

// ── Synchronous lookup: DB cache → hardcoded seed ─────────────────

QString PriceService::cachedCoingeckoId(const QString& mint) {
    // 1. Check DB cache
    QString cached = TokenAccountDb::getCoingeckoId(mint);
    if (!cached.isNull()) {
        return cached; // "" means "not on CoinGecko", non-empty means found
    }

    // 2. Check hardcoded seed
    const auto& seed = seedMap();
    auto it = seed.constFind(mint);
    if (it != seed.constEnd()) {
        // Write to DB so future lookups hit the cache
        TokenAccountDb::setCoingeckoId(mint, it.value());
        return it.value();
    }

    // 3. Not cached yet — caller should use resolveCoingeckoId()
    return QString();
}

// ── Async resolution: single mint via CoinGecko contract API ──────

void PriceService::resolveCoingeckoId(const QString& mint) {
    // Check cache first (avoid unnecessary API call)
    QString cached = cachedCoingeckoId(mint);
    if (!cached.isNull()) {
        emit coingeckoIdResolved(mint, cached);
        return;
    }

    // GET /coins/solana/contract/{mint_address}
    QUrl url(COINGECKO_BASE_URL + "/coins/solana/contract/" + mint);

    enqueue(
        url, "resolveCoingeckoId:" + mint,
        [this, mint](const QJsonObject& root) {
            QString cgId = root["id"].toString();
            if (cgId.isEmpty()) {
                TokenAccountDb::setCoingeckoId(mint, "");
                emit coingeckoIdResolved(mint, "");
            } else {
                TokenAccountDb::setCoingeckoId(mint, cgId);

                // Extract name, symbol, and icon from the same response
                QString name = root["name"].toString();
                QString symbol = root["symbol"].toString().toUpper();
                QString image = root["image"].toObject()["small"].toString();
                if (!name.isEmpty()) {
                    TokenAccountDb::updateTokenMetadata(mint, name, symbol, image);
                    qDebug() << "[PriceService] Resolved" << mint << "->" << cgId << "name:" << name
                             << "symbol:" << symbol;
                } else {
                    qDebug() << "[PriceService] Resolved" << mint << "->" << cgId;
                }

                emit coingeckoIdResolved(mint, cgId);
            }
        },
        // 404 handler — token not listed on CoinGecko
        [this, mint]() {
            TokenAccountDb::setCoingeckoId(mint, "");
            qDebug() << "[PriceService] Mint not on CoinGecko:" << mint;
            emit coingeckoIdResolved(mint, "");
        });
}

// ── Batch resolution: multiple mints ──────────────────────────────

void PriceService::resolveCoingeckoIds(const QStringList& mints) {
    auto results = std::make_shared<QMap<QString, QString>>();
    auto pending = std::make_shared<int>(0);

    // First pass: resolve everything we can from cache
    QStringList needsApi;
    for (const QString& mint : mints) {
        QString cached = cachedCoingeckoId(mint);
        if (!cached.isNull()) {
            (*results)[mint] = cached;
        } else {
            needsApi.append(mint);
        }
    }

    if (needsApi.isEmpty()) {
        emit allCoingeckoIdsResolved(*results);
        return;
    }

    *pending = needsApi.size();

    for (const QString& mint : needsApi) {
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(
            this, &PriceService::coingeckoIdResolved, this,
            [this, conn, mint, results, pending](const QString& resolvedMint, const QString& cgId) {
                if (resolvedMint != mint) {
                    return;
                }
                disconnect(*conn);

                (*results)[mint] = cgId;
                (*pending)--;

                if (*pending <= 0) {
                    emit allCoingeckoIdsResolved(*results);
                }
            });

        resolveCoingeckoId(mint);
    }
}

// ── Fetch live prices ─────────────────────────────────────────────

void PriceService::fetchPrices(const QStringList& coingeckoIds) {
    QUrl url(COINGECKO_BASE_URL + "/simple/price");
    QUrlQuery query;
    query.addQueryItem("ids", coingeckoIds.join(","));
    query.addQueryItem("vs_currencies", "usd");
    query.addQueryItem("include_24hr_change", "true");
    url.setQuery(query);

    enqueue(url, "fetchPrices", [this, coingeckoIds](const QJsonObject& root) {
        QList<TokenPrice> results;
        results.reserve(coingeckoIds.size());

        for (const QString& id : coingeckoIds) {
            if (root.contains(id)) {
                TokenPrice tp = TokenPrice::fromJson(id, root[id].toObject());
                results.append(tp);
            }
        }

        emit pricesReady(results);
    });
}

// ── Fetch historical prices ───────────────────────────────────────

void PriceService::fetchHistoricalPrices(const QString& coingeckoId, qint64 fromTimestamp,
                                         qint64 toTimestamp) {
    QUrl url(COINGECKO_BASE_URL + "/coins/" + coingeckoId + "/market_chart/range");
    QUrlQuery query;
    query.addQueryItem("vs_currency", "usd");
    query.addQueryItem("from", QString::number(fromTimestamp));
    query.addQueryItem("to", QString::number(toTimestamp));
    url.setQuery(query);

    enqueue(url, "fetchHistoricalPrices:" + coingeckoId,
            [this, coingeckoId](const QJsonObject& root) {
                QList<HistoricalPrice> prices = HistoricalPrice::fromMarketChartJson(root);
                emit historicalPricesReady(coingeckoId, prices);
            });
}
