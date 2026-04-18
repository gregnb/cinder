#include "TerminalHandler.h"
#include "TerminalCommandCatalog.h"
#include "TerminalHandlerCommon.h"

#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "services/SolanaApi.h"
#include "tx/KnownTokens.h"
#include "tx/SystemInstruction.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionExecutor.h"

#include <QDateTime>

using namespace terminal;

TerminalHandler::TerminalHandler(SolanaApi* api, IdlRegistry* idlRegistry,
                                 NetworkStatsService* networkStats, PriceService* priceService,
                                 ValidatorService* validatorService, JupiterApi* jupiterApi,
                                 QObject* parent)
    : QObject(parent), m_api(api), m_idlRegistry(idlRegistry), m_networkStats(networkStats),
      m_priceService(priceService), m_validatorService(validatorService), m_jupiterApi(jupiterApi),
      m_pendingConns(this) {
    m_pendingConns.setTimeoutHandler([this]() { failActiveRequest("Request timed out."); });

    connect(m_api, &SolanaApi::requestFailed, this,
            [this](const QString& method, const QString& error) {
                if (!hasPending()) {
                    return;
                }
                failActiveRequest("RPC error (" + method + "): " + error);
            });
}

void TerminalHandler::setWalletAddress(const QString& address) { m_walletAddress = address; }

void TerminalHandler::setKeypair(const Keypair& keypair) { m_keypair = keypair; }

void TerminalHandler::setSigner(Signer* signer) { m_signer = signer; }

void TerminalHandler::setOutputSink(
    const std::function<void(const QString&, const QColor&)>& sink) {
    m_outputSink = sink;
}

void TerminalHandler::setSecretSink(
    const std::function<void(const QString&, const QString&)>& sink) {
    m_secretSink = sink;
}

bool TerminalHandler::hasPending() const { return !m_pendingConns.isEmpty(); }

bool TerminalHandler::hasPendingConfirm() const { return m_pendingConfirm; }

void TerminalHandler::cancelPendingConfirm() {
    m_pendingConfirm = false;
    m_pendingAction = nullptr;
}

bool TerminalHandler::consumePendingConfirm(const QString& answer) {
    if (!m_pendingConfirm) {
        return false;
    }
    const QString lowered = answer.trimmed().toLower();
    if (lowered == "y" || lowered == "yes") {
        m_pendingConfirm = false;
        if (m_pendingAction) {
            m_pendingAction();
        }
        m_pendingAction = nullptr;
    } else {
        m_pendingConfirm = false;
        m_pendingAction = nullptr;
        emitOutput("Cancelled.", kDimColor);
    }
    return true;
}

void TerminalHandler::cancelPending(const QString& reason) {
    m_pendingConns.stopTimeout();
    clearPendingConnections();
    if (!reason.isEmpty()) {
        emitOutput("Cancelled: " + reason, kDimColor);
    }
}

void TerminalHandler::startRpcTimeout() { m_pendingConns.startTimeout(kRpcTimeoutMs); }

TerminalCommandContext TerminalHandler::commandContext(const QStringList& args) {
    return TerminalCommandContext(
        args, [this](const QString& text, const QColor& color) { emitOutput(text, color); });
}

TerminalAsyncOp TerminalHandler::asyncOp() {
    return TerminalAsyncOp(m_pendingConns, [this]() { startRpcTimeout(); });
}

void TerminalHandler::trackPendingConnection(const QMetaObject::Connection& connection) {
    m_pendingConns << connection;
}

void TerminalHandler::clearPendingConnections() { m_pendingConns.clear(); }

void TerminalHandler::failActiveRequest(const QString& message) {
    cancelPending();
    emitOutput(message, kErrorColor);
}

void TerminalHandler::emitOutput(const QString& text, const QColor& color) {
    if (m_outputSink) {
        m_outputSink(text, color);
    }
}

void TerminalHandler::cmdHelp(const QStringList& args) {
    Q_UNUSED(args)
    emitOutput("Cinder Terminal — commands:");
    QString currentGroup;
    const auto& entries = terminalHelpEntries();
    for (const auto& entry : entries) {
        if (entry.group != currentGroup) {
            currentGroup = entry.group;
            emitOutput("");
            emitOutput("  " + currentGroup, kPromptColor);
        }
        emitOutput("    " + padRight(entry.usage, 34) + entry.description, kDimColor);
    }
    emitOutput("");
}

void TerminalHandler::cmdAddress() {
    if (m_walletAddress.isEmpty()) {
        emitOutput("No wallet loaded.", kErrorColor);
        return;
    }
    emitOutput(m_walletAddress, kPromptColor);
}

void TerminalHandler::cmdBalance() {
    const auto ctx = commandContext({});
    if (!ctx.requireWallet(m_walletAddress)) {
        return;
    }
    emitOutput("Fetching balance...", kDimColor);
    auto op = asyncOp();
    op.watch(connect(m_api, &SolanaApi::balanceReady, this,
                     [this](const QString& addr, quint64 lamports) {
                         if (addr != m_walletAddress) {
                             return;
                         }
                         cancelPending();
                         emitOutput(formatSol(lamports) + "  (" + formatLamports(lamports) + ")");
                     }));
    op.run([this]() { m_api->fetchBalance(m_walletAddress); });
}

void TerminalHandler::cmdBalances() {
    const auto ctx = commandContext({});
    if (!ctx.requireWallet(m_walletAddress)) {
        return;
    }

    auto accounts = TokenAccountDb::getAccountsByOwnerRecords(m_walletAddress);
    if (accounts.isEmpty()) {
        emitOutput("No token accounts found in DB.", kDimColor);
        return;
    }

    emitOutput(padRight("  Token", 14) + padRight("Balance", 18) + "Mint", kDimColor);
    emitOutput("  " + QString(50, QChar(0x2500)), kDimColor);

    for (const auto& acct : accounts) {
        QString sym = acct.symbol;
        QString mint = acct.tokenAddress;
        double bal = acct.balance.toDouble();

        KnownToken known = resolveKnownToken(mint);
        if (!known.symbol.isEmpty()) {
            sym = known.symbol;
        }
        if (sym.isEmpty()) {
            sym = truncAddr(mint);
        }

        QString balStr = QString::number(bal, 'f', bal >= 1.0 ? 4 : 6);
        emitOutput("  " + padRight(sym, 12) + padRight(balStr, 18) + truncAddr(mint));
    }
}

void TerminalHandler::cmdSend(const QStringList& args) {
    const auto ctx = commandContext(args);
    if (!ctx.requireArgs(3, "Usage: send <to_address> <sol_amount>")) {
        return;
    }
    if (!ctx.requireWallet(m_walletAddress)) {
        return;
    }
    if (!ctx.requireSigner(m_signer)) {
        return;
    }
    if (!ctx.requireNoPending(hasPending())) {
        return;
    }

    const QString& toAddress = args[1];
    bool ok = false;
    double solAmount = args[2].toDouble(&ok);
    if (!ok || solAmount <= 0) {
        emitOutput("Invalid amount.", kErrorColor);
        return;
    }
    quint64 lamports = 0;
    if (!SolAmount::toLamports(solAmount, &lamports)) {
        emitOutput("Invalid amount.", kErrorColor);
        return;
    }

    emitOutput("  Send:  " + QString::number(solAmount, 'f', 6) + " SOL");
    emitOutput("  To:    " + truncAddr(toAddress), kDimColor);
    emitOutput("  Proceed? [y/n]", kDimColor);

    m_pendingConfirm = true;
    m_pendingAction = [this, toAddress, lamports]() {
        SimpleTransactionInput txInput;
        txInput.feePayer = m_walletAddress;
        txInput.instructions = {SystemInstruction::transfer(m_walletAddress, toAddress, lamports)};
        txInput.api = m_api;
        txInput.context = this;
        txInput.signer = m_signer;

        SimpleTransactionCallbacks txCb;
        txCb.onBroadcasting = [this]() { emitOutput("  Sending transaction...", kDimColor); };
        txCb.onSent = [this](const QString& txSig) {
            cancelPending();
            emitOutput("  \u2713 SOL sent!", kPromptColor);
            emitOutput("  TX: " + truncAddr(txSig), kDimColor);
        };
        txCb.onFailed = [this](const QString& error) {
            cancelPending();
            emitOutput(error, kErrorColor);
        };
        executeSimpleTransactionAsync(txInput, txCb);
    };
}

void TerminalHandler::cmdHistory(const QStringList& args) {
    int limit = 10;
    if (args.size() > 1) {
        limit = args[1].toInt();
    }
    if (limit <= 0) {
        limit = 10;
    }

    auto txns = TransactionDb::getTransactionsRecords(m_walletAddress, {}, {}, limit);
    if (txns.isEmpty()) {
        emitOutput("No transactions in DB.", kDimColor);
        return;
    }

    emitOutput(padRight("  #", 5) + padRight("Signature", 14) + padRight("Date", 22) + "Status",
               kDimColor);
    emitOutput("  " + QString(55, QChar(0x2500)), kDimColor);

    for (int i = 0; i < txns.size(); ++i) {
        auto& tx = txns[i];
        QString sig = tx.signature;
        qint64 bt = tx.blockTime;
        bool err = tx.err;
        QString date = QDateTime::fromSecsSinceEpoch(bt).toString("yyyy-MM-dd hh:mm:ss");
        QString status = err ? "\xe2\x9c\x97 Failed" : "\xe2\x9c\x93 OK";
        QColor c = err ? kErrorColor : QColor(255, 255, 255);
        emitOutput("  " + padRight(QString::number(i + 1), 4) + padRight(truncAddr(sig), 14) +
                       padRight(date, 22) + status,
                   c);
    }
}
