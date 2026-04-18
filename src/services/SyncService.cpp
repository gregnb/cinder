#include "SyncService.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "services/PortfolioService.h"
#include "services/SolanaApi.h"
#include "tx/KnownTokens.h"
#include "tx/TxParseUtils.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

static const int BALANCE_INTERVAL_MS = 15000;    // 15 seconds
static const int SIGNATURES_INTERVAL_MS = 30000; // 30 seconds

SyncService::SyncService(SolanaApi* solanaApi, PortfolioService* portfolioService, QObject* parent)
    : QObject(parent), m_solanaApi(solanaApi), m_portfolioService(portfolioService) {
    m_balanceTimer.setInterval(BALANCE_INTERVAL_MS);
    m_signaturesTimer.setInterval(SIGNATURES_INTERVAL_MS);

    connect(&m_balanceTimer, &QTimer::timeout, this, &SyncService::onBalanceTick);
    connect(&m_signaturesTimer, &QTimer::timeout, this, &SyncService::onSignaturesTick);
}

QString SyncService::walletAddress() const { return m_address; }

void SyncService::setWalletAddress(const QString& address) {
    if (address == m_address) {
        return;
    }

    stop();
    m_address = address;

    if (!m_address.isEmpty()) {
        start();
    }
}

void SyncService::start() {
    if (m_address.isEmpty()) {
        return;
    }

    connectSignals();

    // Immediate first sync
    onBalanceTick();
    onSignaturesTick();

    m_balanceTimer.start();
    m_signaturesTimer.start();

    qDebug() << "[SyncService] Started for" << m_address;
}

void SyncService::stop() {
    m_balanceTimer.stop();
    m_signaturesTimer.stop();
    disconnectSignals();

    m_balanceCycleRunning = false;
    m_signaturesCycleRunning = false;
    m_balanceReceived = false;
    m_tokenAccountsReceived = false;
    m_pendingSignatures.clear();
    m_pendingTxFetches = 0;
    m_isWarmedUp = false;
    m_paused = false;
}

void SyncService::pause() {
    if (m_paused) {
        return;
    }
    m_paused = true;
    m_balanceTimer.stop();
    m_signaturesTimer.stop();
    qDebug() << "[SyncService] Paused";
}

void SyncService::resume() {
    if (!m_paused) {
        return;
    }
    m_paused = false;
    if (!m_address.isEmpty()) {
        m_balanceTimer.start();
        m_signaturesTimer.start();
    }
    qDebug() << "[SyncService] Resumed";
}

bool SyncService::isPaused() const { return m_paused; }

// ── Signal connection management ──────────────────────────────────

void SyncService::connectSignals() {
    disconnectSignals();

    m_connBalance =
        connect(m_solanaApi, &SolanaApi::balanceReady, this, &SyncService::handleBalance);
    m_connTokenAccounts = connect(m_solanaApi, &SolanaApi::tokenAccountsReady, this,
                                  &SyncService::handleTokenAccounts);
    m_connSignatures =
        connect(m_solanaApi, &SolanaApi::signaturesReady, this, &SyncService::handleSignatures);
    m_connTransaction =
        connect(m_solanaApi, &SolanaApi::transactionReady, this, &SyncService::handleTransaction);
    m_connPortfolio = connect(m_portfolioService, &PortfolioService::snapshotComplete, this,
                              [this](int snapshotId) {
                                  Q_UNUSED(snapshotId);
                                  if (!m_address.isEmpty()) {
                                      emit portfolioSynced(m_address);
                                  }
                              });
    m_connError =
        connect(m_solanaApi, &SolanaApi::requestFailed, this,
                [this](const QString& method, const QString& error) {
                    // If a getTransaction call fails, decrement the pending counter
                    // so the signatures cycle can complete and retry on the next tick.
                    if (method == "getTransaction" && m_signaturesCycleRunning) {
                        qDebug() << "[SyncService] getTransaction failed:" << error;
                        m_pendingTxFetches--;
                        if (m_pendingTxFetches <= 0) {
                            if (!m_isWarmedUp) {
                                m_isWarmedUp = true;
                                qDebug() << "[SyncService] Warmup complete — notifications enabled";
                            }
                            m_signaturesCycleRunning = false;
                            emit transactionsSynced(m_address);
                        }
                    }
                    emit syncError(method, error);
                });
}

void SyncService::disconnectSignals() {
    disconnect(m_connBalance);
    disconnect(m_connTokenAccounts);
    disconnect(m_connSignatures);
    disconnect(m_connTransaction);
    disconnect(m_connPortfolio);
    disconnect(m_connError);
}

// ── Balance + token accounts cycle (every 15s) ───────────────────

void SyncService::onBalanceTick() {
    if (m_balanceCycleRunning || m_address.isEmpty()) {
        return;
    }

    m_balanceCycleRunning = true;
    m_balanceReceived = false;
    m_tokenAccountsReceived = false;

    qDebug() << "[SyncService] Balance tick — fetching balance + tokens";
    m_solanaApi->fetchBalance(m_address);
    m_solanaApi->fetchTokenAccountsByOwner(m_address);
}

void SyncService::handleBalance(const QString& addr, quint64 lamports) {
    if (addr != m_address || !m_balanceCycleRunning) {
        return;
    }

    double sol = lamports / 1e9;

    TokenAccountDb::upsertToken(WSOL_MINT, "SOL", "Solana", 9, "native");
    TokenAccountDb::upsertAccount(m_address, WSOL_MINT, m_address, QString::number(sol, 'f', 9),
                                  0.0);

    m_balanceReceived = true;
    onBalanceCyclePartDone();
}

void SyncService::handleTokenAccounts(const QString& owner, const QJsonArray& accounts) {
    if (owner != m_address || !m_balanceCycleRunning) {
        return;
    }

    for (const auto& val : accounts) {
        QJsonObject acct = val.toObject();
        QString pubkey = acct["pubkey"].toString();
        QJsonObject info =
            acct["account"].toObject()["data"].toObject()["parsed"].toObject()["info"].toObject();

        QString mint = info["mint"].toString();
        QString tokenAmount = info["tokenAmount"].toObject()["uiAmountString"].toString();
        int decimals = info["tokenAmount"].toObject()["decimals"].toInt();
        QString state = info["state"].toString();

        QString program = acct["account"].toObject()["owner"].toString();
        TokenAccountDb::upsertToken(mint, mint.left(6), mint.left(6), decimals, program);
        TokenAccountDb::upsertAccount(pubkey, mint, owner, tokenAmount, 0.0, state);
    }

    m_tokenAccountsReceived = true;
    onBalanceCyclePartDone();
}

void SyncService::onBalanceCyclePartDone() {
    if (!m_balanceReceived || !m_tokenAccountsReceived) {
        return;
    }

    qDebug() << "[SyncService] Balance cycle complete — taking snapshot";
    m_balanceCycleRunning = false;
    emit balanceSynced(m_address);
    m_portfolioService->takeSnapshot(m_address);
}

// ── Signatures cycle (every 30s) ─────────────────────────────────

void SyncService::onSignaturesTick() {
    if (m_signaturesCycleRunning || m_address.isEmpty()) {
        return;
    }

    m_signaturesCycleRunning = true;
    m_pendingSignatures.clear();
    m_pendingTxFetches = 0;

    m_solanaApi->fetchSignatures(m_address, 25);
}

void SyncService::handleSignatures(const QString& addr, const QList<SignatureInfo>& sigs) {
    if (addr != m_address || !m_signaturesCycleRunning) {
        return;
    }

    int skipped = 0;
    for (const auto& sig : sigs) {
        if (TransactionDb::hasTransaction(sig.signature)) {
            skipped++;
            continue;
        }
        m_pendingSignatures.insert(sig.signature);
        m_solanaApi->fetchTransaction(sig.signature);
    }

    m_pendingTxFetches = m_pendingSignatures.size();

    qDebug() << "[SyncService] Signatures tick —" << m_pendingTxFetches << "new," << skipped
             << "already in DB";

    if (m_pendingTxFetches == 0) {
        if (!m_isWarmedUp) {
            m_isWarmedUp = true;
            qDebug() << "[SyncService] Warmup complete — notifications enabled";
        }
        m_signaturesCycleRunning = false;
        emit transactionsSynced(m_address);
    }
}

void SyncService::handleTransaction(const QString& signature, const TransactionResponse& tx) {
    if (!m_pendingSignatures.contains(signature)) {
        return;
    }
    m_pendingSignatures.remove(signature);

    // Parse activities from top-level + inner instructions
    QList<Activity> activities = TxParseUtils::extractActivities(tx, m_address);

    QString rawJson = QJsonDocument(tx.rawJson).toJson(QJsonDocument::Compact);
    TransactionDb::insertTransaction(signature, tx.slot, tx.blockTime, rawJson,
                                     static_cast<int>(tx.meta.fee), tx.meta.hasError, activities);

    // Emit notification signal for receive activities (only after warmup)
    if (m_isWarmedUp) {
        for (const auto& act : activities) {
            if (act.activityType == "receive") {
                emit newActivityDetected(signature, act);
            }
        }
    }

    m_pendingTxFetches--;
    if (m_pendingTxFetches <= 0) {
        if (!m_isWarmedUp) {
            m_isWarmedUp = true;
            qDebug() << "[SyncService] Warmup complete — notifications enabled";
        }
        m_signaturesCycleRunning = false;
        emit transactionsSynced(m_address);
    }
}
