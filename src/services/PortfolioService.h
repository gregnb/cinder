#ifndef PORTFOLIOSERVICE_H
#define PORTFOLIOSERVICE_H

#include "services/model/PriceData.h"
#include <QObject>

class PriceService;

struct PortfolioSummaryModel {
    double totalValueUsd = 0.0;
    double totalCostBasis = 0.0;
    double unrealizedPnl = 0.0;
    double pnlPercent = 0.0;
};

class PortfolioService : public QObject {
    Q_OBJECT

  public:
    explicit PortfolioService(PriceService* priceService, QObject* parent = nullptr);

    // Take a live snapshot now: reads balances from DB, fetches prices, persists.
    void takeSnapshot(const QString& ownerAddress);

    // Check for gaps since last snapshot and backfill with historical data.
    void backfillIfNeeded(const QString& ownerAddress);

    // ── Synchronous P&L queries (read from DB) ──────────────
    static double unrealizedPnl(const QString& ownerAddress, const QString& mint,
                                double currentPrice);
    static double totalPortfolioCostBasis(const QString& ownerAddress);
    static PortfolioSummaryModel portfolioSummaryModel(const QString& ownerAddress,
                                                       double currentTotalUsd);

  signals:
    void snapshotComplete(int snapshotId);
    void backfillProgress(int current, int total);
    void backfillComplete();

  private:
    void onPricesReady(const QList<TokenPrice>& prices, const QString& ownerAddress);
    void doBackfill(const QString& ownerAddress, qint64 lastSnapshotTs, qint64 nowTs);

    PriceService* m_priceService = nullptr;
    QString m_pendingOwner;
};

#endif // PORTFOLIOSERVICE_H
