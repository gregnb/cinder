#ifndef SYNCSERVICE_H
#define SYNCSERVICE_H

#include "db/TransactionDb.h"
#include "services/model/SignatureInfo.h"
#include "services/model/TransactionResponse.h"
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QTimer>

class SolanaApi;
class PortfolioService;

class SyncService : public QObject {
    Q_OBJECT

  public:
    explicit SyncService(SolanaApi* solanaApi, PortfolioService* portfolioService,
                         QObject* parent = nullptr);

    void setWalletAddress(const QString& address);
    void start();
    void stop();
    void pause();
    void resume();
    bool isPaused() const;
    QString walletAddress() const;

  signals:
    void balanceSynced(const QString& address);
    void portfolioSynced(const QString& address);
    void transactionsSynced(const QString& address);
    void syncError(const QString& method, const QString& error);
    void newActivityDetected(const QString& signature, const Activity& activity);

  private slots:
    void onBalanceTick();
    void onSignaturesTick();

  private:
    void handleBalance(const QString& addr, quint64 lamports);
    void handleTokenAccounts(const QString& owner, const QJsonArray& accounts);
    void onBalanceCyclePartDone();

    void handleSignatures(const QString& addr, const QList<SignatureInfo>& sigs);
    void handleTransaction(const QString& signature, const TransactionResponse& tx);

    void connectSignals();
    void disconnectSignals();

    SolanaApi* m_solanaApi = nullptr;
    PortfolioService* m_portfolioService = nullptr;

    QString m_address;

    QTimer m_balanceTimer;
    QTimer m_signaturesTimer;

    bool m_balanceCycleRunning = false;
    bool m_signaturesCycleRunning = false;

    bool m_balanceReceived = false;
    bool m_tokenAccountsReceived = false;

    QSet<QString> m_pendingSignatures;
    int m_pendingTxFetches = 0;
    bool m_isWarmedUp = false;
    bool m_paused = false;

    QMetaObject::Connection m_connBalance;
    QMetaObject::Connection m_connTokenAccounts;
    QMetaObject::Connection m_connSignatures;
    QMetaObject::Connection m_connTransaction;
    QMetaObject::Connection m_connPortfolio;
    QMetaObject::Connection m_connError;
};

#endif // SYNCSERVICE_H
