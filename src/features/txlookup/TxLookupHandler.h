#ifndef TXLOOKUPHANDLER_H
#define TXLOOKUPHANDLER_H

#include "models/TxLookup.h"
#include "tx/TxClassifier.h"
#include "tx/TxParseUtils.h"
#include <QMap>
#include <QMetaObject>
#include <QObject>
#include <QString>

class SolanaApi;
class IdlRegistry;
struct AccountKey;
struct Instruction;
struct TransactionResponse;

class TxLookupHandler : public QObject {
    Q_OBJECT

  public:
    enum class LoadError {
        NoLocalNoApi,
        FetchFailed,
    };

    explicit TxLookupHandler(SolanaApi* api, QObject* parent = nullptr);

    void setWalletAddress(const QString& address);
    void loadTransaction(const QString& signature);
    static bool isValidSignature(const QString& text);
    QString firstSigner(const TransactionResponse& tx) const;
    qint64 priorityFeeLamports(const TransactionResponse& tx) const;
    bool usesDurableNonce(const TransactionResponse& tx) const;
    TxClassifier::Classification classify(const TransactionResponse& tx,
                                          const QString& walletAddress) const;
    QList<TxParseUtils::CuEntry> computeUnitEntries(const TransactionResponse& tx) const;
    TxBalanceChanges balanceChanges(const TransactionResponse& tx) const;
    TxBalanceViewData balanceViewData(const TransactionResponse& tx) const;
    QString friendlyProgramName(const QString& programId, const IdlRegistry* registry) const;
    QString formatTypeName(const QString& type) const;
    QString formatKeyName(const QString& key) const;
    bool looksLikeAddress(const QString& text) const;
    QMap<int, QList<Instruction>> innerInstructionsByIndex(const TransactionResponse& tx) const;
    QMap<QString, AccountKey> accountKeyByAddress(const TransactionResponse& tx) const;
    QString feePayerAddress(const TransactionResponse& tx) const;
    QList<TxInstructionField> instructionFields(const Instruction& ix) const;

  signals:
    void transactionLoaded(const QString& signature, const TransactionResponse& tx);
    void transactionLoadFailed(const QString& signature, TxLookupHandler::LoadError error,
                               const QString& detail);

  private:
    SolanaApi* m_solanaApi = nullptr;
    QString m_walletAddress;
    QString m_pendingSig;
    QMetaObject::Connection m_txConn;
    QMetaObject::Connection m_errConn;
};

#endif // TXLOOKUPHANDLER_H
