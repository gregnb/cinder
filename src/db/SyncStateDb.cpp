#include "SyncStateDb.h"
#include "Database.h"
#include "DbUtil.h"
#include <QSqlDatabase>

QSqlDatabase SyncStateDb::db() { return Database::connection(); }

QString SyncStateDb::get(const QString& address, const QString& key) {
    return DbUtil::scalarString(
               db(), "SELECT value FROM sync_state WHERE address = :address AND key = :key",
               {{":address", address}, {":key", key}})
        .value_or(QString{});
}

void SyncStateDb::set(const QString& address, const QString& key, const QString& value) {
    DbUtil::exec(db(),
                 "INSERT OR REPLACE INTO sync_state (address, key, value) "
                 "VALUES (:address, :key, :value)",
                 {{":address", address}, {":key", key}, {":value", value}});
}

QString SyncStateDb::oldestFetchedSignature(const QString& address) {
    return get(address, "oldest_fetched_signature");
}

void SyncStateDb::setOldestFetchedSignature(const QString& address, const QString& signature) {
    set(address, "oldest_fetched_signature", signature);
}

bool SyncStateDb::isBackfillComplete(const QString& address) {
    return get(address, "backfill_complete") == "1";
}

void SyncStateDb::setBackfillComplete(const QString& address, bool complete) {
    set(address, "backfill_complete", complete ? "1" : "0");
}

int SyncStateDb::totalFetched(const QString& address) {
    QString val = get(address, "total_fetched");
    return val.isEmpty() ? 0 : val.toInt();
}

void SyncStateDb::setTotalFetched(const QString& address, int count) {
    set(address, "total_fetched", QString::number(count));
}
