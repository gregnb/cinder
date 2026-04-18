#include "StakingHandler.h"
#include "crypto/Keypair.h"
#include "crypto/Signer.h"
#include "services/SolanaApi.h"
#include "tx/StakeInstruction.h"
#include "tx/TransactionBuilder.h"
#include <memory>

StakingHandler::StakingHandler(QObject* parent) : QObject(parent) {}

void StakingHandler::setSolanaApi(SolanaApi* api) { m_solanaApi = api; }

void StakingHandler::setSigner(Signer* signer) { m_signer = signer; }

bool StakingHandler::validateDependencies(StakingAction action) {
    if (m_signer && m_solanaApi) {
        return true;
    }
    emitActionFailed(action, "wallet_locked");
    return false;
}

void StakingHandler::emitActionFailed(StakingAction action, const QString& code,
                                      const QString& message) {
    emit actionUpdated({action, StakingActionPhase::Failed, {}, {}, code, message});
}

void StakingHandler::requestLatestBlockhash(const std::function<void(const QString&)>& onReady,
                                            StakingAction action) {
    if (!m_solanaApi) {
        emitActionFailed(action, "wallet_locked");
        return;
    }

    auto failConn = std::make_shared<QMetaObject::Connection>();
    auto bhConn = std::make_shared<QMetaObject::Connection>();
    *bhConn = connect(m_solanaApi, &SolanaApi::latestBlockhashReady, this,
                      [bhConn, failConn, onReady](const QString& blockhash, quint64) {
                          disconnect(*bhConn);
                          disconnect(*failConn);
                          onReady(blockhash);
                      });

    *failConn =
        connect(m_solanaApi, &SolanaApi::requestFailed, this,
                [this, failConn, bhConn, action](const QString& method, const QString& error) {
                    if (method != "getLatestBlockhash") {
                        return;
                    }
                    disconnect(*failConn);
                    disconnect(*bhConn);
                    emitActionFailed(action, "rpc_blockhash_failed", error);
                });

    m_solanaApi->fetchLatestBlockhash();
}

void StakingHandler::signAndSend(const QByteArray& message,
                                 const std::function<QByteArray(const QByteArray&)>& buildSignedTx,
                                 StakingAction action, const QString& signFailedCode,
                                 const QString& signFailedMessage,
                                 const std::function<void(const QString&)>& onSubmitted) {
    if (!m_signer || !m_solanaApi) {
        emitActionFailed(action, "wallet_locked");
        return;
    }

    m_signer->signAsync(
        message, this,
        [this, buildSignedTx, action, signFailedCode, signFailedMessage,
         onSubmitted](const QByteArray& sig, const QString& err) {
            if (sig.isEmpty()) {
                QString msg = signFailedMessage.isEmpty() ? err : signFailedMessage;
                emitActionFailed(action, signFailedCode, msg);
                return;
            }

            QByteArray signedTx = buildSignedTx(sig);

            auto sentConn = std::make_shared<QMetaObject::Connection>();
            auto failConn = std::make_shared<QMetaObject::Connection>();

            *sentConn = connect(m_solanaApi, &SolanaApi::transactionSent, this,
                                [sentConn, failConn, onSubmitted](const QString& txSig) {
                                    disconnect(*sentConn);
                                    disconnect(*failConn);
                                    onSubmitted(txSig);
                                });

            *failConn = connect(
                m_solanaApi, &SolanaApi::requestFailed, this,
                [this, sentConn, failConn, action](const QString& method, const QString& error) {
                    if (method != "sendTransaction") {
                        return;
                    }
                    disconnect(*failConn);
                    disconnect(*sentConn);
                    emitActionFailed(action, "rpc_send_failed", error);
                });

            m_solanaApi->sendTransaction(signedTx);
        });
}

void StakingHandler::createStake(const StakeRequest& req) {
    if (!validateDependencies(StakingAction::Stake)) {
        return;
    }

    emit actionUpdated({StakingAction::Stake, StakingActionPhase::Building});

    Keypair stakeKp = Keypair::generate();
    QString stakeAddr = stakeKp.address();

    requestLatestBlockhash(
        [this, req, stakeKp, stakeAddr](const QString& blockhash) mutable {
            QList<TransactionInstruction> ixs =
                StakeInstruction::createAndDelegate(req.walletAddress, stakeAddr, req.voteAccount,
                                                    req.lamports, req.rentExemptLamports);

            TransactionBuilder builder;
            builder.setFeePayer(req.walletAddress).setRecentBlockhash(blockhash);
            for (const auto& ix : ixs) {
                builder.addInstruction(ix);
            }

            QByteArray message = builder.serializeMessage();
            if (message.isEmpty()) {
                emitActionFailed(StakingAction::Stake, "build_failed");
                return;
            }

            emit actionUpdated({StakingAction::Stake, StakingActionPhase::Sending});

            signAndSend(
                message,
                [stakeKp, builder, message](const QByteArray& walletSig) mutable {
                    QByteArray stakeSig = stakeKp.sign(message);
                    return builder.buildSigned({walletSig, stakeSig});
                },
                StakingAction::Stake, "sign_failed", {},
                [this, stakeAddr](const QString& txSig) {
                    emit actionUpdated(
                        {StakingAction::Stake, StakingActionPhase::Submitted, txSig, stakeAddr});
                    emit refreshRequested({true, true, 3000});
                });
        },
        StakingAction::Stake);
}

void StakingHandler::deactivateStake(const DeactivateRequest& req) {
    if (!validateDependencies(StakingAction::Deactivate)) {
        return;
    }

    requestLatestBlockhash(
        [this, req](const QString& blockhash) {
            TransactionInstruction ix =
                StakeInstruction::deactivate(req.stakeAccount, req.walletAddress);
            TransactionBuilder builder;
            builder.setFeePayer(req.walletAddress).setRecentBlockhash(blockhash).addInstruction(ix);

            QByteArray message = builder.serializeMessage();
            if (message.isEmpty()) {
                emitActionFailed(StakingAction::Deactivate, "build_failed");
                return;
            }

            signAndSend(
                message,
                [builder](const QByteArray& sig) mutable { return builder.buildSigned({sig}); },
                StakingAction::Deactivate, "sign_failed",
                QObject::tr("Signing was cancelled or failed."),
                [this](const QString&) {
                    emit actionUpdated({StakingAction::Deactivate, StakingActionPhase::Submitted});
                    emit refreshRequested({true, false, 3000});
                });
        },
        StakingAction::Deactivate);
}

void StakingHandler::withdrawStake(const WithdrawRequest& req) {
    if (!validateDependencies(StakingAction::Withdraw)) {
        return;
    }

    requestLatestBlockhash(
        [this, req](const QString& blockhash) {
            TransactionInstruction ix = StakeInstruction::withdraw(
                req.stakeAccount, req.walletAddress, req.walletAddress, req.lamports);
            TransactionBuilder builder;
            builder.setFeePayer(req.walletAddress).setRecentBlockhash(blockhash).addInstruction(ix);

            QByteArray message = builder.serializeMessage();
            if (message.isEmpty()) {
                emitActionFailed(StakingAction::Withdraw, "build_failed");
                return;
            }

            signAndSend(
                message,
                [builder](const QByteArray& sig) mutable { return builder.buildSigned({sig}); },
                StakingAction::Withdraw, "sign_failed",
                QObject::tr("Signing was cancelled or failed."),
                [this](const QString&) {
                    emit actionUpdated({StakingAction::Withdraw, StakingActionPhase::Submitted});
                    emit refreshRequested({true, true, 3000});
                });
        },
        StakingAction::Withdraw);
}
