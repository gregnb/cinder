#ifndef PORTFOLIODB_H
#define PORTFOLIODB_H

#include <QList>
#include <QString>
#include <optional>

class QSqlDatabase;

struct PortfolioSnapshotRecord {
    int id = 0;
    qint64 timestamp = 0;
    double totalUsd = 0.0;
    double solPrice = 0.0;
    qint64 createdAt = 0;
};

struct TokenSnapshotRecord {
    QString mint;
    QString symbol;
    double balance = 0.0;
    double priceUsd = 0.0;
    double valueUsd = 0.0;
};

struct PortfolioSummaryRecord {
    double totalValueUsd = 0.0;
    double totalCostBasis = 0.0;
    double unrealizedPnl = 0.0;
    double pnlPercent = 0.0;
};

struct CostBasisLotRecord {
    int id = 0;
    QString mint;
    QString symbol;
    qint64 acquiredAt = 0;
    double quantity = 0.0;
    double costPerUnit = 0.0;
    double costTotal = 0.0;
    double remainingQty = 0.0;
    QString source;
    QString txSignature;
};

class PortfolioDb {
  public:
    // ── Snapshots ────────────────────────────────────────────
    static int insertSnapshot(const QString& ownerAddress, qint64 timestamp, double totalUsd,
                              double solPrice);
    static bool insertTokenSnapshot(int snapshotId, const QString& mint, const QString& symbol,
                                    double balance, double priceUsd, double valueUsd);

    static std::optional<PortfolioSnapshotRecord>
    getLatestSnapshotRecord(const QString& ownerAddress);
    static QList<PortfolioSnapshotRecord>
    getSnapshotsRecords(const QString& ownerAddress, qint64 fromTs, qint64 toTs, int maxRows = 0);
    static QList<TokenSnapshotRecord> getTokenSnapshotsRecords(int snapshotId);

    // Count total snapshots.
    static int countSnapshots(const QString& ownerAddress);

    // ── Pruning ────────────────────────────────────────────────
    // Thin out old snapshots: keep hourly for >7 days, daily for >30 days.
    // Call periodically (e.g. on app startup or after snapshot insertion).
    static int pruneOldSnapshots(const QString& ownerAddress);

    // ── Cost basis (FIFO) ────────────────────────────────────
    static bool insertLot(const QString& ownerAddress, const QString& mint, const QString& symbol,
                          qint64 acquiredAt, double quantity, double costPerUnit,
                          const QString& source, const QString& txSignature = {});

    static bool consumeLotsFifo(const QString& ownerAddress, const QString& mint, double quantity);

    static QList<CostBasisLotRecord> getOpenLotsRecords(const QString& ownerAddress,
                                                        const QString& mint);
    static double totalCostBasis(const QString& ownerAddress, const QString& mint);
    static double totalCostBasisAll(const QString& ownerAddress);
    static double totalRemainingQty(const QString& ownerAddress, const QString& mint);

  private:
    static QSqlDatabase db();
};

#endif // PORTFOLIODB_H
