#ifndef TRANSACTIONDB_H
#define TRANSACTIONDB_H

#include "services/model/SignatureInfo.h"
#include <QList>
#include <QString>
#include <QStringList>

class QSqlDatabase;

struct Activity {
    QString activityType; // "send" or "receive"
    QString fromAddress;
    QString toAddress;
    QString token; // "SOL" or mint address
    double amount;
};

struct TransactionRecord {
    int id = 0;
    QString signature;
    qint64 blockTime = 0;
    QString activityType;
    QString fromAddress;
    QString toAddress;
    QString token;
    double amount = 0.0;
    int fee = 0;
    int slot = 0;
    bool err = false;
};

struct TransactionFilter {
    QString signature;       // substring match on signature
    qint64 timeFrom = 0;     // block_time >= (0 = no filter)
    qint64 timeTo = 0;       // block_time <= (0 = no filter)
    QStringList actionTypes; // activity_type IN (...)
    QString fromAddress;     // substring match on from_address
    QString toAddress;       // substring match on to_address
    double amountMin = -1;   // amount >= (-1 = no filter)
    double amountMax = -1;   // amount <= (-1 = no filter)
    QString token;           // substring match on token column or tokens.symbol

    bool isEmpty() const {
        return signature.isEmpty() && timeFrom == 0 && timeTo == 0 && actionTypes.isEmpty() &&
               fromAddress.isEmpty() && toAddress.isEmpty() && amountMin < 0 && amountMax < 0 &&
               token.isEmpty();
    }
};

class TransactionDb {
  public:
    // Insert a full transaction (raw + extracted activities) in one DB transaction
    static bool insertTransaction(const QString& signature, int slot, qint64 blockTime,
                                  const QString& rawJson, int fee, bool err,
                                  const QList<Activity>& activities);

    // Check if we already have this transaction
    static bool hasTransaction(const QString& signature);

    // Get the most recent block_time we've stored (for incremental sync)
    static qint64 getLatestBlockTime();

    // List view: get transactions with optional filters
    static QList<TransactionRecord> getTransactionsRecords(const QString& address = {},
                                                           const QString& token = {},
                                                           const QString& activityType = {},
                                                           int limit = 50, int offset = 0);

    // List view: get transactions with full filter support
    static QList<TransactionRecord> getFilteredTransactionsRecords(const QString& address,
                                                                   const TransactionFilter& filter,
                                                                   int limit = 50, int offset = 0);

    // Recent distinct signatures touching an address, newest first
    static QList<SignatureInfo> getRecentSignaturesForAddress(const QString& address,
                                                              int limit = 10);

    // All distinct signatures touching an address, newest first
    static QList<SignatureInfo> getAllSignaturesForAddress(const QString& address);

    // Count total rows for pagination
    static int countTransactions(const QString& address = {});

    // Count rows matching filter (for filtered pagination)
    static int countFilteredTransactions(const QString& address, const TransactionFilter& filter);

    // Detail view: get raw JSON for a signature
    static QString getRawJson(const QString& signature);

  private:
    static QSqlDatabase db();
};

#endif // TRANSACTIONDB_H
