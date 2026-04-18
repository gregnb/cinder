#include "PortfolioService.h"
#include "PriceService.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include <QDateTime>
#include <QDebug>

static constexpr qint64 SECONDS_PER_DAY = 86400;

// ── Constructor ───────────────────────────────────────────────────

PortfolioService::PortfolioService(PriceService* priceService, QObject* parent)
    : QObject(parent), m_priceService(priceService) {}

// ── Take a live snapshot ──────────────────────────────────────────

void PortfolioService::takeSnapshot(const QString& ownerAddress) {
    // 1. Read current balances from the local token_accounts DB
    QList<TokenAccountRecord> accounts = TokenAccountDb::getAccountsByOwnerRecords(ownerAddress);

    // 2. Collect all mints — some may need CoinGecko ID resolution
    QStringList allMints;
    for (const auto& acct : accounts) {
        allMints.append(acct.tokenAddress);
    }

    if (allMints.isEmpty()) {
        qDebug() << "[PortfolioService] No tokens to snapshot";
        return;
    }

    m_pendingOwner = ownerAddress;

    // 3. Resolve all mints → CoinGecko IDs (cache hits are instant, misses call API)
    disconnect(m_priceService, &PriceService::allCoingeckoIdsResolved, this, nullptr);

    connect(m_priceService, &PriceService::allCoingeckoIdsResolved, this,
            [this](const QMap<QString, QString>& mintToId) {
                disconnect(m_priceService, &PriceService::allCoingeckoIdsResolved, this, nullptr);

                // Collect non-empty CoinGecko IDs
                QStringList cgIds;
                for (auto it = mintToId.constBegin(); it != mintToId.constEnd(); ++it) {
                    if (!it.value().isEmpty() && !cgIds.contains(it.value())) {
                        cgIds.append(it.value());
                    }
                }

                if (cgIds.isEmpty()) {
                    qDebug() << "[PortfolioService] No priceable tokens found";
                    return;
                }

                // 4. Now fetch prices
                disconnect(m_priceService, &PriceService::pricesReady, this, nullptr);
                connect(m_priceService, &PriceService::pricesReady, this,
                        [this](const QList<TokenPrice>& prices) {
                            disconnect(m_priceService, &PriceService::pricesReady, this, nullptr);
                            onPricesReady(prices, m_pendingOwner);
                        });

                m_priceService->fetchPrices(cgIds);
            });

    m_priceService->resolveCoingeckoIds(allMints);
}

void PortfolioService::onPricesReady(const QList<TokenPrice>& prices, const QString& ownerAddress) {
    // Build a price lookup: coingeckoId → price
    QMap<QString, double> priceLookup;
    double solPrice = 0.0;
    for (const auto& tp : prices) {
        priceLookup[tp.coingeckoId] = tp.priceUsd;
        if (tp.coingeckoId == "solana") {
            solPrice = tp.priceUsd;
        }
    }

    // Re-read balances (they may have changed during the async fetch)
    QList<TokenAccountRecord> accounts = TokenAccountDb::getAccountsByOwnerRecords(ownerAddress);

    // Insert the snapshot
    qint64 now = QDateTime::currentSecsSinceEpoch();
    double totalUsd = 0.0;

    // First pass: calculate total
    struct TokenRow {
        QString mint;
        QString symbol;
        double balance;
        double price;
        double value;
    };
    QList<TokenRow> rows;

    for (const auto& acct : accounts) {
        QString mint = acct.tokenAddress;
        QString symbol = acct.symbol;
        double balance = acct.balance.toDouble();
        QString cgId = PriceService::cachedCoingeckoId(mint);
        double price = priceLookup.value(cgId, 0.0);
        double value = balance * price;
        totalUsd += value;
        rows.append({mint, symbol, balance, price, value});
    }

    int snapshotId = PortfolioDb::insertSnapshot(ownerAddress, now, totalUsd, solPrice);
    if (snapshotId < 0) {
        qWarning() << "[PortfolioService] Failed to insert snapshot";
        return;
    }

    // Insert per-token rows
    for (const auto& row : rows) {
        PortfolioDb::insertTokenSnapshot(snapshotId, row.mint, row.symbol, row.balance, row.price,
                                         row.value);
    }

    qDebug() << "[PortfolioService] Snapshot" << snapshotId << "taken: $" << totalUsd << "across"
             << rows.size() << "tokens";

    emit snapshotComplete(snapshotId);
}

// ── Gap detection & backfill ──────────────────────────────────────

void PortfolioService::backfillIfNeeded(const QString& ownerAddress) {
    auto latest = PortfolioDb::getLatestSnapshotRecord(ownerAddress);
    qint64 now = QDateTime::currentSecsSinceEpoch();

    if (!latest.has_value()) {
        // No snapshots at all — just take one now
        qDebug() << "[PortfolioService] No previous snapshots, taking first one";
        takeSnapshot(ownerAddress);
        return;
    }

    qint64 lastTs = latest->timestamp;
    qint64 gap = now - lastTs;

    if (gap <= SECONDS_PER_DAY) {
        qDebug() << "[PortfolioService] Last snapshot is recent (" << gap
                 << "s ago), skipping backfill";
        takeSnapshot(ownerAddress);
        return;
    }

    qDebug() << "[PortfolioService] Gap detected:" << (gap / SECONDS_PER_DAY)
             << "days, starting backfill";
    doBackfill(ownerAddress, lastTs, now);
}

void PortfolioService::doBackfill(const QString& ownerAddress, qint64 lastSnapshotTs,
                                  qint64 nowTs) {
    // Get the last snapshot's token breakdown to carry forward balances
    auto latest = PortfolioDb::getLatestSnapshotRecord(ownerAddress);
    if (!latest.has_value()) {
        takeSnapshot(ownerAddress);
        emit backfillComplete();
        return;
    }
    int lastId = latest->id;
    QList<TokenSnapshotRecord> lastTokens = PortfolioDb::getTokenSnapshotsRecords(lastId);

    // Collect the CoinGecko IDs we need historical prices for
    QMap<QString, double> lastBalances;  // cgId → balance
    QMap<QString, QString> cgIdToMint;   // cgId → mint
    QMap<QString, QString> cgIdToSymbol; // cgId → symbol

    for (const auto& ts : lastTokens) {
        QString mint = ts.mint;
        QString cgId = PriceService::cachedCoingeckoId(mint);
        if (!cgId.isEmpty()) {
            lastBalances[cgId] = ts.balance;
            cgIdToMint[cgId] = mint;
            cgIdToSymbol[cgId] = ts.symbol;
        }
    }

    if (lastBalances.isEmpty()) {
        qDebug() << "[PortfolioService] No tokens to backfill";
        takeSnapshot(ownerAddress);
        emit backfillComplete();
        return;
    }

    // Calculate how many days to fill
    qint64 startDay = (lastSnapshotTs / SECONDS_PER_DAY + 1) * SECONDS_PER_DAY; // next midnight
    qint64 endDay = (nowTs / SECONDS_PER_DAY) * SECONDS_PER_DAY;                // today midnight
    int totalDays = static_cast<int>((endDay - startDay) / SECONDS_PER_DAY) + 1;

    if (totalDays <= 0) {
        takeSnapshot(ownerAddress);
        emit backfillComplete();
        return;
    }

    // We need to fetch historical prices for each token, then insert daily snapshots.
    // Track how many tokens have returned data.
    auto pendingCount = std::make_shared<int>(lastBalances.size());
    auto historicalData = std::make_shared<QMap<QString, QList<HistoricalPrice>>>();

    for (auto it = lastBalances.constBegin(); it != lastBalances.constEnd(); ++it) {
        const QString& cgId = it.key();

        // One-shot connection per token
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(
            m_priceService, &PriceService::historicalPricesReady, this,
            [this, conn, cgId, pendingCount, historicalData, lastBalances, cgIdToMint, cgIdToSymbol,
             startDay, endDay, totalDays,
             ownerAddress](const QString& returnedId, const QList<HistoricalPrice>& prices) {
                if (returnedId != cgId) {
                    return;
                }
                disconnect(*conn);

                (*historicalData)[cgId] = prices;
                (*pendingCount)--;

                emit backfillProgress(static_cast<int>(historicalData->size()),
                                      static_cast<int>(lastBalances.size()));

                if (*pendingCount > 0) {
                    return;
                }

                // All tokens have returned — insert synthetic daily snapshots
                for (qint64 dayTs = startDay; dayTs <= endDay; dayTs += SECONDS_PER_DAY) {
                    double totalUsd = 0.0;
                    double solPrice = 0.0;

                    struct Row {
                        QString mint;
                        QString symbol;
                        double balance;
                        double price;
                        double value;
                    };
                    QList<Row> rows;

                    for (auto bit = lastBalances.constBegin(); bit != lastBalances.constEnd();
                         ++bit) {
                        const QString& tid = bit.key();
                        double balance = bit.value();

                        // Find closest price for this day
                        double price = 0.0;
                        const auto& hPrices = (*historicalData)[tid];
                        qint64 dayMs = dayTs * 1000;
                        qint64 bestDiff = std::numeric_limits<qint64>::max();
                        for (const auto& hp : hPrices) {
                            qint64 diff = qAbs(hp.timestamp - dayMs);
                            if (diff < bestDiff) {
                                bestDiff = diff;
                                price = hp.priceUsd;
                            }
                        }

                        double value = balance * price;
                        totalUsd += value;
                        if (tid == "solana") {
                            solPrice = price;
                        }

                        rows.append({cgIdToMint[tid], cgIdToSymbol[tid], balance, price, value});
                    }

                    int sid = PortfolioDb::insertSnapshot(ownerAddress, dayTs, totalUsd, solPrice);
                    if (sid >= 0) {
                        for (const auto& r : rows) {
                            PortfolioDb::insertTokenSnapshot(sid, r.mint, r.symbol, r.balance,
                                                             r.price, r.value);
                        }
                    }
                }

                qDebug() << "[PortfolioService] Backfill complete:" << totalDays << "days";

                // Now take a fresh live snapshot for today
                takeSnapshot(ownerAddress);
                emit backfillComplete();
            });

        m_priceService->fetchHistoricalPrices(cgId, lastSnapshotTs, nowTs);
    }
}

// ── Synchronous P&L queries ───────────────────────────────────────

double PortfolioService::unrealizedPnl(const QString& ownerAddress, const QString& mint,
                                       double currentPrice) {
    double remaining = PortfolioDb::totalRemainingQty(ownerAddress, mint);
    double costBasis = PortfolioDb::totalCostBasis(ownerAddress, mint);
    return (currentPrice * remaining) - costBasis;
}

double PortfolioService::totalPortfolioCostBasis(const QString& ownerAddress) {
    return PortfolioDb::totalCostBasisAll(ownerAddress);
}

PortfolioSummaryModel PortfolioService::portfolioSummaryModel(const QString& ownerAddress,
                                                              double currentTotalUsd) {
    double totalCost = totalPortfolioCostBasis(ownerAddress);
    double unrealized = currentTotalUsd - totalCost;
    double pctReturn = (totalCost > 0.0) ? (unrealized / totalCost) * 100.0 : 0.0;

    return {currentTotalUsd, totalCost, unrealized, pctReturn};
}
