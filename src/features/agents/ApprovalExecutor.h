#ifndef APPROVALEXECUTOR_H
#define APPROVALEXECUTOR_H

#include "db/McpDb.h"
#include <QObject>
#include <functional>

class QTimer;
class SolanaApi;
class Signer;
class JupiterApi;

class ApprovalExecutor : public QObject {
    Q_OBJECT
  public:
    explicit ApprovalExecutor(QObject* parent = nullptr);

    // Factory callback: given a wallet address, returns a Signer* owned by caller.
    // Returns nullptr if the wallet can't be signed for.
    using SignerFactory = std::function<Signer*(const QString& walletAddress)>;

    void setSolanaApi(SolanaApi* api);
    void setSigner(Signer* signer);
    void setWalletAddress(const QString& address);
    void setSignerFactory(SignerFactory factory);
    void start();
    void stop();

  signals:
    void contactsChanged();
    void balancesChanged();
    void stakeChanged();
    void nonceAccountsChanged();

  private:
    void poll();
    void executeApproval(const McpApprovalRecord& record);

    Signer* signerForWallet(const QString& wallet);

    // ── Existing handlers ────────────────────────────────────────
    void executeSendSol(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeSendToken(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeAddContact(const QString& id, const QJsonObject& args);
    void executeRemoveContact(const QString& id, const QJsonObject& args);

    // ── Stake handlers ───────────────────────────────────────────
    void executeStakeCreate(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeStakeDeactivate(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeStakeWithdraw(const QString& id, const QString& wallet, const QJsonObject& args);

    // ── Token handlers ───────────────────────────────────────────
    void executeTokenBurn(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeTokenClose(const QString& id, const QString& wallet, const QJsonObject& args);

    // ── Swap handler ─────────────────────────────────────────────
    void executeSwap(const QString& id, const QString& wallet, const QJsonObject& args);

    // ── Nonce handlers ───────────────────────────────────────────
    void executeNonceCreate(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeNonceAdvance(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeNonceWithdraw(const QString& id, const QString& wallet, const QJsonObject& args);
    void executeNonceClose(const QString& id, const QString& wallet, const QJsonObject& args);

    void markCompleted(const QString& id, const QJsonObject& result);
    void markFailed(const QString& id, const QString& error);

    SolanaApi* m_api = nullptr;
    JupiterApi* m_jupiterApi = nullptr;
    Signer* m_signer = nullptr;
    Signer* m_tempSigner = nullptr; // on-demand signer for non-active wallets
    QString m_walletAddress;
    SignerFactory m_signerFactory;
    QTimer* m_timer = nullptr;
    bool m_executing = false;

    // Pending swap state (one at a time, since m_executing gates concurrency)
    QString m_pendingSwapId;
    QString m_pendingSwapWallet;
};

#endif // APPROVALEXECUTOR_H
