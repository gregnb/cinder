#ifndef TERMINALHANDLER_H
#define TERMINALHANDLER_H

#include <QColor>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

#include "crypto/Keypair.h"
#include "features/terminal/TerminalAsyncOp.h"
#include "features/terminal/TerminalCommandCatalog.h"
#include "features/terminal/TerminalCommandContext.h"
#include "features/terminal/TerminalRequestTracker.h"

class Signer;
class SolanaApi;
class IdlRegistry;
class NetworkStatsService;
class PriceService;
class ValidatorService;
class JupiterApi;
class QJsonObject;
class TransactionResponse;

class TerminalHandler : public QObject {
    Q_OBJECT

  public:
    explicit TerminalHandler(SolanaApi* api, IdlRegistry* idlRegistry,
                             NetworkStatsService* networkStats, PriceService* priceService,
                             ValidatorService* validatorService, JupiterApi* jupiterApi,
                             QObject* parent = nullptr);

    void setWalletAddress(const QString& address);
    void setKeypair(const Keypair& keypair);
    void setSigner(Signer* signer);
    void setOutputSink(const std::function<void(const QString&, const QColor&)>& sink);
    void setSecretSink(const std::function<void(const QString&, const QString&)>& sink);
    bool hasPending() const;
    bool hasPendingConfirm() const;
    void cancelPendingConfirm();
    bool consumePendingConfirm(const QString& answer);
    void cancelPending(const QString& reason = {});

    void cmdHelp(const QStringList& args);
    void cmdAddress();
    void cmdBalance();
    void cmdBalances();
    void cmdSend(const QStringList& args);
    void cmdHistory(const QStringList& args);
    void cmdVersion();
    void cmdWallet(const TerminalParsedCommand& cmd);
    void cmdKey(const TerminalParsedCommand& cmd);
    void cmdMnemonic(const TerminalParsedCommand& cmd);
    void cmdTx(const TerminalParsedCommand& cmd);
    void cmdToken(const TerminalParsedCommand& cmd);
    void cmdAccount(const TerminalParsedCommand& cmd);
    void cmdRpc(const TerminalParsedCommand& cmd);
    void cmdContact(const TerminalParsedCommand& cmd);
    void cmdPortfolio(const TerminalParsedCommand& cmd);
    void cmdProgram(const TerminalParsedCommand& cmd);
    void cmdEncode(const TerminalParsedCommand& cmd);
    void cmdDb(const TerminalParsedCommand& cmd);
    void cmdNonce(const TerminalParsedCommand& cmd);
    void cmdNetwork(const QStringList& args);
    void cmdStake(const TerminalParsedCommand& cmd);
    void cmdValidator(const TerminalParsedCommand& cmd);
    void cmdSwap(const TerminalParsedCommand& cmd);
    void cmdPrice(const QStringList& args);

  private:
    enum class TxDisplayMode { Inspect, Classify, Deltas, Cu };

    void trackPendingConnection(const QMetaObject::Connection& connection);
    void clearPendingConnections();
    void failActiveRequest(const QString& message);
    void fetchTx(const QString& sig,
                 const std::function<void(const QString&, const QJsonObject&)>& cb);
    void startRpcTimeout();
    void emitOutput(const QString& text, const QColor& color = QColor(255, 255, 255));
    void renderTxByMode(const QJsonObject& rawJson, TxDisplayMode mode);
    void renderTxClassify(const TransactionResponse& tx);
    void renderTxDeltas(const TransactionResponse& tx);
    void renderTxComputeUnits(const TransactionResponse& tx);
    void renderTxInspect(const TransactionResponse& tx);
    TerminalCommandContext commandContext(const QStringList& args);
    TerminalAsyncOp asyncOp();

    SolanaApi* m_api = nullptr;
    IdlRegistry* m_idlRegistry = nullptr;
    NetworkStatsService* m_networkStats = nullptr;
    PriceService* m_priceService = nullptr;
    ValidatorService* m_validatorService = nullptr;
    JupiterApi* m_jupiterApi = nullptr;
    QString m_walletAddress;
    Keypair m_keypair;
    Signer* m_signer = nullptr;
    std::function<void(const QString&, const QColor&)> m_outputSink;
    std::function<void(const QString&, const QString&)> m_secretSink;
    bool m_pendingConfirm = false;
    std::function<void()> m_pendingAction;
    TerminalRequestTracker m_pendingConns;
};

#endif // TERMINALHANDLER_H
