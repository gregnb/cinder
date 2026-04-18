#ifndef SYNCSTATEDB_H
#define SYNCSTATEDB_H

#include <QString>

class QSqlDatabase;

class SyncStateDb {
  public:
    // Generic key-value access per address
    static QString get(const QString& address, const QString& key);
    static void set(const QString& address, const QString& key, const QString& value);

    // Convenience accessors for backfill state
    static QString oldestFetchedSignature(const QString& address);
    static void setOldestFetchedSignature(const QString& address, const QString& signature);

    static bool isBackfillComplete(const QString& address);
    static void setBackfillComplete(const QString& address, bool complete);

    static int totalFetched(const QString& address);
    static void setTotalFetched(const QString& address, int count);

  private:
    static QSqlDatabase db();
};

#endif // SYNCSTATEDB_H
