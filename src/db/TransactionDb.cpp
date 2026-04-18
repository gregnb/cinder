#include "TransactionDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {

    TransactionRecord recordFromQuery(const QSqlQuery& q) {
        TransactionRecord r;
        r.id = q.value("id").toInt();
        r.signature = q.value("signature").toString();
        r.blockTime = q.value("block_time").toLongLong();
        r.activityType = q.value("activity_type").toString();
        r.fromAddress = q.value("from_address").toString();
        r.toAddress = q.value("to_address").toString();
        r.token = q.value("token").toString();
        r.amount = q.value("amount").toDouble();
        r.fee = q.value("fee").toInt();
        r.slot = q.value("slot").toInt();
        r.err = q.value("err").toInt() != 0;
        return r;
    }

    void bindFilterValues(QSqlQuery& q, const TransactionFilter& filter) {
        if (!filter.signature.isEmpty()) {
            q.bindValue(":sig_filter", filter.signature);
        }
        if (filter.timeFrom > 0) {
            q.bindValue(":time_from", filter.timeFrom);
        }
        if (filter.timeTo > 0) {
            q.bindValue(":time_to", filter.timeTo);
        }
        for (int i = 0; i < filter.actionTypes.size(); ++i) {
            q.bindValue(QString(":at%1").arg(i), filter.actionTypes[i]);
        }
        if (!filter.fromAddress.isEmpty()) {
            q.bindValue(":from_filter", filter.fromAddress);
        }
        if (!filter.toAddress.isEmpty()) {
            q.bindValue(":to_filter", filter.toAddress);
        }
        if (filter.amountMin >= 0) {
            q.bindValue(":amount_min", filter.amountMin);
        }
        if (filter.amountMax >= 0) {
            q.bindValue(":amount_max", filter.amountMax);
        }
        if (!filter.token.isEmpty()) {
            q.bindValue(":token_filter", filter.token);
            q.bindValue(":token_filter2", filter.token);
        }
    }

    void appendFilterClauses(QString& sql, const TransactionFilter& filter) {
        if (!filter.signature.isEmpty()) {
            sql += " AND t.signature LIKE '%' || :sig_filter || '%'";
        }
        if (filter.timeFrom > 0) {
            sql += " AND t.block_time >= :time_from";
        }
        if (filter.timeTo > 0) {
            sql += " AND t.block_time <= :time_to";
        }
        if (!filter.actionTypes.isEmpty()) {
            QStringList placeholders;
            for (int i = 0; i < filter.actionTypes.size(); ++i) {
                placeholders << QString(":at%1").arg(i);
            }
            sql += " AND t.activity_type IN (" + placeholders.join(",") + ")";
        }
        if (!filter.fromAddress.isEmpty()) {
            sql += " AND t.from_address LIKE '%' || :from_filter || '%'";
        }
        if (!filter.toAddress.isEmpty()) {
            sql += " AND t.to_address LIKE '%' || :to_filter || '%'";
        }
        if (filter.amountMin >= 0) {
            sql += " AND t.amount >= :amount_min";
        }
        if (filter.amountMax >= 0) {
            sql += " AND t.amount <= :amount_max";
        }
        if (!filter.token.isEmpty()) {
            sql += " AND (t.token LIKE '%' || :token_filter || '%'"
                   "      OR t.token IN (SELECT address FROM tokens"
                   "                     WHERE symbol LIKE '%' || :token_filter2 || '%'))";
        }
    }

} // namespace

QSqlDatabase TransactionDb::db() { return Database::connection(); }

bool TransactionDb::insertTransaction(const QString& signature, int slot, qint64 blockTime,
                                      const QString& rawJson, int fee, bool err,
                                      const QList<Activity>& activities) {
    if (hasTransaction(signature)) {
        qWarning() << "[TransactionDb] insert skip existing" << signature;
        return true;
    }

    QSqlDatabase database = db();
    database.transaction();

    if (!DbUtil::exec(
            database,
            "INSERT OR IGNORE INTO transaction_raw (signature, slot, block_time, raw_json) "
            "VALUES (:sig, :slot, :bt, :json)",
            {{":sig", signature}, {":slot", slot}, {":bt", blockTime}, {":json", rawJson}})) {
        qWarning() << "[TransactionDb] insert transaction_raw failed" << signature;
        database.rollback();
        return false;
    }

    for (const auto& a : activities) {
        if (!DbUtil::exec(
                database,
                "INSERT INTO transactions "
                "(signature, block_time, activity_type, from_address, to_address, token, amount, "
                "fee, slot, err) "
                "VALUES (:sig, :bt, :type, :from, :to, :token, :amount, :fee, :slot, :err)",
                {{":sig", signature},
                 {":bt", blockTime},
                 {":type", a.activityType},
                 {":from", a.fromAddress},
                 {":to", a.toAddress},
                 {":token", a.token},
                 {":amount", a.amount},
                 {":fee", fee},
                 {":slot", slot},
                 {":err", err ? 1 : 0}})) {
            qWarning() << "[TransactionDb] insert activity row failed" << signature
                       << a.activityType;
            database.rollback();
            return false;
        }
    }

    const bool committed = database.commit();
    qWarning() << "[TransactionDb] insert" << (committed ? "committed" : "commit failed")
               << signature << "activities=" << activities.size();
    return committed;
}

bool TransactionDb::hasTransaction(const QString& signature) {
    return DbUtil::exists(db(), "SELECT 1 FROM transaction_raw WHERE signature = :sig LIMIT 1",
                          {{":sig", signature}});
}

qint64 TransactionDb::getLatestBlockTime() {
    return DbUtil::scalarInt(db(), "SELECT MAX(block_time) FROM transaction_raw").value_or(0);
}

int TransactionDb::countTransactions(const QString& address) {
    QString sql = "SELECT COUNT(*) FROM transactions WHERE 1=1";
    QSqlQuery q(db());

    if (!address.isEmpty()) {
        sql += " AND (from_address = :addr OR to_address = :addr"
               " OR signature IN (SELECT signature FROM transactions"
               "   WHERE from_address = :addr2 OR to_address = :addr2))";
    }

    if (!q.prepare(sql)) {
        return 0;
    }

    if (!address.isEmpty()) {
        q.bindValue(":addr", address);
        q.bindValue(":addr2", address);
    }

    if (!q.exec()) {
        return 0;
    }

    return q.next() ? q.value(0).toInt() : 0;
}

QList<TransactionRecord> TransactionDb::getTransactionsRecords(const QString& address,
                                                               const QString& token,
                                                               const QString& activityType,
                                                               int limit, int offset) {
    QString sql = "SELECT * FROM transactions WHERE 1=1";
    if (!address.isEmpty()) {
        sql += " AND (from_address = :addr OR to_address = :addr"
               " OR signature IN (SELECT signature FROM transactions"
               "   WHERE from_address = :addr2 OR to_address = :addr2))";
    }
    if (!token.isEmpty()) {
        sql += " AND token = :token";
    }
    if (!activityType.isEmpty()) {
        sql += " AND activity_type = :type";
    }
    sql += " ORDER BY block_time DESC LIMIT :limit OFFSET :offset";

    QSqlQuery q(db());
    if (!q.prepare(sql)) {
        return {};
    }

    if (!address.isEmpty()) {
        q.bindValue(":addr", address);
        q.bindValue(":addr2", address);
    }
    if (!token.isEmpty()) {
        q.bindValue(":token", token);
    }
    if (!activityType.isEmpty()) {
        q.bindValue(":type", activityType);
    }
    q.bindValue(":limit", limit);
    q.bindValue(":offset", offset);

    if (!q.exec()) {
        return {};
    }

    QList<TransactionRecord> results;
    while (q.next()) {
        results.append(recordFromQuery(q));
    }
    return results;
}

int TransactionDb::countFilteredTransactions(const QString& address,
                                             const TransactionFilter& filter) {
    QString sql = "SELECT COUNT(*) FROM transactions t WHERE 1=1";
    if (!address.isEmpty()) {
        sql += " AND (t.from_address = :addr OR t.to_address = :addr"
               " OR t.signature IN (SELECT signature FROM transactions"
               "   WHERE from_address = :addr2 OR to_address = :addr2))";
    }
    appendFilterClauses(sql, filter);

    QSqlQuery q(db());
    if (!q.prepare(sql)) {
        return 0;
    }

    if (!address.isEmpty()) {
        q.bindValue(":addr", address);
        q.bindValue(":addr2", address);
    }
    bindFilterValues(q, filter);

    if (!q.exec()) {
        return 0;
    }

    return q.next() ? q.value(0).toInt() : 0;
}

QList<TransactionRecord> TransactionDb::getFilteredTransactionsRecords(
    const QString& address, const TransactionFilter& filter, int limit, int offset) {
    QString sql = "SELECT t.* FROM transactions t WHERE 1=1";
    if (!address.isEmpty()) {
        sql += " AND (t.from_address = :addr OR t.to_address = :addr"
               " OR t.signature IN (SELECT signature FROM transactions"
               "   WHERE from_address = :addr2 OR to_address = :addr2))";
    }
    appendFilterClauses(sql, filter);
    sql += " ORDER BY t.block_time DESC LIMIT :limit OFFSET :offset";

    QSqlQuery q(db());
    if (!q.prepare(sql)) {
        return {};
    }

    if (!address.isEmpty()) {
        q.bindValue(":addr", address);
        q.bindValue(":addr2", address);
    }
    bindFilterValues(q, filter);
    q.bindValue(":limit", limit);
    q.bindValue(":offset", offset);

    if (!q.exec()) {
        return {};
    }

    QList<TransactionRecord> results;
    while (q.next()) {
        results.append(recordFromQuery(q));
    }
    return results;
}

QList<SignatureInfo> TransactionDb::getRecentSignaturesForAddress(const QString& address,
                                                                  int limit) {
    if (address.isEmpty()) {
        return {};
    }

    const QString sql =
        "SELECT signature, MAX(block_time) AS block_time, MAX(slot) AS slot, MAX(err) AS err "
        "FROM transactions "
        "WHERE from_address = :addr OR to_address = :addr "
        "GROUP BY signature "
        "ORDER BY block_time DESC "
        "LIMIT :limit";

    QSqlQuery q(db());
    if (!q.prepare(sql)) {
        return {};
    }

    q.bindValue(":addr", address);
    q.bindValue(":limit", limit);

    if (!q.exec()) {
        return {};
    }

    QList<SignatureInfo> results;
    while (q.next()) {
        SignatureInfo s;
        s.signature = q.value("signature").toString();
        s.blockTime = q.value("block_time").toLongLong();
        s.slot = q.value("slot").toLongLong();
        s.hasError = q.value("err").toInt() != 0;
        results.append(s);
    }
    return results;
}

QList<SignatureInfo> TransactionDb::getAllSignaturesForAddress(const QString& address) {
    if (address.isEmpty()) {
        return {};
    }

    const QString sql =
        "SELECT signature, MAX(block_time) AS block_time, MAX(slot) AS slot, MAX(err) AS err "
        "FROM transactions "
        "WHERE from_address = :addr OR to_address = :addr "
        "GROUP BY signature "
        "ORDER BY block_time DESC";

    QSqlQuery q(db());
    if (!q.prepare(sql)) {
        return {};
    }

    q.bindValue(":addr", address);

    if (!q.exec()) {
        return {};
    }

    QList<SignatureInfo> results;
    while (q.next()) {
        SignatureInfo s;
        s.signature = q.value("signature").toString();
        s.blockTime = q.value("block_time").toLongLong();
        s.slot = q.value("slot").toLongLong();
        s.hasError = q.value("err").toInt() != 0;
        results.append(s);
    }
    return results;
}

QString TransactionDb::getRawJson(const QString& signature) {
    auto raw =
        DbUtil::scalarString(db(), "SELECT raw_json FROM transaction_raw WHERE signature = :sig",
                             {{":sig", signature}})
            .value_or(QString{});
    qWarning() << "[TransactionDb] getRawJson" << (raw.isEmpty() ? "miss" : "hit") << signature
               << (raw.isEmpty() ? 0 : raw.size());
    return raw;
}
