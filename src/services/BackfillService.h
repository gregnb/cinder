#ifndef BACKFILLSERVICE_H
#define BACKFILLSERVICE_H

#include "services/model/SignatureInfo.h"
#include "services/model/TransactionResponse.h"
#include <QObject>
#include <QSet>
#include <QTimer>

class SolanaApi;

class BackfillService : public QObject {
    Q_OBJECT

  public:
    explicit BackfillService(SolanaApi* solanaApi, QObject* parent = nullptr);

    void setWalletAddress(const QString& address);
    void start();
    void stop();
    void pause();
    void resume();
    bool isPaused() const;
    bool isRunning() const { return m_running; }

  signals:
    void started();
    void progressUpdated(int totalFetched);
    void pageComplete(int newInPage);
    void backfillComplete();

  private:
    void connectSignals();
    void disconnectSignals();
    void fetchNextPage();
    void handleSignatures(const QString& addr, const QList<SignatureInfo>& sigs);
    void handleTransaction(const QString& signature, const TransactionResponse& tx);
    void onTxFetchDone();

    SolanaApi* m_solanaApi = nullptr;
    QString m_address;
    QString m_cursor; // oldest signature fetched so far (pagination cursor)
    bool m_running = false;
    bool m_pageInProgress = false;

    QSet<QString> m_pendingSignatures;
    int m_pendingTxFetches = 0;
    int m_newInCurrentPage = 0;

    QTimer m_startDelay;
    QTimer m_pageDelay;
    bool m_paused = false;

    QMetaObject::Connection m_connSignatures;
    QMetaObject::Connection m_connTransaction;
    QMetaObject::Connection m_connError;

    static constexpr int BACKFILL_START_DELAY_MS = 10000; // 10s initial delay
    static constexpr int BACKFILL_PAGE_DELAY_MS = 5000;   // 5s between pages
    static constexpr int BACKFILL_SIGNATURE_LIMIT = 100;  // max per page
};

#endif // BACKFILLSERVICE_H
