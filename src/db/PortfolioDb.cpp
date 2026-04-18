#include "PortfolioDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {

    PortfolioSnapshotRecord snapshotFromQuery(const QSqlQuery& q) {
        PortfolioSnapshotRecord r;
        r.id = q.value("id").toInt();
        r.timestamp = q.value("timestamp").toLongLong();
        r.totalUsd = q.value("total_usd").toDouble();
        r.solPrice = q.value("sol_price").toDouble();
        r.createdAt = q.value("created_at").toLongLong();
        return r;
    }

    TokenSnapshotRecord tokenSnapshotFromQuery(const QSqlQuery& q) {
        TokenSnapshotRecord r;
        r.mint = q.value("mint").toString();
        r.symbol = q.value("symbol").toString();
        r.balance = q.value("balance").toDouble();
        r.priceUsd = q.value("price_usd").toDouble();
        r.valueUsd = q.value("value_usd").toDouble();
        return r;
    }

    CostBasisLotRecord lotFromQuery(const QSqlQuery& q) {
        CostBasisLotRecord r;
        r.id = q.value("id").toInt();
        r.mint = q.value("mint").toString();
        r.symbol = q.value("symbol").toString();
        r.acquiredAt = q.value("acquired_at").toLongLong();
        r.quantity = q.value("quantity").toDouble();
        r.costPerUnit = q.value("cost_per_unit").toDouble();
        r.costTotal = q.value("cost_total").toDouble();
        r.remainingQty = q.value("remaining_qty").toDouble();
        r.source = q.value("source").toString();
        r.txSignature = q.value("tx_signature").toString();
        return r;
    }

} // namespace

QSqlDatabase PortfolioDb::db() { return Database::connection(); }

// ── Snapshots ─────────────────────────────────────────────────────

int PortfolioDb::insertSnapshot(const QString& ownerAddress, qint64 timestamp, double totalUsd,
                                double solPrice) {
    QSqlQuery q(db());
    if (!DbUtil::prepareBindExec(
            q,
            "INSERT INTO portfolio_snapshot (owner_address, timestamp, total_usd, sol_price) "
            "VALUES (:owner, :ts, :total, :sol)",
            {{":owner", ownerAddress},
             {":ts", timestamp},
             {":total", totalUsd},
             {":sol", solPrice}})) {
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool PortfolioDb::insertTokenSnapshot(int snapshotId, const QString& mint, const QString& symbol,
                                      double balance, double priceUsd, double valueUsd) {
    return DbUtil::exec(
        db(),
        "INSERT INTO token_snapshot (snapshot_id, mint, symbol, balance, price_usd, value_usd) "
        "VALUES (:sid, :mint, :sym, :bal, :price, :val)",
        {{":sid", snapshotId},
         {":mint", mint},
         {":sym", symbol},
         {":bal", balance},
         {":price", priceUsd},
         {":val", valueUsd}});
}

std::optional<PortfolioSnapshotRecord>
PortfolioDb::getLatestSnapshotRecord(const QString& ownerAddress) {
    static const QString kSql = R"(
        SELECT id, timestamp, total_usd, sol_price, created_at
        FROM portfolio_snapshot
        WHERE owner_address = :owner
        ORDER BY timestamp DESC
        LIMIT 1
    )";

    return DbUtil::one<PortfolioSnapshotRecord>(db(), kSql, {{":owner", ownerAddress}},
                                                snapshotFromQuery);
}

QList<PortfolioSnapshotRecord> PortfolioDb::getSnapshotsRecords(const QString& ownerAddress,
                                                                qint64 fromTs, qint64 toTs,
                                                                int maxRows) {
    if (maxRows > 0) {
        static const QString kSqlLimited = R"(
            SELECT id, timestamp, total_usd, sol_price FROM (
                SELECT id, timestamp, total_usd, sol_price
                FROM portfolio_snapshot
                WHERE owner_address = :owner AND timestamp >= :from AND timestamp <= :to
                ORDER BY timestamp DESC
                LIMIT :lim
            )
            ORDER BY timestamp ASC
        )";

        return DbUtil::many<PortfolioSnapshotRecord>(
            db(), kSqlLimited,
            {{":owner", ownerAddress}, {":from", fromTs}, {":to", toTs}, {":lim", maxRows}},
            snapshotFromQuery);
    }

    static const QString kSql = R"(
        SELECT id, timestamp, total_usd, sol_price
        FROM portfolio_snapshot
        WHERE owner_address = :owner AND timestamp >= :from AND timestamp <= :to
        ORDER BY timestamp ASC
    )";

    return DbUtil::many<PortfolioSnapshotRecord>(
        db(), kSql, {{":owner", ownerAddress}, {":from", fromTs}, {":to", toTs}},
        snapshotFromQuery);
}

QList<TokenSnapshotRecord> PortfolioDb::getTokenSnapshotsRecords(int snapshotId) {
    return DbUtil::many<TokenSnapshotRecord>(
        db(),
        "SELECT mint, symbol, balance, price_usd, value_usd FROM token_snapshot "
        "WHERE snapshot_id = :sid ORDER BY value_usd DESC",
        {{":sid", snapshotId}}, tokenSnapshotFromQuery);
}

int PortfolioDb::countSnapshots(const QString& ownerAddress) {
    return DbUtil::scalarInt(db(),
                             "SELECT COUNT(*) FROM portfolio_snapshot WHERE owner_address = :owner",
                             {{":owner", ownerAddress}})
        .value_or(0);
}

// ── Pruning ──────────────────────────────────────────────────────

int PortfolioDb::pruneOldSnapshots(const QString& ownerAddress) {
    QSqlDatabase database = db();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 oneWeekAgo = now - 7LL * 86400;
    const qint64 oneMonthAgo = now - 30LL * 86400;

    int deleted = 0;

    {
        DbUtil::exec(database,
                     "DELETE FROM token_snapshot WHERE snapshot_id IN ("
                     "  SELECT id FROM portfolio_snapshot"
                     "  WHERE owner_address = :owner AND timestamp < :cutoff"
                     "  AND id NOT IN ("
                     "    SELECT MIN(id) FROM portfolio_snapshot"
                     "    WHERE owner_address = :owner2 AND timestamp < :cutoff2"
                     "    GROUP BY timestamp / 86400"
                     "  )"
                     ")",
                     {{":owner", ownerAddress},
                      {":cutoff", oneMonthAgo},
                      {":owner2", ownerAddress},
                      {":cutoff2", oneMonthAgo}});

        QSqlQuery q2(database);
        if (!DbUtil::prepareBindExec(q2,
                                     "DELETE FROM portfolio_snapshot "
                                     "WHERE owner_address = :owner AND timestamp < :cutoff "
                                     "AND id NOT IN ("
                                     "  SELECT MIN(id) FROM portfolio_snapshot "
                                     "  WHERE owner_address = :owner2 AND timestamp < :cutoff2 "
                                     "  GROUP BY timestamp / 86400"
                                     ")",
                                     {{":owner", ownerAddress},
                                      {":cutoff", oneMonthAgo},
                                      {":owner2", ownerAddress},
                                      {":cutoff2", oneMonthAgo}})) {
            return deleted;
        }
        deleted += q2.numRowsAffected();
    }

    {
        DbUtil::exec(database,
                     "DELETE FROM token_snapshot WHERE snapshot_id IN ("
                     "  SELECT id FROM portfolio_snapshot"
                     "  WHERE owner_address = :owner"
                     "  AND timestamp >= :monthAgo AND timestamp < :weekAgo"
                     "  AND id NOT IN ("
                     "    SELECT MIN(id) FROM portfolio_snapshot"
                     "    WHERE owner_address = :owner2"
                     "    AND timestamp >= :monthAgo2 AND timestamp < :weekAgo2"
                     "    GROUP BY timestamp / 3600"
                     "  )"
                     ")",
                     {{":owner", ownerAddress},
                      {":monthAgo", oneMonthAgo},
                      {":weekAgo", oneWeekAgo},
                      {":owner2", ownerAddress},
                      {":monthAgo2", oneMonthAgo},
                      {":weekAgo2", oneWeekAgo}});

        QSqlQuery q2(database);
        if (!DbUtil::prepareBindExec(q2,
                                     "DELETE FROM portfolio_snapshot "
                                     "WHERE owner_address = :owner "
                                     "AND timestamp >= :monthAgo AND timestamp < :weekAgo "
                                     "AND id NOT IN ("
                                     "  SELECT MIN(id) FROM portfolio_snapshot "
                                     "  WHERE owner_address = :owner2 "
                                     "  AND timestamp >= :monthAgo2 AND timestamp < :weekAgo2 "
                                     "  GROUP BY timestamp / 3600"
                                     ")",
                                     {{":owner", ownerAddress},
                                      {":monthAgo", oneMonthAgo},
                                      {":weekAgo", oneWeekAgo},
                                      {":owner2", ownerAddress},
                                      {":monthAgo2", oneMonthAgo},
                                      {":weekAgo2", oneWeekAgo}})) {
            return deleted;
        }
        deleted += q2.numRowsAffected();
    }

    return deleted;
}

// ── Cost basis (FIFO) ─────────────────────────────────────────────

bool PortfolioDb::insertLot(const QString& ownerAddress, const QString& mint, const QString& symbol,
                            qint64 acquiredAt, double quantity, double costPerUnit,
                            const QString& source, const QString& txSignature) {
    static const QString kSql = R"(
        INSERT INTO cost_basis_lot
            (owner_address, mint, symbol, acquired_at, quantity, cost_per_unit, cost_total,
             remaining_qty, source, tx_signature)
        VALUES (:owner, :mint, :sym, :at, :qty, :cpu, :ct, :rem, :src, :tx)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":owner", ownerAddress},
                         {":mint", mint},
                         {":sym", symbol},
                         {":at", acquiredAt},
                         {":qty", quantity},
                         {":cpu", costPerUnit},
                         {":ct", costPerUnit * quantity},
                         {":rem", quantity},
                         {":src", source},
                         {":tx", txSignature.isEmpty() ? QVariant() : txSignature}});
}

bool PortfolioDb::consumeLotsFifo(const QString& ownerAddress, const QString& mint,
                                  double quantity) {
    QSqlDatabase database = db();
    database.transaction();

    QSqlQuery q(database);
    if (!DbUtil::prepareBindExec(
            q,
            "SELECT id, remaining_qty FROM cost_basis_lot "
            "WHERE owner_address = :owner AND mint = :mint AND remaining_qty > 0 "
            "ORDER BY acquired_at ASC",
            {{":owner", ownerAddress}, {":mint", mint}})) {
        database.rollback();
        return false;
    }

    double remaining = quantity;
    while (q.next() && remaining > 0.0) {
        const int lotId = q.value("id").toInt();
        const double lotQty = q.value("remaining_qty").toDouble();

        const double consume = qMin(lotQty, remaining);
        const double newQty = lotQty - consume;
        remaining -= consume;

        if (!DbUtil::exec(database, "UPDATE cost_basis_lot SET remaining_qty = :qty WHERE id = :id",
                          {{":qty", newQty}, {":id", lotId}})) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

QList<CostBasisLotRecord> PortfolioDb::getOpenLotsRecords(const QString& ownerAddress,
                                                          const QString& mint) {
    static const QString kSql = R"(
        SELECT id, mint, symbol, acquired_at, quantity, cost_per_unit,
               cost_total, remaining_qty, source, tx_signature
        FROM cost_basis_lot
        WHERE owner_address = :owner AND mint = :mint AND remaining_qty > 0
        ORDER BY acquired_at ASC
    )";

    return DbUtil::many<CostBasisLotRecord>(db(), kSql, {{":owner", ownerAddress}, {":mint", mint}},
                                            lotFromQuery);
}

double PortfolioDb::totalCostBasis(const QString& ownerAddress, const QString& mint) {
    static const QString kSql = R"(
        SELECT SUM(cost_per_unit * remaining_qty) AS total
        FROM cost_basis_lot
        WHERE owner_address = :owner AND mint = :mint AND remaining_qty > 0
    )";

    auto v = DbUtil::scalarString(db(), kSql, {{":owner", ownerAddress}, {":mint", mint}});
    return v.has_value() ? v->toDouble() : 0.0;
}

double PortfolioDb::totalCostBasisAll(const QString& ownerAddress) {
    static const QString kSql = R"(
        SELECT SUM(cost_per_unit * remaining_qty) AS total
        FROM cost_basis_lot
        WHERE owner_address = :owner AND remaining_qty > 0
    )";

    auto v = DbUtil::scalarString(db(), kSql, {{":owner", ownerAddress}});
    return v.has_value() ? v->toDouble() : 0.0;
}

double PortfolioDb::totalRemainingQty(const QString& ownerAddress, const QString& mint) {
    static const QString kSql = R"(
        SELECT SUM(remaining_qty) AS total
        FROM cost_basis_lot
        WHERE owner_address = :owner AND mint = :mint AND remaining_qty > 0
    )";

    auto v = DbUtil::scalarString(db(), kSql, {{":owner", ownerAddress}, {":mint", mint}});
    return v.has_value() ? v->toDouble() : 0.0;
}
