#ifndef SOLANAAPI_H
#define SOLANAAPI_H

#include "services/model/PriorityFee.h"
#include "services/model/SignatureInfo.h"
#include "services/model/SimulationResponse.h"
#include "services/model/TransactionResponse.h"
#include "tx/NonceAccount.h"
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <optional>

class SolanaApi : public QObject {
    Q_OBJECT

  public:
    explicit SolanaApi(QObject* parent = nullptr);
    explicit SolanaApi(const QString& rpcUrl, QObject* parent = nullptr);
    void setRpcInterceptorForTests(
        const std::function<std::optional<QJsonObject>(const QString&, const QJsonArray&)>&
            interceptor);
    void setMethodLimitForTests(const QString& method, int limit);

    // Multi-endpoint pool (all endpoints used via round-robin)
    void setRpcUrls(const QStringList& urls);
    QStringList rpcUrls() const;
    void addRpcUrl(const QString& url);
    void removeRpcUrl(const QString& url);

    // Backward compat (Terminal `rpc set <url>` replaces entire list)
    void setRpcUrl(const QString& url);
    QString rpcUrl() const;

    // ── Read methods ─────────────────────────────────────

    // Fetch SOL balance for an address (in lamports).
    void fetchBalance(const QString& address);

    // Fetch all SPL token accounts owned by an address (Token Program + Token-2022).
    void fetchTokenAccountsByOwner(const QString& ownerAddress);

    // Fetch transaction signatures for an address.
    // Pass `before` (a signature) to paginate backwards.
    void fetchSignatures(const QString& address, int limit = 1000, const QString& before = {});

    // Fetch full transaction detail for a signature.
    void fetchTransaction(const QString& signature);

    // Fetch recent prioritization fees for a set of writable accounts.
    // Pass up to 128 account addresses. Empty list returns global fees.
    void fetchRecentPrioritizationFees(const QStringList& accounts = {});

    // Fetch minimum balance required for rent exemption (in lamports).
    void fetchMinimumBalanceForRentExemption(quint64 dataSize);

    // Fetch a nonce account's data.
    void fetchNonceAccount(const QString& nonceAddress, const QString& commitment = "finalized");

    // Fetch raw account data for an address (base64 encoded).
    void fetchAccountInfo(const QString& address, const QString& encoding = "base64");

    // Fetch the latest blockhash.
    void fetchLatestBlockhash(const QString& commitment = "finalized");

    // ── Staking methods ──────────────────────────────────

    // Fetch all vote accounts (current + delinquent validators).
    void fetchVoteAccounts();

    // Fetch current inflation rate.
    void fetchInflationRate();

    // Fetch current epoch info.
    void fetchEpochInfo(const QString& commitment = "finalized");

    // Fetch minimum delegation amount for staking.
    void fetchStakeMinimumDelegation(const QString& commitment = "finalized");

    // Fetch all stake accounts where the wallet is the withdrawer.
    void fetchStakeAccounts(const QString& walletAddress);
    void fetchInflationReward(const QString& address, quint64 epoch);

    // Fetch total SOL supply.
    void fetchSupply(const QString& commitment = "finalized");

    // ── Network stats methods ─────────────────────────────

    // Fetch recent performance samples (TPS data). Each sample covers ~60s.
    void fetchRecentPerformanceSamples(int limit = 30);

    // Fetch current block height.
    void fetchBlockHeight(const QString& commitment = "finalized");

    // Fetch Solana node version.
    void fetchVersion();

    // Check confirmation status of one or more signatures (lightweight polling).
    void fetchSignatureStatuses(const QStringList& signatures);

    // ── Write methods ────────────────────────────────────

    // Send a fully-signed, serialized transaction (base64-encoded).
    void sendTransaction(const QByteArray& serializedTx, bool skipPreflight = false,
                         const QString& preflightCommitment = "finalized", int maxRetries = -1);

    // Simulate a transaction without submitting it.
    void simulateTransaction(const QByteArray& serializedTx, bool sigVerify = false,
                             bool replaceRecentBlockhash = true,
                             const QString& commitment = "confirmed");

    // ── Low-priority methods (for background backfill) ───

    void fetchSignaturesLowPriority(const QString& address, int limit = 1000,
                                    const QString& before = {});
    void fetchTransactionLowPriority(const QString& signature);
    void fetchAccountInfoLowPriority(const QString& address, const QString& encoding = "base64");

    // Queue management (for sleep/wake suspension)
    void flushQueues();
    int totalQueueSize() const;

  signals:
    void balanceReady(const QString& address, quint64 lamports);
    void tokenAccountsReady(const QString& owner, const QJsonArray& accounts);
    void signaturesReady(const QString& address, const QList<SignatureInfo>& signatures);
    void transactionReady(const QString& signature, const TransactionResponse& tx);
    void prioritizationFeesReady(const QList<PriorityFee>& fees);
    void minimumBalanceReady(quint64 lamports);
    void nonceAccountReady(const QString& address, const NonceAccount& nonce);
    void accountInfoReady(const QString& address, const QByteArray& data, const QString& owner,
                          quint64 lamports);
    void latestBlockhashReady(const QString& blockhash, quint64 lastValidBlockHeight);
    void transactionSent(const QString& signature);
    void simulationReady(const SimulationResponse& result);
    void voteAccountsReady(const QJsonArray& current, const QJsonArray& delinquent);
    void inflationRateReady(double total, double validator, double foundation, double epoch);
    void epochInfoReady(quint64 epoch, quint64 slotIndex, quint64 slotsInEpoch,
                        quint64 absoluteSlot);
    void stakeMinimumDelegationReady(quint64 lamports);
    void stakeAccountsReady(const QString& wallet, const QJsonArray& accounts);
    void inflationRewardReady(const QString& address, quint64 epoch, bool found, qint64 amount,
                              quint64 postBalance, quint64 effectiveSlot, int commission);
    void supplyReady(quint64 total, quint64 circulating);
    void performanceSamplesReady(const QJsonArray& samples);
    void blockHeightReady(quint64 height);
    void versionReady(const QString& version);
    void signatureStatusesReady(const QJsonArray& statuses);
    void requestFailed(const QString& method, const QString& error);

    // Dedicated backfill signals (separate from SyncService's signals)
    void backfillSignaturesReady(const QString& address, const QList<SignatureInfo>& signatures);
    void backfillTransactionReady(const QString& signature, const TransactionResponse& tx);
    void backfillAccountInfoReady(const QString& address, const QByteArray& data,
                                  const QString& owner, quint64 lamports);

  private:
    // Priority levels for the RPC queue
    enum class Priority { High, Normal, Low };

    struct PendingRpc {
        QString method;
        QJsonArray params;
        std::function<void(const QJsonObject&)> onSuccess;
        Priority priority;
        int retries = 0;
        static constexpr int MAX_RETRIES = 3; // for hard network failures (not 429s)
    };

    void postRpc(const QString& method, const QJsonArray& params,
                 const std::function<void(const QJsonObject&)>& onSuccess);

    void postRpcHighPriority(const QString& method, const QJsonArray& params,
                             const std::function<void(const QJsonObject&)>& onSuccess);

    void postRpcLowPriority(const QString& method, const QJsonArray& params,
                            const std::function<void(const QJsonObject&)>& onSuccess);

    void enqueueRpc(const QString& method, const QJsonArray& params,
                    const std::function<void(const QJsonObject&)>& onSuccess, Priority priority);
    void reEnqueue(PendingRpc req); // push back into its original priority queue
    void processQueue();
    void scheduleDrain();
    int availableTokens();
    int availableMethodTokens(const QString& method);
    bool isMethodInBackoff(const QString& method);
    bool dequeueFirstDispatchable(QQueue<PendingRpc>& queue, PendingRpc& out);
    void updateMethodLimit(const QString& method, int limit);
    void sendRpc(const PendingRpc& req);
    void logRpcStats();
    QString nextRpcUrl();

    QNetworkAccessManager m_nam;
    std::function<std::optional<QJsonObject>(const QString&, const QJsonArray&)> m_rpcInterceptor;
    QStringList m_rpcUrls;
    int m_nextUrlIndex = 0;
    int m_nextId = 1;

    // Priority queue state
    QQueue<PendingRpc> m_highQueue;
    QQueue<PendingRpc> m_normalQueue;
    QQueue<PendingRpc> m_lowQueue;
    int m_activeRequests = 0;

    // Token bucket rate limiter
    QList<qint64> m_requestTimestamps; // dispatch times within rolling window
    QTimer m_drainTimer;               // fires when next token becomes available

    // Per-method rate limits (learned from x-ratelimit-method-limit headers)
    QHash<QString, int> m_methodLimits;               // method → max req per METHOD_RATE_WINDOW_MS
    QHash<QString, QList<qint64>> m_methodTimestamps; // method → dispatch times
    static constexpr int DEFAULT_METHOD_LIMIT =
        1; // conservative: avoid simultaneous same-method calls

    // Per-method backoff (only freezes the affected method, not the whole queue)
    QHash<QString, qint64> m_methodBackoffUntil; // method → epoch ms when backoff expires

    // Data volume tracking (100 MB / 30s server limit)
    struct DataSample {
        qint64 timestamp;
        qint64 bytes;
    };
    QList<DataSample> m_dataSamples;
    qint64 dataUsedInWindow(); // bytes in the last DATA_WINDOW_MS

    static constexpr int DATA_WINDOW_MS = 30000;              // 30-second rolling window
    static constexpr qint64 DATA_LIMIT = 100LL * 1024 * 1024; // 100 MB

    static constexpr int RATE_WINDOW_MS = 1000;         // 1-second global rolling window
    static constexpr int RATE_LIMIT = 2;                // max requests per global window
    static constexpr int MAX_CONCURRENT = 2;            // max in-flight HTTP requests
    static constexpr int METHOD_RATE_WINDOW_MS = 10000; // 10-second per-method rolling window

    static constexpr int BACKOFF_BASE_MS = 5000; // default when no Retry-After header
    static constexpr int BACKOFF_CAP_MS = 60000; // max 60s

    // RPC stats tracking
    QTimer m_statsTimer;
    QHash<QString, int> m_requestCounts; // method → total requests since last stats dump
    QHash<QString, int> m_429Counts;     // method → 429 count since last stats dump
    int m_totalRequests = 0;
    int m_total429s = 0;
};

#endif // SOLANAAPI_H
