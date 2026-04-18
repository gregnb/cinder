#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "db/TokenAccountDb.h"
#include "services/SolanaApi.h"
#include "tx/AssociatedTokenInstruction.h"
#include "tx/KnownTokens.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TokenInstruction.h"
#include "tx/TransactionExecutor.h"
#include "tx/TxParseUtils.h"

using namespace terminal;

void TerminalHandler::cmdToken(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: token <info|send|burn|close|accounts|ata>", kDimColor);
            break;

        case TerminalSubcommand::TokenInfo: {
            if (!ctx.requireArgs(3, "Usage: token info <mint>")) {
                break;
            }
            const QString& mint = args[2];
            auto info = TokenAccountDb::getTokenRecord(mint);
            KnownToken known = resolveKnownToken(mint);
            QString sym = known.symbol.isEmpty() ? (info.has_value() ? info->symbol : QString())
                                                 : known.symbol;
            QString name = info.has_value() ? info->name : QString();
            int decimals = info.has_value() ? info->decimals : 0;
            emitOutput("  Mint:      " + mint);
            emitOutput("  Symbol:    " + (sym.isEmpty() ? "(unknown)" : sym));
            emitOutput("  Name:      " + (name.isEmpty() ? "(unknown)" : name), kDimColor);
            emitOutput("  Decimals:  " + QString::number(decimals), kDimColor);
            break;
        }

        case TerminalSubcommand::TokenAccounts: {
            if (!ctx.requireArgs(3, "Usage: token accounts <address>")) {
                break;
            }
            emitOutput("Fetching token accounts...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(
                m_api, &SolanaApi::tokenAccountsReady, this,
                [this, args](const QString& owner, const QJsonArray& accounts) {
                    if (owner != args[2]) {
                        return;
                    }
                    cancelPending();
                    if (accounts.isEmpty()) {
                        emitOutput("  No token accounts found.", kDimColor);
                        return;
                    }
                    emitOutput(padRight("  Mint", 16) + padRight("Balance", 18) + "Symbol",
                               kDimColor);
                    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
                    for (const auto& acctVal : accounts) {
                        QJsonObject acct = acctVal.toObject();
                        QJsonObject parsed = acct["account"]
                                                 .toObject()["data"]
                                                 .toObject()["parsed"]
                                                 .toObject()["info"]
                                                 .toObject();
                        QString mint = parsed["mint"].toString();
                        double bal = parsed["tokenAmount"].toObject()["uiAmount"].toDouble();
                        KnownToken kt = resolveKnownToken(mint);
                        QString sym = kt.symbol.isEmpty() ? "?" : kt.symbol;
                        emitOutput("  " + padRight(truncAddr(mint), 14) +
                                   padRight(QString::number(bal, 'f', 6), 18) + sym);
                    }
                }));
            op.run([this, args]() { m_api->fetchTokenAccountsByOwner(args[2]); });
            break;
        }

        case TerminalSubcommand::TokenAta: {
            if (!ctx.requireArgs(4, "Usage: token ata <owner> <mint>")) {
                break;
            }
            const QString& owner = args[2];
            const QString& mint = args[3];
            QString ata = AssociatedTokenInstruction::deriveAddress(owner, mint);
            if (ata.isEmpty()) {
                emitOutput("Failed to derive ATA (invalid owner or mint).", kErrorColor);
                break;
            }
            emitOutput("  Owner:  " + owner, kDimColor);
            emitOutput("  Mint:   " + mint, kDimColor);
            emitOutput("  ATA:    " + ata, kPromptColor);
            break;
        }

        case TerminalSubcommand::TokenSend: {
            if (!ctx.requireArgs(5, "Usage: token send <mint> <to_address> <amount>")) {
                break;
            }
            if (!ctx.requireWallet(m_walletAddress)) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            const QString& mint = args[2];
            const QString& toAddress = args[3];
            auto acct = TokenAccountDb::getAccountRecord(m_walletAddress, mint);
            if (!acct.has_value()) {
                emitOutput("No token account found for mint " + truncAddr(mint), kErrorColor);
                break;
            }

            bool ok = false;
            double amount = args[4].toDouble(&ok);
            if (!ok || amount <= 0) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }
            quint64 rawAmount = 0;
            if (!TokenAmountCodec::toRaw(amount, acct->decimals, &rawAmount)) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }

            QString sym = acct->symbol.isEmpty() ? truncAddr(mint) : acct->symbol;
            emitOutput("  Send:  " + QString::number(amount, 'f', acct->decimals) + " " + sym);
            emitOutput("  To:    " + truncAddr(toAddress), kDimColor);
            emitOutput("  Proceed? [y/n]", kDimColor);

            QString sourceAta = acct->accountAddress;
            int decimals = acct->decimals;
            QString tokenProgram = acct->tokenProgram;
            m_pendingConfirm = true;
            m_pendingAction = [this, mint, toAddress, rawAmount, sourceAta, decimals,
                               tokenProgram]() {
                QString destAta =
                    AssociatedTokenInstruction::deriveAddress(toAddress, mint, tokenProgram);
                if (destAta.isEmpty()) {
                    emitOutput("Failed to derive recipient ATA.", kErrorColor);
                    return;
                }

                QList<TransactionInstruction> instructions;
                instructions.append(AssociatedTokenInstruction::createIdempotent(
                    m_walletAddress, destAta, toAddress, mint, tokenProgram));
                instructions.append(TokenInstruction::transferChecked(
                    sourceAta, mint, destAta, m_walletAddress, rawAmount,
                    static_cast<uint8_t>(decimals), tokenProgram));

                SimpleTransactionInput txInput;
                txInput.feePayer = m_walletAddress;
                txInput.instructions = instructions;
                txInput.api = m_api;
                txInput.context = this;
                txInput.signer = m_signer;

                SimpleTransactionCallbacks txCb;
                txCb.onBroadcasting = [this]() {
                    emitOutput("  Sending transaction...", kDimColor);
                };
                txCb.onSent = [this](const QString& txSig) {
                    cancelPending();
                    emitOutput("  \u2713 Tokens sent!", kPromptColor);
                    emitOutput("  TX: " + truncAddr(txSig), kDimColor);
                };
                txCb.onFailed = [this](const QString& error) {
                    cancelPending();
                    emitOutput(error, kErrorColor);
                };
                executeSimpleTransactionAsync(txInput, txCb);
            };
            break;
        }

        case TerminalSubcommand::TokenBurn: {
            if (!ctx.requireArgs(4, "Usage: token burn <mint> <amount>")) {
                break;
            }
            if (!ctx.requireWallet(m_walletAddress)) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            const QString& mint = args[2];
            auto acct = TokenAccountDb::getAccountRecord(m_walletAddress, mint);
            if (!acct.has_value()) {
                emitOutput("No token account found for mint " + truncAddr(mint), kErrorColor);
                break;
            }

            bool ok = false;
            double amount = args[3].toDouble(&ok);
            if (!ok || amount <= 0) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }
            quint64 rawAmount = 0;
            if (!TokenAmountCodec::toRaw(amount, acct->decimals, &rawAmount)) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }

            QString sym = acct->symbol.isEmpty() ? truncAddr(mint) : acct->symbol;
            emitOutput("  Burn:  " + QString::number(amount, 'f', acct->decimals) + " " + sym);
            emitOutput("  Proceed? [y/n]", kDimColor);

            QString tokenAccount = acct->accountAddress;
            QString tokenProgram = acct->tokenProgram;
            m_pendingConfirm = true;
            m_pendingAction = [this, tokenAccount, mint, rawAmount, tokenProgram]() {
                SimpleTransactionInput txInput;
                txInput.feePayer = m_walletAddress;
                txInput.instructions = {TokenInstruction::burn(tokenAccount, mint, m_walletAddress,
                                                               rawAmount, tokenProgram)};
                txInput.api = m_api;
                txInput.context = this;
                txInput.signer = m_signer;

                SimpleTransactionCallbacks txCb;
                txCb.onBroadcasting = [this]() {
                    emitOutput("  Sending transaction...", kDimColor);
                };
                txCb.onSent = [this](const QString& txSig) {
                    cancelPending();
                    emitOutput("  \u2713 Tokens burned!", kPromptColor);
                    emitOutput("  TX: " + truncAddr(txSig), kDimColor);
                };
                txCb.onFailed = [this](const QString& error) {
                    cancelPending();
                    emitOutput(error, kErrorColor);
                };
                executeSimpleTransactionAsync(txInput, txCb);
            };
            break;
        }

        case TerminalSubcommand::TokenClose: {
            if (!ctx.requireArgs(3, "Usage: token close <mint>")) {
                break;
            }
            if (!ctx.requireWallet(m_walletAddress)) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            const QString& mint = args[2];
            auto acct = TokenAccountDb::getAccountRecord(m_walletAddress, mint);
            if (!acct.has_value()) {
                emitOutput("No token account found for mint " + truncAddr(mint), kErrorColor);
                break;
            }

            double bal = acct->balance.toDouble();
            if (bal > 0) {
                emitOutput("Token account still has a balance of " +
                               QString::number(bal, 'f', acct->decimals) +
                               ". Burn or send "
                               "tokens first.",
                           kErrorColor);
                break;
            }

            QString sym = acct->symbol.isEmpty() ? truncAddr(mint) : acct->symbol;
            emitOutput("  Close token account for " + sym);
            emitOutput("  Rent (~0.002 SOL) will be returned to your wallet.", kDimColor);
            emitOutput("  Proceed? [y/n]", kDimColor);

            QString tokenAccount = acct->accountAddress;
            QString tokenProgram = acct->tokenProgram;
            m_pendingConfirm = true;
            m_pendingAction = [this, tokenAccount, tokenProgram]() {
                SimpleTransactionInput txInput;
                txInput.feePayer = m_walletAddress;
                txInput.instructions = {TokenInstruction::closeAccount(
                    tokenAccount, m_walletAddress, m_walletAddress, tokenProgram)};
                txInput.api = m_api;
                txInput.context = this;
                txInput.signer = m_signer;

                SimpleTransactionCallbacks txCb;
                txCb.onBroadcasting = [this]() {
                    emitOutput("  Sending transaction...", kDimColor);
                };
                txCb.onSent = [this](const QString& txSig) {
                    cancelPending();
                    emitOutput("  \u2713 Token account closed!", kPromptColor);
                    emitOutput("  TX: " + truncAddr(txSig), kDimColor);
                };
                txCb.onFailed = [this](const QString& error) {
                    cancelPending();
                    emitOutput(error, kErrorColor);
                };
                executeSimpleTransactionAsync(txInput, txCb);
            };
            break;
        }

        default:
            break;
    }
}

void TerminalHandler::cmdAccount(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: account <nonce|rent>", kDimColor);
            break;

        case TerminalSubcommand::AccountNonce: {
            if (!ctx.requireArgs(3, "Usage: account nonce <address>")) {
                break;
            }
            emitOutput("Fetching nonce account...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(
                m_api, &SolanaApi::nonceAccountReady, this,
                [this, args](const QString& addr, const NonceAccount& nonce) {
                    if (addr != args[2]) {
                        return;
                    }
                    cancelPending();
                    emitOutput("  State:      " +
                                   QString(nonce.isInitialized() ? "Initialized" : "Uninitialized"),
                               nonce.isInitialized() ? kPromptColor : kErrorColor);
                    emitOutput("  Authority:  " + nonce.authority);
                    emitOutput("  Nonce:      " + nonce.nonce, kDimColor);
                    emitOutput("  Fee:        " + QString::number(nonce.lamportsPerSignature) +
                                   " lamports/sig",
                               kDimColor);
                }));
            op.run([this, args]() { m_api->fetchNonceAccount(args[2]); });
            break;
        }

        case TerminalSubcommand::AccountRent: {
            if (!ctx.requireArgs(3, "Usage: account rent <bytes>")) {
                break;
            }
            quint64 bytes = args[2].toULongLong();
            emitOutput("Fetching rent-exempt minimum...", kDimColor);
            auto op = asyncOp();
            op.watch(
                connect(m_api, &SolanaApi::minimumBalanceReady, this, [this](quint64 lamports) {
                    cancelPending();
                    emitOutput("  Rent-exempt minimum: " + formatLamports(lamports) + " (" +
                               formatSol(lamports) + ")");
                }));
            op.run([this, bytes]() { m_api->fetchMinimumBalanceForRentExemption(bytes); });
            break;
        }

        default:
            break;
    }
}
