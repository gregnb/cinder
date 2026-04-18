#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "crypto/Signer.h"
#include "services/SolanaApi.h"
#include "services/ValidatorService.h"
#include "services/model/ValidatorInfo.h"
#include "tx/SystemOperationBuilder.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionExecutor.h"
#include "tx/TxExecutionConstants.h"

using namespace terminal;

void TerminalHandler::cmdStake(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: stake <list|create|deactivate|withdraw>", kDimColor);
            break;

        case TerminalSubcommand::StakeList: {
            QString addr = (args.size() > 2) ? args[2] : m_walletAddress;
            if (!ctx.requireWallet(addr)) {
                break;
            }

            emitOutput("Fetching stake accounts for " + truncAddr(addr) + "...", kDimColor);
            m_pendingConns << connect(
                m_api, &SolanaApi::stakeAccountsReady, this,
                [this, addr](const QString& wallet, const QJsonArray& accounts) {
                    if (wallet != addr) {
                        return;
                    }
                    cancelPending();
                    if (accounts.isEmpty()) {
                        emitOutput("  No stake accounts found.", kDimColor);
                        return;
                    }
                    emitOutput(padRight("  #", 5) + padRight("Address", 16) +
                                   padRight("Stake (SOL)", 16) + "Status",
                               kDimColor);
                    emitOutput("  " + QString(50, QChar(0x2500)), kDimColor);

                    for (int i = 0; i < accounts.size(); ++i) {
                        QJsonObject acct = accounts[i].toObject();
                        QJsonObject parsed =
                            acct["account"].toObject()["data"].toObject()["parsed"].toObject();
                        QString stakeAddr = acct["pubkey"].toString();
                        quint64 lamports =
                            static_cast<quint64>(acct["account"].toObject()["lamports"].toDouble());
                        double sol = lamports / 1e9;

                        QString stateStr = parsed["type"].toString();
                        QJsonObject info = parsed["info"].toObject();
                        QJsonObject stake = info["stake"].toObject();
                        QJsonObject delegation = stake["delegation"].toObject();
                        QString voteAccount = delegation["voter"].toString();

                        QString status;
                        if (stateStr == "delegated") {
                            double deactivationEpoch =
                                delegation["deactivationEpoch"].toString().toDouble();
                            if (deactivationEpoch < 1.8e19) { // not max u64
                                status = "Deactivating";
                            } else {
                                status = "Active";
                            }
                        } else if (stateStr == "initialized") {
                            status = "Initialized";
                        } else {
                            status = stateStr.isEmpty() ? "Unknown" : stateStr;
                        }

                        QColor c = (status == "Active") ? kPromptColor : kDimColor;
                        emitOutput("  " + padRight(QString::number(i + 1), 4) +
                                       padRight(truncAddr(stakeAddr), 16) +
                                       padRight(QString::number(sol, 'f', 4), 16) + status,
                                   c);
                        if (!voteAccount.isEmpty()) {
                            emitOutput("      Validator: " + truncAddr(voteAccount), kDimColor);
                        }
                    }
                });
            startRpcTimeout();
            m_api->fetchStakeAccounts(addr);
            break;
        }

        case TerminalSubcommand::StakeCreate: {
            if (!ctx.requireArgs(4, "Usage: stake create <vote_account> <sol_amount>")) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            const QString& voteAccount = args[2];
            bool ok = false;
            double solAmount = args[3].toDouble(&ok);
            if (!ok || solAmount <= 0) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }
            quint64 lamports = 0;
            if (!SolAmount::toLamports(solAmount, &lamports)) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }

            emitOutput("Creating stake account and delegating " +
                           QString::number(solAmount, 'f', 4) + " SOL to " +
                           truncAddr(voteAccount) + "...",
                       kDimColor);

            m_pendingConns << connect(
                m_api, &SolanaApi::minimumBalanceReady, this,
                [this, voteAccount, lamports](quint64 rentLamports) {
                    cancelPending();
                    double total = (lamports + rentLamports) / 1e9;
                    emitOutput("  Stake:       " + formatSol(lamports));
                    emitOutput("  Rent:        " + formatSol(rentLamports), kDimColor);
                    emitOutput("  Total cost:  " + QString::number(total, 'f', 6) + " SOL");
                    emitOutput("  Delegate to: " + truncAddr(voteAccount), kDimColor);
                    emitOutput("  Proceed? [y/n]", kDimColor);

                    m_pendingConfirm = true;
                    m_pendingAction = [this, voteAccount, lamports, rentLamports]() {
                        Keypair stakeKp = Keypair::generate();
                        QString stakeAddr = stakeKp.address();
                        emitOutput("  Stake account: " + stakeAddr, kPromptColor);

                        StakeCreateAndDelegateInput stakeInput;
                        stakeInput.walletAddress = m_walletAddress;
                        stakeInput.stakeAddress = stakeAddr;
                        stakeInput.voteAccount = voteAccount;
                        stakeInput.stakeLamports = lamports;
                        stakeInput.rentLamports = rentLamports;
                        const StakeCreateAndDelegateResult stakeResult =
                            SystemOperationBuilder::buildStakeCreateAndDelegate(stakeInput);
                        if (!stakeResult.ok) {
                            emitOutput("Error building transaction.", kErrorColor);
                            return;
                        }
                        auto instructions = stakeResult.instructions;

                        SimpleTransactionInput txInput;
                        txInput.feePayer = m_walletAddress;
                        txInput.instructions = instructions;
                        txInput.api = m_api;
                        txInput.context = this;
                        txInput.signer = m_signer;
                        txInput.appendSignatures = AdditionalSigner::keypairSignatureAppender(
                            stakeKp, QStringLiteral("Failed to sign stake keypair"));

                        SimpleTransactionCallbacks txCb;
                        txCb.onBroadcasting = [this]() {
                            emitOutput("  Sending transaction...", kDimColor);
                        };
                        txCb.onSent = [this](const QString& txSig) {
                            cancelPending();
                            emitOutput("  ✓ Stake created and delegated!", kPromptColor);
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
                TxExecutionConstants::StakeAccountSpaceBytes);
            break;
        }

        case TerminalSubcommand::StakeDeactivate: {
            if (!ctx.requireArgs(3, "Usage: stake deactivate <stake_account>")) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            const QString& stakeAccount = args[2];
            emitOutput("Deactivating stake account " + truncAddr(stakeAccount) + "...", kDimColor);

            SimpleTransactionInput txInput;
            txInput.feePayer = m_walletAddress;
            txInput.instructions = {
                SystemOperationBuilder::buildStakeDeactivate(stakeAccount, m_walletAddress)};
            txInput.api = m_api;
            txInput.context = this;
            txInput.signer = m_signer;

            SimpleTransactionCallbacks txCb;
            txCb.onBroadcasting = [this]() { emitOutput("  Sending transaction...", kDimColor); };
            txCb.onSent = [this](const QString& txSig) {
                cancelPending();
                emitOutput("  ✓ Stake deactivated!", kPromptColor);
                emitOutput("  TX: " + truncAddr(txSig), kDimColor);
                emitOutput("  Note: Cooldown period is ~2 epochs before withdrawal.", kDimColor);
            };
            txCb.onFailed = [this](const QString& error) {
                cancelPending();
                emitOutput(error, kErrorColor);
            };
            executeSimpleTransactionAsync(txInput, txCb);
            break;
        }

        case TerminalSubcommand::StakeWithdraw: {
            if (!ctx.requireArgs(4, "Usage: stake withdraw <stake_account> <sol_amount>")) {
                break;
            }
            if (!ctx.requireSigner(m_signer)) {
                break;
            }
            if (!ctx.requireNoPending(hasPending())) {
                break;
            }

            const QString& stakeAccount = args[2];
            bool ok = false;
            double solAmount = args[3].toDouble(&ok);
            if (!ok || solAmount <= 0) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }
            quint64 lamports = 0;
            if (!SolAmount::toLamports(solAmount, &lamports)) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }

            emitOutput("Withdrawing " + QString::number(solAmount, 'f', 4) + " SOL from " +
                           truncAddr(stakeAccount) + "...",
                       kDimColor);

            SimpleTransactionInput txInput;
            txInput.feePayer = m_walletAddress;
            txInput.instructions = {SystemOperationBuilder::buildStakeWithdraw(
                stakeAccount, m_walletAddress, m_walletAddress, lamports)};
            txInput.api = m_api;
            txInput.context = this;
            txInput.signer = m_signer;

            SimpleTransactionCallbacks txCb;
            txCb.onBroadcasting = [this]() { emitOutput("  Sending transaction...", kDimColor); };
            txCb.onSent = [this](const QString& txSig) {
                cancelPending();
                emitOutput("  ✓ Withdrawal sent!", kPromptColor);
                emitOutput("  TX: " + truncAddr(txSig), kDimColor);
            };
            txCb.onFailed = [this](const QString& error) {
                cancelPending();
                emitOutput(error, kErrorColor);
            };
            executeSimpleTransactionAsync(txInput, txCb);
            break;
        }

        default:
            break;
    }
}

void TerminalHandler::cmdValidator(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    if (!m_validatorService) {
        m_validatorService = new ValidatorService(m_api, this);
    }

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: validator <list|info>", kDimColor);
            break;

        case TerminalSubcommand::ValidatorList: {
            int topN = 20;
            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--top" && i + 1 < args.size()) {
                    topN = args[i + 1].toInt();
                    if (topN <= 0) {
                        topN = 20;
                    }
                }
            }

            if (!m_validatorService->validators().isEmpty()) {
                const auto& validators = m_validatorService->validators();
                int show = qMin(topN, static_cast<int>(validators.size()));
                emitOutput("  Top " + QString::number(show) + " Validators by Stake", kAccentColor);
                emitOutput("  " + QString(68, QChar(0x2500)), kDimColor);
                emitOutput("  " + padRight("#", 4) + padRight("Name", 24) + padRight("Stake", 14) +
                               padRight("APY", 8) + padRight("Comm", 6) + "Vote Account",
                           kDimColor);
                emitOutput("  " + QString(68, QChar(0x2500)), kDimColor);

                for (int i = 0; i < show; ++i) {
                    const auto& v = validators[i];
                    QString name = v.name.isEmpty() ? truncAddr(v.voteAccount) : v.name;
                    if (name.length() > 22) {
                        name = name.left(20) + "..";
                    }
                    double stakeM = v.stakeInSol() / 1e6;
                    QString stakeStr = (stakeM >= 1.0) ? QString::number(stakeM, 'f', 2) + "M"
                                                       : QString::number(v.stakeInSol(), 'f', 0);
                    QColor c = v.delinquent ? kErrorColor : QColor(255, 255, 255);
                    emitOutput("  " + padRight(QString::number(i + 1), 4) + padRight(name, 24) +
                                   padRight(stakeStr, 14) +
                                   padRight(QString::number(v.apy, 'f', 2) + "%", 8) +
                                   padRight(QString::number(v.commission) + "%", 6) +
                                   truncAddr(v.voteAccount),
                               c);
                }
                emitOutput("");
                emitOutput("  Epoch: " + QString::number(m_validatorService->currentEpoch()) +
                               "  |  Total validators: " + QString::number(validators.size()),
                           kDimColor);
                break;
            }

            emitOutput("Fetching validator data...", kDimColor);
            m_pendingConns << connect(
                m_validatorService, &ValidatorService::validatorsReady, this,
                [this, topN](const QList<ValidatorInfo>& validators) {
                    cancelPending();
                    if (validators.isEmpty()) {
                        emitOutput("  No validator data available.", kDimColor);
                        return;
                    }
                    int show = qMin(topN, static_cast<int>(validators.size()));
                    emitOutput("  Top " + QString::number(show) + " Validators by Stake",
                               kAccentColor);
                    emitOutput("  " + QString(68, QChar(0x2500)), kDimColor);
                    emitOutput("  " + padRight("#", 4) + padRight("Name", 24) +
                                   padRight("Stake", 14) + padRight("APY", 8) +
                                   padRight("Comm", 6) + "Vote Account",
                               kDimColor);
                    emitOutput("  " + QString(68, QChar(0x2500)), kDimColor);

                    for (int i = 0; i < show; ++i) {
                        const auto& v = validators[i];
                        QString name = v.name.isEmpty() ? truncAddr(v.voteAccount) : v.name;
                        if (name.length() > 22) {
                            name = name.left(20) + "..";
                        }
                        double stakeM = v.stakeInSol() / 1e6;
                        QString stakeStr = (stakeM >= 1.0)
                                               ? QString::number(stakeM, 'f', 2) + "M"
                                               : QString::number(v.stakeInSol(), 'f', 0);
                        QColor c = v.delinquent ? kErrorColor : QColor(255, 255, 255);
                        emitOutput("  " + padRight(QString::number(i + 1), 4) + padRight(name, 24) +
                                       padRight(stakeStr, 14) +
                                       padRight(QString::number(v.apy, 'f', 2) + "%", 8) +
                                       padRight(QString::number(v.commission) + "%", 6) +
                                       truncAddr(v.voteAccount),
                                   c);
                    }
                    emitOutput("");
                    emitOutput("  Epoch: " + QString::number(m_validatorService->currentEpoch()) +
                                   "  |  Total validators: " + QString::number(validators.size()),
                               kDimColor);
                });
            m_pendingConns << connect(m_validatorService, &ValidatorService::error, this,
                                      [this](const QString& msg) {
                                          cancelPending();
                                          emitOutput("Error: " + msg, kErrorColor);
                                      });
            startRpcTimeout();
            m_validatorService->refresh();
            break;
        }

        case TerminalSubcommand::ValidatorInfo: {
            if (!ctx.requireArgs(3, "Usage: validator info <vote_account>")) {
                break;
            }
            const QString& voteAccount = args[2];

            const auto& validators = m_validatorService->validators();
            if (!validators.isEmpty()) {
                for (const auto& v : validators) {
                    if (v.voteAccount == voteAccount) {
                        emitOutput("  VALIDATOR", kAccentColor);
                        emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
                        emitOutput("  Name:         " + (v.name.isEmpty() ? "(unknown)" : v.name));
                        emitOutput("  Vote Account: " + v.voteAccount);
                        emitOutput("  Node Pubkey:  " + v.nodePubkey, kDimColor);
                        emitOutput("  Stake:        " + QString::number(v.stakeInSol(), 'f', 2) +
                                       " SOL",
                                   kDimColor);
                        emitOutput("  APY:          " + QString::number(v.apy, 'f', 2) + "%",
                                   v.apy > 0 ? kPromptColor : kDimColor);
                        emitOutput("  Commission:   " + QString::number(v.commission) + "%",
                                   kDimColor);
                        if (!v.version.isEmpty()) {
                            emitOutput("  Version:      " + v.version, kDimColor);
                        }
                        if (!v.city.isEmpty()) {
                            emitOutput("  Location:     " + v.city +
                                           (v.country.isEmpty() ? "" : ", " + v.country),
                                       kDimColor);
                        }
                        emitOutput("  Delinquent:   " + QString(v.delinquent ? "Yes" : "No"),
                                   v.delinquent ? kErrorColor : kDimColor);
                        if (v.superminority) {
                            emitOutput("  Superminority: Yes", kWarnColor);
                        }
                        break;
                    }
                }
                if (std::none_of(validators.begin(), validators.end(), [&](const ValidatorInfo& v) {
                        return v.voteAccount == voteAccount;
                    })) {
                    emitOutput("  Validator not found: " + truncAddr(voteAccount), kErrorColor);
                    emitOutput("  Run 'validator list' first to load data.", kDimColor);
                }
                break;
            }

            emitOutput("Fetching validator data...", kDimColor);
            m_pendingConns << connect(
                m_validatorService, &ValidatorService::validatorsReady, this,
                [this, voteAccount](const QList<ValidatorInfo>& validators) {
                    cancelPending();
                    for (const auto& v : validators) {
                        if (v.voteAccount == voteAccount) {
                            emitOutput("  VALIDATOR", kAccentColor);
                            emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
                            emitOutput("  Name:         " +
                                       (v.name.isEmpty() ? "(unknown)" : v.name));
                            emitOutput("  Vote Account: " + v.voteAccount);
                            emitOutput("  Node Pubkey:  " + v.nodePubkey, kDimColor);
                            emitOutput("  Stake:        " +
                                           QString::number(v.stakeInSol(), 'f', 2) + " SOL",
                                       kDimColor);
                            emitOutput("  APY:          " + QString::number(v.apy, 'f', 2) + "%",
                                       v.apy > 0 ? kPromptColor : kDimColor);
                            emitOutput("  Commission:   " + QString::number(v.commission) + "%",
                                       kDimColor);
                            if (!v.version.isEmpty()) {
                                emitOutput("  Version:      " + v.version, kDimColor);
                            }
                            if (!v.city.isEmpty()) {
                                emitOutput("  Location:     " + v.city +
                                               (v.country.isEmpty() ? "" : ", " + v.country),
                                           kDimColor);
                            }
                            emitOutput("  Delinquent:   " + QString(v.delinquent ? "Yes" : "No"),
                                       v.delinquent ? kErrorColor : kDimColor);
                            if (v.superminority) {
                                emitOutput("  Superminority: Yes", kWarnColor);
                            }
                            return;
                        }
                    }
                    emitOutput("  Validator not found: " + truncAddr(voteAccount), kErrorColor);
                });
            m_pendingConns << connect(m_validatorService, &ValidatorService::error, this,
                                      [this](const QString& msg) {
                                          cancelPending();
                                          emitOutput("Error: " + msg, kErrorColor);
                                      });
            startRpcTimeout();
            m_validatorService->refresh();
            break;
        }

        default:
            break;
    }
}
