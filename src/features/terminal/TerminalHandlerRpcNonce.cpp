#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "crypto/Signer.h"
#include "db/NonceDb.h"
#include "services/SolanaApi.h"
#include "services/model/PriorityFee.h"
#include "tx/NonceAccount.h"
#include "tx/SystemOperationBuilder.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionExecutor.h"
#include "tx/TxExecutionConstants.h"

using namespace terminal;

void TerminalHandler::cmdRpc(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;

    switch (cmd.sub) {
        case TerminalSubcommand::None: {
            const QStringList urls = m_api->rpcUrls();
            emitOutput("  RPC Endpoints (" + QString::number(urls.size()) + "):", kDimColor);
            for (int i = 0; i < urls.size(); ++i) {
                emitOutput("    [" + QString::number(i) + "] " + urls[i]);
            }
            break;
        }

        case TerminalSubcommand::RpcSet: {
            if (args.size() < 3) {
                emitOutput("Usage: rpc set <url>", kDimColor);
                break;
            }
            m_api->setRpcUrl(args[2]);
            emitOutput("  ✓ RPC endpoint updated: " + args[2], kPromptColor);
            break;
        }

        case TerminalSubcommand::RpcAdd: {
            if (args.size() < 3) {
                emitOutput("Usage: rpc add <url>", kDimColor);
                break;
            }
            m_api->addRpcUrl(args[2]);
            emitOutput("  ✓ Added: " + args[2], kPromptColor);
            break;
        }

        case TerminalSubcommand::RpcRemove: {
            if (args.size() < 3) {
                emitOutput("Usage: rpc remove <url>", kDimColor);
                break;
            }
            m_api->removeRpcUrl(args[2]);
            emitOutput("  ✓ Removed: " + args[2], kPromptColor);
            break;
        }

        case TerminalSubcommand::RpcBlockhash: {
            emitOutput("Fetching latest blockhash...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(m_api, &SolanaApi::latestBlockhashReady, this,
                             [this](const QString& blockhash, quint64 lastValid) {
                                 cancelPending();
                                 emitOutput("  Blockhash:          " + blockhash);
                                 emitOutput("  Last Valid Height:  " + QString::number(lastValid),
                                            kDimColor);
                             }));
            op.run([this]() { m_api->fetchLatestBlockhash(); });
            break;
        }

        case TerminalSubcommand::RpcFees: {
            emitOutput("Fetching priority fees...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(m_api, &SolanaApi::prioritizationFeesReady, this,
                             [this](const QList<PriorityFee>& fees) {
                                 cancelPending();
                                 if (fees.isEmpty()) {
                                     emitOutput("  No fee data.", kDimColor);
                                     return;
                                 }
                                 quint64 minFee = UINT64_MAX, maxFee = 0, sum = 0;
                                 for (const auto& f : fees) {
                                     minFee = qMin(minFee, f.prioritizationFee);
                                     maxFee = qMax(maxFee, f.prioritizationFee);
                                     sum += f.prioritizationFee;
                                 }
                                 quint64 median = fees[fees.size() / 2].prioritizationFee;
                                 emitOutput("  Recent Priority Fees (" +
                                                QString::number(fees.size()) + " slots):",
                                            kDimColor);
                                 emitOutput("  Min:     " + QString::number(minFee) +
                                            QString::fromUtf8(" µL/CU"));
                                 emitOutput("  Median:  " + QString::number(median) +
                                            QString::fromUtf8(" µL/CU"));
                                 emitOutput("  Avg:     " + QString::number(sum / fees.size()) +
                                            QString::fromUtf8(" µL/CU"));
                                 emitOutput("  Max:     " + QString::number(maxFee) +
                                            QString::fromUtf8(" µL/CU"));
                             }));
            op.run([this]() { m_api->fetchRecentPrioritizationFees(); });
            break;
        }

        default:
            break;
    }
}

void TerminalHandler::cmdNonce(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: nonce <info|create|refresh|advance|withdraw|close>", kDimColor);
            break;

        case TerminalSubcommand::NonceInfo: {
            auto row = NonceDb::getByAuthorityRecord(m_walletAddress);
            if (!row.has_value()) {
                emitOutput("  No nonce account found for this wallet.", kDimColor);
                break;
            }
            emitOutput("  Address:    " + row->address, kPromptColor);
            emitOutput("  Authority:  " + row->authority, kDimColor);
            emitOutput("  Nonce:      " + row->nonceValue, kDimColor);
            emitOutput("  Created:    " + QString::number(row->createdAt), kDimColor);
            break;
        }

        case TerminalSubcommand::NonceCreate: {
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (NonceDb::hasNonceAccount(m_walletAddress)) {
                emitOutput("A nonce account already exists for this wallet.", kWarnColor);
                emitOutput("Use 'nonce info' to view it, or 'nonce close' to remove it.",
                           kDimColor);
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            emitOutput("Fetching rent-exempt minimum for nonce account (80 bytes)...", kDimColor);
            m_pendingConns << connect(
                m_api, &SolanaApi::minimumBalanceReady, this, [this](quint64 rentLamports) {
                    cancelPending();
                    double rentSol = rentLamports / 1e9;
                    emitOutput("  Rent cost: " + QString::number(rentSol, 'f', 6) + " SOL");
                    emitOutput("  Create nonce account? [y/n]", kDimColor);
                    m_pendingConfirm = true;
                    m_pendingAction = [this, rentLamports]() {
                        Keypair nonceKp = Keypair::generate();
                        QString nonceAddr = nonceKp.address();
                        emitOutput("  Nonce address: " + nonceAddr, kPromptColor);

                        NonceCreateOperationInput createInput;
                        createInput.walletAddress = m_walletAddress;
                        createInput.nonceAddress = nonceAddr;
                        createInput.authorityAddress = m_walletAddress;
                        createInput.rentLamports = rentLamports;
                        const NonceCreateOperationResult createResult =
                            SystemOperationBuilder::buildNonceCreateAccount(createInput);
                        if (!createResult.ok) {
                            emitOutput("Error building transaction.", kErrorColor);
                            return;
                        }
                        auto instructions = createResult.instructions;

                        SimpleTransactionInput txInput;
                        txInput.feePayer = m_walletAddress;
                        txInput.instructions = instructions;
                        txInput.api = m_api;
                        txInput.context = this;
                        txInput.signer = m_signer;
                        txInput.appendSignatures = AdditionalSigner::keypairSignatureAppender(
                            nonceKp, QStringLiteral("Failed to sign nonce keypair"));

                        SimpleTransactionCallbacks txCb;
                        txCb.onBroadcasting = [this]() {
                            emitOutput("  Sending transaction...", kDimColor);
                        };
                        txCb.onSent = [this, nonceAddr](const QString& txSig) {
                            cancelPending();
                            emitOutput("  TX sent: " + truncAddr(txSig), kPromptColor);
                            emitOutput("  Waiting for confirmation...", kDimColor);

                            int* attempt = new int(0);
                            QTimer* pollTimer = new QTimer(this);
                            pollTimer->setInterval(3000);
                            connect(pollTimer, &QTimer::timeout, this,
                                    [this, nonceAddr, attempt, pollTimer]() {
                                        ++(*attempt);
                                        if (*attempt > 20) {
                                            pollTimer->stop();
                                            pollTimer->deleteLater();
                                            delete attempt;
                                            emitOutput("  Timed out waiting for "
                                                       "confirmation.",
                                                       kErrorColor);
                                            emitOutput("  The account may still be "
                                                       "created. Try "
                                                       "'nonce refresh' later.",
                                                       kDimColor);
                                            return;
                                        }
                                        m_api->fetchNonceAccount(nonceAddr, "confirmed");
                                    });

                            auto pollConn = std::make_shared<QMetaObject::Connection>();
                            *pollConn =
                                connect(m_api, &SolanaApi::nonceAccountReady, this,
                                        [this, nonceAddr, attempt, pollTimer,
                                         pollConn](const QString& addr, const NonceAccount& nonce) {
                                            if (addr != nonceAddr) {
                                                return;
                                            }
                                            if (!nonce.isInitialized()) {
                                                return;
                                            }

                                            pollTimer->stop();
                                            pollTimer->deleteLater();
                                            delete attempt;
                                            disconnect(*pollConn);

                                            NonceDb::insertNonceAccount(nonceAddr, m_walletAddress,
                                                                        nonce.nonce);
                                            emitOutput("");
                                            emitOutput("  ✓ Nonce account created!", kPromptColor);
                                            emitOutput("  Address:  " + nonceAddr);
                                            emitOutput("  Nonce:    " + nonce.nonce, kDimColor);
                                        });

                            pollTimer->start();
                        };
                        txCb.onFailed = [this](const QString& error) {
                            cancelPending();
                            emitOutput(error, kErrorColor);
                        };
                        executeSimpleTransactionAsync(txInput, txCb);
                    };
                });
            startRpcTimeout();
            m_api->fetchMinimumBalanceForRentExemption(
                TxExecutionConstants::NonceAccountSpaceBytes);
            break;
        }

        case TerminalSubcommand::NonceRefresh: {
            auto row = NonceDb::getByAuthorityRecord(m_walletAddress);
            if (!row.has_value()) {
                emitOutput("No nonce account found. Use 'nonce create' first.", kErrorColor);
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }
            QString addr = row->address;
            emitOutput("Fetching nonce account " + truncAddr(addr) + "...", kDimColor);
            m_pendingConns << connect(
                m_api, &SolanaApi::nonceAccountReady, this,
                [this, addr](const QString& retAddr, const NonceAccount& nonce) {
                    if (retAddr != addr) {
                        return;
                    }
                    cancelPending();
                    if (!nonce.isInitialized()) {
                        emitOutput("  Account exists but is not initialized.", kErrorColor);
                        return;
                    }
                    NonceDb::updateNonceValue(addr, nonce.nonce);
                    emitOutput("  ✓ Nonce value updated", kPromptColor);
                    emitOutput("  Nonce:      " + nonce.nonce, kDimColor);
                    emitOutput("  Authority:  " + nonce.authority, kDimColor);
                    emitOutput("  Fee:        " + QString::number(nonce.lamportsPerSignature) +
                                   " lamports/sig",
                               kDimColor);
                });
            startRpcTimeout();
            m_api->fetchNonceAccount(addr);
            break;
        }

        case TerminalSubcommand::NonceAdvance: {
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            auto row = NonceDb::getByAuthorityRecord(m_walletAddress);
            if (!row.has_value()) {
                emitOutput("No nonce account found. Use 'nonce create' first.", kErrorColor);
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }
            QString nonceAddr = row->address;
            emitOutput("Advancing nonce for " + truncAddr(nonceAddr) + "...", kDimColor);

            SimpleTransactionInput txInput;
            txInput.feePayer = m_walletAddress;
            txInput.instructions = {
                SystemOperationBuilder::buildNonceAdvance(nonceAddr, m_walletAddress)};
            txInput.api = m_api;
            txInput.context = this;
            txInput.signer = m_signer;

            SimpleTransactionCallbacks txCb;
            txCb.onBroadcasting = [this]() { emitOutput("  Sending transaction...", kDimColor); };
            txCb.onSent = [this, nonceAddr](const QString& txSig) {
                cancelPending();
                emitOutput("  TX sent: " + truncAddr(txSig), kPromptColor);
                emitOutput("  Fetching new nonce value...", kDimColor);

                QTimer::singleShot(3000, this, [this, nonceAddr]() {
                    m_pendingConns << connect(
                        m_api, &SolanaApi::nonceAccountReady, this,
                        [this, nonceAddr](const QString& addr, const NonceAccount& nonce) {
                            if (addr != nonceAddr) {
                                return;
                            }
                            cancelPending();
                            if (nonce.isInitialized()) {
                                NonceDb::updateNonceValue(nonceAddr, nonce.nonce);
                                emitOutput("  ✓ Nonce advanced", kPromptColor);
                                emitOutput("  New nonce: " + nonce.nonce, kDimColor);
                            } else {
                                emitOutput("  Nonce account not initialized.", kErrorColor);
                            }
                        });
                    startRpcTimeout();
                    m_api->fetchNonceAccount(nonceAddr, "confirmed");
                });
            };
            txCb.onFailed = [this](const QString& error) {
                cancelPending();
                emitOutput(error, kErrorColor);
            };
            executeSimpleTransactionAsync(txInput, txCb);
            break;
        }

        case TerminalSubcommand::NonceWithdraw: {
            if (!ctx.requireArgs(3, "Usage: nonce withdraw <sol_amount>")) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            auto row = NonceDb::getByAuthorityRecord(m_walletAddress);
            if (!row.has_value()) {
                emitOutput("No nonce account found.", kErrorColor);
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            bool ok = false;
            double solAmount = args[2].toDouble(&ok);
            if (!ok || solAmount <= 0) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }
            quint64 lamports = 0;
            if (!SolAmount::toLamports(solAmount, &lamports)) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }
            QString nonceAddr = row->address;

            emitOutput("Withdrawing " + QString::number(solAmount, 'f', 6) + " SOL from " +
                           truncAddr(nonceAddr) + "...",
                       kDimColor);

            SimpleTransactionInput txInput;
            txInput.feePayer = m_walletAddress;
            txInput.instructions = {SystemOperationBuilder::buildNonceWithdraw(
                nonceAddr, m_walletAddress, m_walletAddress, lamports)};
            txInput.api = m_api;
            txInput.context = this;
            txInput.signer = m_signer;

            SimpleTransactionCallbacks txCb;
            txCb.onBroadcasting = [this]() { emitOutput("  Sending transaction...", kDimColor); };
            txCb.onSent = [this](const QString& txSig) {
                cancelPending();
                emitOutput("  ✓ Withdrawal sent: " + truncAddr(txSig), kPromptColor);
            };
            txCb.onFailed = [this](const QString& error) {
                cancelPending();
                emitOutput(error, kErrorColor);
            };
            executeSimpleTransactionAsync(txInput, txCb);
            break;
        }

        case TerminalSubcommand::NonceClose: {
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            auto row = NonceDb::getByAuthorityRecord(m_walletAddress);
            if (!row.has_value()) {
                emitOutput("No nonce account found.", kErrorColor);
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }
            QString nonceAddr = row->address;

            emitOutput("Fetching nonce account balance...", kDimColor);
            m_pendingConns << connect(
                m_api, &SolanaApi::minimumBalanceReady, this,
                [this, nonceAddr](quint64 rentLamports) {
                    cancelPending();
                    double rentSol = rentLamports / 1e9;
                    emitOutput("  This will close the nonce account and recover ~" +
                               QString::number(rentSol, 'f', 6) + " SOL.");
                    emitOutput("  Proceed? [y/n]", kDimColor);

                    m_pendingConfirm = true;
                    m_pendingAction = [this, nonceAddr, rentLamports]() {
                        SimpleTransactionInput txInput;
                        txInput.feePayer = m_walletAddress;
                        txInput.instructions = {SystemOperationBuilder::buildNonceWithdraw(
                            nonceAddr, m_walletAddress, m_walletAddress, rentLamports)};
                        txInput.api = m_api;
                        txInput.context = this;
                        txInput.signer = m_signer;

                        SimpleTransactionCallbacks txCb;
                        txCb.onBroadcasting = [this]() {
                            emitOutput("  Sending transaction...", kDimColor);
                        };
                        txCb.onSent = [this, nonceAddr](const QString& txSig) {
                            cancelPending();
                            NonceDb::deleteNonceAccount(nonceAddr);
                            emitOutput("  ✓ Nonce account closed", kPromptColor);
                            emitOutput("  TX: " + truncAddr(txSig), kDimColor);
                        };
                        txCb.onFailed = [this](const QString& error) {
                            cancelPending();
                            emitOutput(error, kErrorColor);
                        };
                        executeSimpleTransactionAsync(txInput, txCb);
                    };
                });
            startRpcTimeout();
            m_api->fetchMinimumBalanceForRentExemption(
                TxExecutionConstants::NonceAccountSpaceBytes);
            break;
        }

        default:
            break;
    }
}
