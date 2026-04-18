#include "features/sendreceive/SendReceiveHandler.h"

#include "crypto/Keypair.h"
#include "crypto/Signer.h"
#include "db/NonceDb.h"
#include "services/SolanaApi.h"
#include "tx/ComputeBudgetInstruction.h"
#include "tx/SystemOperationBuilder.h"
#include "tx/TransactionExecutor.h"
#include "tx/TxExecutionConstants.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

namespace {
    constexpr quint64 kNonceAccountSpace = TxExecutionConstants::NonceAccountSpaceBytes;
    constexpr int kRpcLookupTimeoutMs = TxExecutionConstants::RpcLookupTimeoutMs;
    constexpr int kRpcShortLookupTimeoutMs = TxExecutionConstants::RpcShortLookupTimeoutMs;
    constexpr const char* kMethodAccountInfo = TxExecutionConstants::MethodAccountInfo;
} // namespace

void SendReceiveHandler::executeCreateNonceFlow(const QString& walletAddress, quint64 rentLamports,
                                                SolanaApi* solanaApi, Signer* signer,
                                                QObject* context,
                                                const CreateNonceCallbacks& callbacks) const {
    const auto progress = [callbacks](const QString& text) {
        if (callbacks.onProgress) {
            callbacks.onProgress(text);
        }
    };
    const auto fail = [callbacks](const QString& text) {
        if (callbacks.onError) {
            callbacks.onError(text);
        }
    };

    if (!solanaApi || !signer || !context) {
        fail(QCoreApplication::translate("SendReceivePage",
                                         "Error: wallet not available for signing."));
        return;
    }
    if (rentLamports == 0) {
        fail(QCoreApplication::translate("SendReceivePage", "Error: rent cost not available."));
        return;
    }

    progress(QCoreApplication::translate("SendReceivePage", "Generating nonce account keypair"));
    const Keypair nonceKeypair = Keypair::generate();
    const QString nonceAddress = nonceKeypair.address();
    NonceCreateOperationInput createInput;
    createInput.walletAddress = walletAddress;
    createInput.nonceAddress = nonceAddress;
    createInput.authorityAddress = walletAddress;
    createInput.rentLamports = rentLamports;
    const NonceCreateOperationResult createResult =
        SystemOperationBuilder::buildNonceCreateAccount(createInput);
    if (!createResult.ok) {
        fail(QCoreApplication::translate("SendReceivePage", "Error building transaction."));
        return;
    }
    const auto instructions = createResult.instructions;

    progress(QCoreApplication::translate("SendReceivePage", "Building and signing transaction"));

    SimpleTransactionInput txInput;
    txInput.feePayer = walletAddress;
    txInput.instructions = instructions;
    txInput.api = solanaApi;
    txInput.context = context;
    txInput.signer = signer;
    txInput.appendSignatures = AdditionalSigner::keypairSignatureAppender(
        nonceKeypair, QStringLiteral("Failed to sign nonce account keypair"));

    SimpleTransactionCallbacks txCb;
    txCb.onBroadcasting = [progress]() {
        progress(QCoreApplication::translate("SendReceivePage", "Sending transaction"));
    };
    txCb.onSent = [callbacks, nonceAddress](const QString& signature) {
        if (callbacks.onTransactionSent) {
            callbacks.onTransactionSent(signature, nonceAddress);
        }
    };
    txCb.onFailed = [fail](const QString& error) { fail(error); };
    executeSimpleTransactionAsync(txInput, txCb);
}

void SendReceiveHandler::pollNonceAccountFlow(const QString& nonceAddress,
                                              const QString& authorityAddress, SolanaApi* solanaApi,
                                              QObject* context,
                                              const PollNonceCallbacks& callbacks) const {
    if (!solanaApi || !context || nonceAddress.isEmpty() || authorityAddress.isEmpty()) {
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "Invalid nonce polling context."));
        }
        return;
    }

    constexpr int kMaxAttempts = 20;
    constexpr int kRetryIntervalMs = 3000;
    constexpr int kAttemptTimeoutMs = 10000;
    constexpr int kFirstDelayMs = 2000;

    auto attempt = std::make_shared<int>(0);
    auto poll = std::make_shared<std::function<void()>>();

    *poll = [poll, attempt, nonceAddress, authorityAddress, solanaApi, context, callbacks]() {
        ++(*attempt);
        if (callbacks.onAttempt) {
            callbacks.onAttempt(*attempt, kMaxAttempts);
        }

        auto* guard = new QObject(context);
        auto done = std::make_shared<bool>(false);

        QTimer::singleShot(
            kAttemptTimeoutMs, guard, [guard, done, poll, attempt, callbacks, context]() {
                if (*done) {
                    return;
                }
                *done = true;
                guard->deleteLater();

                if (*attempt < kMaxAttempts) {
                    QTimer::singleShot(kRetryIntervalMs, context, [poll]() { (*poll)(); });
                    return;
                }

                if (callbacks.onFailed) {
                    callbacks.onFailed(QCoreApplication::translate(
                        "SendReceivePage",
                        "Timed out waiting for nonce data. The account was created "
                        "on-chain — click Retry to check again."));
                }
            });

        QObject::connect(
            solanaApi, &SolanaApi::requestFailed, guard,
            [guard, done, poll, attempt, callbacks, context](const QString& method,
                                                             const QString& error) {
                if (*done || method != kMethodAccountInfo) {
                    return;
                }
                *done = true;
                guard->deleteLater();

                if (*attempt < kMaxAttempts) {
                    QTimer::singleShot(kRetryIntervalMs, context, [poll]() { (*poll)(); });
                    return;
                }

                if (callbacks.onFailed) {
                    callbacks.onFailed(QCoreApplication::translate(
                                           "SendReceivePage",
                                           "Failed to fetch nonce data: %1\n"
                                           "The account was created on-chain — click Retry to "
                                           "check again.")
                                           .arg(error));
                }
            });

        QObject::connect(
            solanaApi, &SolanaApi::nonceAccountReady, guard,
            [guard, done, poll, attempt, callbacks, nonceAddress, context,
             authorityAddress](const QString& addr, const NonceAccount& nonce) {
                if (*done || addr != nonceAddress) {
                    return;
                }
                *done = true;
                guard->deleteLater();

                if (!nonce.isInitialized()) {
                    if (*attempt < kMaxAttempts) {
                        QTimer::singleShot(kRetryIntervalMs, context, [poll]() { (*poll)(); });
                        return;
                    }

                    if (callbacks.onFailed) {
                        callbacks.onFailed(
                            QCoreApplication::translate("SendReceivePage",
                                                        "Nonce account not yet initialized after "
                                                        "%1 attempts. Click Retry to "
                                                        "check again.")
                                .arg(kMaxAttempts));
                    }
                    return;
                }

                NonceDb::insertNonceAccount(addr, authorityAddress, nonce.nonce);
                if (callbacks.onSuccess) {
                    callbacks.onSuccess(addr, nonce.nonce);
                }
            });

        solanaApi->fetchNonceAccount(nonceAddress, "confirmed");
    };

    QTimer::singleShot(kFirstDelayMs, context, [poll]() { (*poll)(); });
}

SendReceiveStoredNonce SendReceiveHandler::lookupStoredNonce(const QString& walletAddress) const {
    SendReceiveStoredNonce result;
    if (walletAddress.isEmpty()) {
        return result;
    }

    const auto nonce = NonceDb::getByAuthorityRecord(walletAddress);
    if (!nonce.has_value()) {
        return result;
    }

    result.found = true;
    result.address = nonce->address;
    result.nonceValue = nonce->nonceValue;
    return result;
}

void SendReceiveHandler::refreshStoredNonceValue(
    const QString& nonceAddress, SolanaApi* solanaApi, QObject* context,
    const RefreshStoredNonceCallbacks& callbacks) const {
    if (nonceAddress.isEmpty() || !solanaApi || !context) {
        return;
    }

    auto* guard = new QObject(context);
    auto done = std::make_shared<bool>(false);

    QTimer::singleShot(kRpcShortLookupTimeoutMs, guard, [guard, done]() {
        if (*done) {
            return;
        }
        *done = true;
        guard->deleteLater();
    });

    QObject::connect(
        solanaApi, &SolanaApi::nonceAccountReady, guard,
        [guard, done, nonceAddress, callbacks](const QString& addr, const NonceAccount& nonce) {
            if (*done || addr != nonceAddress) {
                return;
            }
            *done = true;
            if (nonce.isInitialized()) {
                NonceDb::updateNonceValue(addr, nonce.nonce);
                if (callbacks.onUpdated) {
                    callbacks.onUpdated(nonce.nonce);
                }
            }
            guard->deleteLater();
        });

    QObject::connect(solanaApi, &SolanaApi::requestFailed, guard,
                     [guard, done](const QString& method, const QString&) {
                         if (*done || method != kMethodAccountInfo) {
                             return;
                         }
                         *done = true;
                         guard->deleteLater();
                     });

    solanaApi->fetchNonceAccount(nonceAddress);
}

void SendReceiveHandler::fetchNonceRentCost(SolanaApi* solanaApi, QObject* context,
                                            const FetchNonceRentCallbacks& callbacks) const {
    if (!solanaApi || !context) {
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "RPC unavailable for rent lookup."));
        }
        return;
    }

    MinimumBalanceRequest request;
    request.api = solanaApi;
    request.context = context;
    request.dataSize = kNonceAccountSpace;
    request.timeoutMs = kRpcLookupTimeoutMs;

    MinimumBalanceCallbacks lookupCallbacks;
    lookupCallbacks.onReady = [callbacks](quint64 lamports) {
        if (callbacks.onReady) {
            callbacks.onReady(lamports);
        }
    };
    lookupCallbacks.onFailed = [callbacks](const QString& error) {
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "Failed to fetch cost: %1")
                    .arg(error));
        }
    };
    lookupCallbacks.onTimedOut = [callbacks]() {
        if (callbacks.onFailed) {
            callbacks.onFailed(QCoreApplication::translate(
                "SendReceivePage", "Request timed out. Go back and try again."));
        }
    };
    fetchMinimumBalanceWithTimeout(request, lookupCallbacks);
}

void SendReceiveHandler::resolveNonceToggleEnabled(
    const QString& walletAddress, const QString& pendingNonceAddress, SolanaApi* solanaApi,
    QObject* context, const ResolveNonceToggleCallbacks& callbacks) const {
    const SendReceiveStoredNonce stored = lookupStoredNonce(walletAddress);
    if (stored.found) {
        if (callbacks.onStoredNonce) {
            callbacks.onStoredNonce(stored.address, stored.nonceValue);
        }

        RefreshStoredNonceCallbacks refreshCallbacks;
        refreshCallbacks.onUpdated = [callbacks](const QString& nonceValue) {
            if (callbacks.onStoredNonceUpdated) {
                callbacks.onStoredNonceUpdated(nonceValue);
            }
        };
        refreshStoredNonceValue(stored.address, solanaApi, context, refreshCallbacks);
        return;
    }

    if (!pendingNonceAddress.isEmpty()) {
        if (callbacks.onPendingNonceRetry) {
            callbacks.onPendingNonceRetry();
        }
        return;
    }

    if (callbacks.onCreateRequired) {
        callbacks.onCreateRequired();
    }

    FetchNonceRentCallbacks rentCallbacks;
    rentCallbacks.onReady = [callbacks](quint64 lamports) {
        if (callbacks.onRentReady) {
            callbacks.onRentReady(lamports);
        }
    };
    rentCallbacks.onFailed = [callbacks](const QString& errorText) {
        if (callbacks.onRentFailed) {
            callbacks.onRentFailed(errorText);
        }
    };
    fetchNonceRentCost(solanaApi, context, rentCallbacks);
}

void SendReceiveHandler::executeCreateAndPollNonceFlow(
    const QString& walletAddress, quint64 rentLamports, SolanaApi* solanaApi, Signer* signer,
    QObject* context, const CreateAndPollNonceCallbacks& callbacks) const {
    CreateNonceCallbacks createCallbacks;
    createCallbacks.onProgress = [callbacks](const QString& phaseText) {
        if (callbacks.onProgress) {
            callbacks.onProgress(phaseText);
        }
    };
    createCallbacks.onError = [callbacks](const QString& errorText) {
        if (callbacks.onFailed) {
            callbacks.onFailed(errorText);
        }
    };
    createCallbacks.onTransactionSent = [this, callbacks, walletAddress, solanaApi,
                                         context](const QString& txSignature,
                                                  const QString& nonceAddress) {
        if (callbacks.onTransactionSent) {
            callbacks.onTransactionSent(txSignature, nonceAddress);
        }

        PollNonceCallbacks pollCallbacks;
        pollCallbacks.onAttempt = [callbacks](int attempt, int maxAttempts) {
            if (callbacks.onPollAttempt) {
                callbacks.onPollAttempt(attempt, maxAttempts);
            }
        };
        pollCallbacks.onFailed = [callbacks](const QString& errorText) {
            if (callbacks.onFailed) {
                callbacks.onFailed(errorText);
            }
        };
        pollCallbacks.onSuccess = [callbacks](const QString& address, const QString& nonceValue) {
            if (callbacks.onSuccess) {
                callbacks.onSuccess(address, nonceValue);
            }
        };
        pollNonceAccountFlow(nonceAddress, walletAddress, solanaApi, context, pollCallbacks);
    };

    executeCreateNonceFlow(walletAddress, rentLamports, solanaApi, signer, context,
                           createCallbacks);
}

void SendReceiveHandler::executeSendFlow(const SendReceiveExecutionRequest& request,
                                         SolanaApi* solanaApi, Signer* signer, QObject* context,
                                         const ExecuteSendCallbacks& callbacks) const {
    const auto status = [callbacks](const QString& text, bool isError) {
        if (callbacks.onStatus) {
            callbacks.onStatus(text, isError);
        }
    };
    const auto finish = [callbacks]() {
        if (callbacks.onFinished) {
            callbacks.onFinished();
        }
    };

    if (!solanaApi || !signer || !context) {
        status(QCoreApplication::translate("SendReceivePage",
                                           "Error: wallet not available for signing."),
               true);
        finish();
        return;
    }

    qDebug() << "[SendFlow] START";
    QElapsedTimer sendTimer;
    sendTimer.start();

    status(QCoreApplication::translate("SendReceivePage", "Fetching blockhash..."), false);

    // Prepend priority fee instructions if requested
    QList<TransactionInstruction> finalInstructions = request.instructions;
    if (request.priorityFeeMicroLamports > 0) {
        constexpr quint32 kComputeUnitLimit = 200000;
        finalInstructions.prepend(
            ComputeBudgetInstruction::setComputeUnitPrice(request.priorityFeeMicroLamports));
        finalInstructions.prepend(ComputeBudgetInstruction::setComputeUnitLimit(kComputeUnitLimit));
    }

    SimpleTransactionInput txInput;
    txInput.feePayer = request.walletAddress;
    txInput.instructions = finalInstructions;
    txInput.api = solanaApi;
    txInput.context = context;
    txInput.signer = signer;
    txInput.useNonce =
        request.nonceEnabled && !request.nonceAddress.isEmpty() && !request.nonceValue.isEmpty();
    txInput.nonceAddress = request.nonceAddress;
    txInput.nonceAuthority = request.walletAddress;
    txInput.nonceValue = request.nonceValue;

    SimpleTransactionCallbacks txCb;
    auto sendTimerPtr = std::make_shared<QElapsedTimer>(sendTimer);
    txCb.onBroadcasting = [status, sendTimerPtr]() {
        qDebug() << "[SendFlow] Broadcasting at" << sendTimerPtr->elapsed() << "ms";
        status(QCoreApplication::translate("SendReceivePage", "Broadcasting transaction..."),
               false);
    };
    txCb.onSent = [callbacks, finish, sendTimerPtr](const QString& txSignature) {
        qDebug() << "[SendFlow] SENT at" << sendTimerPtr->elapsed()
                 << "ms sig:" << txSignature.left(16) << "...";
        if (callbacks.onSuccess) {
            callbacks.onSuccess(txSignature);
        }
        finish();
    };
    txCb.onFailed = [status, finish, sendTimerPtr](const QString& error) {
        qDebug() << "[SendFlow] FAILED at" << sendTimerPtr->elapsed() << "ms error:" << error;
        status(error, true);
        finish();
    };
    executeSimpleTransactionAsync(txInput, txCb);
}

void SendReceiveHandler::pollTransactionConfirmationFlow(
    const QString& signature, SolanaApi* solanaApi, QObject* context,
    const PollTransactionCallbacks& callbacks) const {
    if (!solanaApi || !context || signature.isEmpty()) {
        if (callbacks.onTimeout) {
            callbacks.onTimeout(
                QCoreApplication::translate("SendReceivePage", "Invalid polling context."));
        }
        return;
    }

    // Phase 1: fast-poll getSignatureStatuses → fire onStatusConfirmed / onStatusFinalized
    // Phase 2: after finalized, single getTransaction for detail backfill → onDetailsReady
    constexpr int kMaxAttempts = 30;
    constexpr int kPollIntervalMs = 2000;
    constexpr int kAttemptTimeoutMs = 8000;
    constexpr int kFirstDelayMs = 1500;
    constexpr int kDetailRetries = 3;
    constexpr int kDetailRetryMs = 2000;

    auto attempt = std::make_shared<int>(0);
    auto confirmedFired = std::make_shared<bool>(false);
    auto poll = std::make_shared<std::function<void()>>();
    auto t0 = std::make_shared<QElapsedTimer>();
    t0->start();

    qDebug() << "[ConfirmPoll] START sig:" << signature.left(16) << "...";

    // Phase 2: fetch full transaction details (non-blocking detail backfill)
    auto fetchDetails = [solanaApi, context, callbacks, signature, t0]() {
        qDebug() << "[ConfirmPoll] Phase2 START (getTransaction) at" << t0->elapsed() << "ms";
        auto detailAttempt = std::make_shared<int>(0);
        auto detailFetch = std::make_shared<std::function<void()>>();

        *detailFetch = [detailFetch, detailAttempt, solanaApi, context, callbacks, signature,
                        t0]() {
            ++(*detailAttempt);
            qDebug() << "[ConfirmPoll] Phase2 attempt" << *detailAttempt << "at" << t0->elapsed()
                     << "ms";
            auto* detailGuard = new QObject(context);
            auto detailDone = std::make_shared<bool>(false);

            QTimer::singleShot(10000, detailGuard, [detailGuard, detailDone, t0]() {
                if (*detailDone) {
                    return;
                }
                *detailDone = true;
                detailGuard->deleteLater();
                qDebug() << "[ConfirmPoll] Phase2 TIMEOUT at" << t0->elapsed() << "ms";
            });

            QObject::connect(solanaApi, &SolanaApi::transactionReady, detailGuard,
                             [detailGuard, detailDone, detailFetch, detailAttempt, callbacks,
                              signature, context,
                              t0](const QString& sig, const TransactionResponse& tx) {
                                 if (*detailDone || sig != signature) {
                                     return;
                                 }

                                 qDebug() << "[ConfirmPoll] Phase2 transactionReady at"
                                          << t0->elapsed() << "ms slot:" << tx.slot;

                                 if (tx.slot == 0 && *detailAttempt < kDetailRetries) {
                                     *detailDone = true;
                                     detailGuard->deleteLater();
                                     qDebug() << "[ConfirmPoll] Phase2 null result, retrying in"
                                              << kDetailRetryMs << "ms";
                                     QTimer::singleShot(kDetailRetryMs, context,
                                                        [detailFetch]() { (*detailFetch)(); });
                                     return;
                                 }

                                 *detailDone = true;
                                 detailGuard->deleteLater();
                                 if (tx.slot > 0) {
                                     if (tx.meta.hasError) {
                                         qDebug() << "[ConfirmPoll] Phase2 FAILED on-chain at"
                                                  << t0->elapsed() << "ms";
                                         if (callbacks.onFailed) {
                                             callbacks.onFailed(tx);
                                         }
                                     } else {
                                         qDebug() << "[ConfirmPoll] Phase2 DONE at" << t0->elapsed()
                                                  << "ms fee:" << tx.meta.fee;
                                         if (callbacks.onDetailsReady) {
                                             callbacks.onDetailsReady(tx);
                                         }
                                     }
                                 } else {
                                     qDebug() << "[ConfirmPoll] Phase2 DONE (no details) at"
                                              << t0->elapsed() << "ms";
                                 }
                             });

            QObject::connect(
                solanaApi, &SolanaApi::requestFailed, detailGuard,
                [detailGuard, detailDone, t0](const QString& method, const QString& error) {
                    if (*detailDone || method != TxExecutionConstants::MethodGetTransaction) {
                        return;
                    }
                    *detailDone = true;
                    detailGuard->deleteLater();
                    qDebug() << "[ConfirmPoll] Phase2 requestFailed at" << t0->elapsed()
                             << "ms error:" << error;
                });

            solanaApi->fetchTransaction(signature);
        };

        (*detailFetch)();
    };

    // Phase 1: poll getSignatureStatuses for confirmed → finalized progression
    *poll = [poll, attempt, confirmedFired, signature, solanaApi, context, callbacks, fetchDetails,
             t0]() {
        ++(*attempt);
        qDebug() << "[ConfirmPoll] Phase1 attempt" << *attempt << "at" << t0->elapsed() << "ms";
        if (callbacks.onAttempt) {
            callbacks.onAttempt(*attempt, kMaxAttempts);
        }

        auto* guard = new QObject(context);
        auto done = std::make_shared<bool>(false);

        auto retry = [done, guard, poll, attempt, context, callbacks, t0]() {
            if (*done) {
                return;
            }
            *done = true;
            guard->deleteLater();
            if (*attempt < kMaxAttempts) {
                QTimer::singleShot(kPollIntervalMs, context, [poll]() { (*poll)(); });
            } else if (callbacks.onTimeout) {
                qDebug() << "[ConfirmPoll] Phase1 TIMED OUT at" << t0->elapsed() << "ms";
                callbacks.onTimeout(QCoreApplication::translate(
                    "SendReceivePage", "Transaction confirmation timed out."));
            }
        };

        QTimer::singleShot(kAttemptTimeoutMs, guard, retry);

        QObject::connect(solanaApi, &SolanaApi::requestFailed, guard,
                         [retry, t0](const QString& method, const QString& error) {
                             if (method == TxExecutionConstants::MethodGetSignatureStatuses) {
                                 qDebug() << "[ConfirmPoll] Phase1 requestFailed at"
                                          << t0->elapsed() << "ms error:" << error;
                                 retry();
                             }
                         });

        QObject::connect(
            solanaApi, &SolanaApi::signatureStatusesReady, guard,
            [done, guard, confirmedFired, context, callbacks, fetchDetails, retry,
             t0](const QJsonArray& statuses) {
                if (*done) {
                    return;
                }

                if (statuses.isEmpty() || statuses[0].isNull()) {
                    qDebug() << "[ConfirmPoll] Phase1 status=null at" << t0->elapsed() << "ms";
                    retry();
                    return;
                }

                const QJsonObject status = statuses[0].toObject();
                const QString confirmationStatus = status["confirmationStatus"].toString();
                const bool hasError = !status["err"].isNull() && !status["err"].isUndefined();

                qDebug() << "[ConfirmPoll] Phase1 status=" << confirmationStatus
                         << "err=" << status["err"] << "at" << t0->elapsed() << "ms";

                if (hasError) {
                    *done = true;
                    guard->deleteLater();
                    TransactionResponse errorTx;
                    errorTx.meta.hasError = true;
                    if (callbacks.onFailed) {
                        callbacks.onFailed(errorTx);
                    }
                    return;
                }

                if (confirmationStatus == TxExecutionConstants::ConfirmationFinalized) {
                    *done = true;
                    guard->deleteLater();
                    qDebug() << "[ConfirmPoll] Phase1 FINALIZED at" << t0->elapsed() << "ms";
                    if (!*confirmedFired && callbacks.onStatusConfirmed) {
                        callbacks.onStatusConfirmed();
                    }
                    if (callbacks.onStatusFinalized) {
                        callbacks.onStatusFinalized();
                    }
                    fetchDetails();
                } else if (confirmationStatus == TxExecutionConstants::ConfirmationConfirmed) {
                    if (!*confirmedFired) {
                        *confirmedFired = true;
                        qDebug() << "[ConfirmPoll] Phase1 CONFIRMED at" << t0->elapsed() << "ms";
                        if (callbacks.onStatusConfirmed) {
                            callbacks.onStatusConfirmed();
                        }
                    }
                    retry();
                } else {
                    retry();
                }
            });

        solanaApi->fetchSignatureStatuses({signature});
    };

    qDebug() << "[ConfirmPoll] waiting" << kFirstDelayMs << "ms before first poll";
    QTimer::singleShot(kFirstDelayMs, context, [poll]() { (*poll)(); });
}
