#include "tx/TransactionExecutor.h"

#include "services/SolanaApi.h"
#include "tx/TxExecutionConstants.h"

#include <QTimer>
#include <memory>

namespace {
    QString fallbackMethod(const QString& value, const char* fallback) {
        return value.isEmpty() ? QString::fromLatin1(fallback) : value;
    }
} // namespace

void signTransactionAsync(TransactionSigningRequest request,
                          const TransactionSigningSuccess& onSuccess,
                          const TransactionSigningError& onError) {
    if (!request.signer) {
        if (onError) {
            onError(
                {TransactionSigningFailureKind::MissingSigner, QStringLiteral("Missing signer")});
        }
        return;
    }

    const QByteArray message = request.builder.serializeMessage();
    if (message.isEmpty()) {
        if (onError) {
            onError({TransactionSigningFailureKind::BuildMessage, request.builder.lastError()});
        }
        return;
    }

    request.signer->signAsync(
        message, request.context,
        [request, onSuccess, onError, message](const QByteArray& signature,
                                               const QString& error) mutable {
            if (signature.isEmpty()) {
                if (onError) {
                    onError({TransactionSigningFailureKind::PrimarySignature, error});
                }
                return;
            }

            QList<QByteArray> signatures{signature};
            if (request.appendSignatures) {
                QString appendError;
                if (!request.appendSignatures(message, &signatures, &appendError)) {
                    if (onError) {
                        onError({TransactionSigningFailureKind::ExtraSignatures, appendError});
                    }
                    return;
                }
            }

            const QByteArray signedTx = request.builder.buildSigned(signatures);
            if (signedTx.isEmpty()) {
                if (onError) {
                    onError(
                        {TransactionSigningFailureKind::BuildSigned, request.builder.lastError()});
                }
                return;
            }

            qDebug() << "[TxExecutor] Signed tx" << signedTx.size() << "bytes," << signatures.size()
                     << "signers, msg" << message.size() << "bytes";
            qDebug() << "[TxExecutor] Message base64:" << message.toBase64();

            if (onSuccess) {
                onSuccess(signedTx, message);
            }
        });
}

void submitSignedTransactionAsync(const TransactionSubmitRequest& request,
                                  const TransactionSubmitCallbacks& callbacks) {
    if (!request.api || !request.context || request.signedTransaction.isEmpty()) {
        if (callbacks.onFailed) {
            callbacks.onFailed(QStringLiteral("Invalid transaction submission request."));
        }
        return;
    }

    auto* guard = new QObject(request.context);
    auto done = std::make_shared<bool>(false);
    const QString failMethod =
        request.expectedFailMethod.isEmpty()
            ? QString::fromLatin1(TxExecutionConstants::MethodSendTransaction)
            : request.expectedFailMethod;

    QTimer::singleShot(request.timeoutMs, guard, [guard, done, callbacks]() {
        if (*done) {
            return;
        }
        *done = true;
        if (callbacks.onTimedOut) {
            callbacks.onTimedOut();
        }
        guard->deleteLater();
    });

    QObject::connect(request.api, &SolanaApi::transactionSent, guard,
                     [guard, done, callbacks](const QString& signature) {
                         if (*done) {
                             return;
                         }
                         *done = true;
                         if (callbacks.onSent) {
                             callbacks.onSent(signature);
                         }
                         guard->deleteLater();
                     });

    QObject::connect(
        request.api, &SolanaApi::requestFailed, guard,
        [guard, done, callbacks, failMethod](const QString& method, const QString& error) {
            if (*done || method != failMethod) {
                return;
            }
            *done = true;
            if (callbacks.onFailed) {
                callbacks.onFailed(error);
            }
            guard->deleteLater();
        });

    request.api->sendTransaction(request.signedTransaction);
}

void signAndSubmitTransactionAsync(const TransactionExecutionRequest& request,
                                   const TransactionExecutionCallbacks& callbacks) {
    if (!request.api || !request.context) {
        if (callbacks.onSubmitFailed) {
            callbacks.onSubmitFailed(QStringLiteral("Invalid transaction execution context."));
        }
        return;
    }

    TransactionSigningRequest signing = request.signing;
    if (!signing.context) {
        signing.context = request.context;
    }

    signTransactionAsync(
        signing,
        [request, callbacks](const QByteArray& signedTx, const QByteArray& message) {
            if (callbacks.onSigned) {
                callbacks.onSigned(signedTx, message);
            }

            TransactionSubmitRequest submitRequest;
            submitRequest.api = request.api;
            submitRequest.context = request.context;
            submitRequest.signedTransaction = signedTx;
            submitRequest.timeoutMs = request.submitTimeoutMs;
            submitRequest.expectedFailMethod = request.submitFailMethod;

            TransactionSubmitCallbacks submitCallbacks;
            submitCallbacks.onSent = callbacks.onSent;
            submitCallbacks.onFailed = callbacks.onSubmitFailed;
            submitCallbacks.onTimedOut = callbacks.onSubmitTimedOut;
            submitSignedTransactionAsync(submitRequest, submitCallbacks);
        },
        [callbacks](const TransactionSigningFailure& failure) {
            if (callbacks.onSigningFailed) {
                callbacks.onSigningFailed(failure);
            }
        });
}

void fetchLatestBlockhashWithTimeout(const LatestBlockhashRequest& request,
                                     const LatestBlockhashCallbacks& callbacks) {
    if (!request.api || !request.context) {
        if (callbacks.onFailed) {
            callbacks.onFailed(QStringLiteral("Invalid blockhash request context."));
        }
        return;
    }

    auto* guard = new QObject(request.context);
    auto done = std::make_shared<bool>(false);
    const QString failMethod =
        fallbackMethod(request.failMethod, TxExecutionConstants::MethodLatestBlockhash);

    QTimer::singleShot(request.timeoutMs, guard, [guard, done, callbacks]() {
        if (*done) {
            return;
        }
        *done = true;
        if (callbacks.onTimedOut) {
            callbacks.onTimedOut();
        }
        guard->deleteLater();
    });

    QObject::connect(request.api, &SolanaApi::latestBlockhashReady, guard,
                     [guard, done, callbacks](const QString& blockhash, quint64 lastValid) {
                         if (*done) {
                             return;
                         }
                         *done = true;
                         if (callbacks.onReady) {
                             callbacks.onReady(blockhash, lastValid);
                         }
                         guard->deleteLater();
                     });

    QObject::connect(
        request.api, &SolanaApi::requestFailed, guard,
        [guard, done, callbacks, failMethod](const QString& method, const QString& error) {
            if (*done || method != failMethod) {
                return;
            }
            *done = true;
            if (callbacks.onFailed) {
                callbacks.onFailed(error);
            }
            guard->deleteLater();
        });

    request.api->fetchLatestBlockhash(request.commitment);
}

void fetchMinimumBalanceWithTimeout(const MinimumBalanceRequest& request,
                                    const MinimumBalanceCallbacks& callbacks) {
    if (!request.api || !request.context) {
        if (callbacks.onFailed) {
            callbacks.onFailed(QStringLiteral("Invalid minimum-balance request context."));
        }
        return;
    }

    auto* guard = new QObject(request.context);
    auto done = std::make_shared<bool>(false);
    const QString failMethod =
        fallbackMethod(request.failMethod, TxExecutionConstants::MethodMinimumBalance);

    QTimer::singleShot(request.timeoutMs, guard, [guard, done, callbacks]() {
        if (*done) {
            return;
        }
        *done = true;
        if (callbacks.onTimedOut) {
            callbacks.onTimedOut();
        }
        guard->deleteLater();
    });

    QObject::connect(request.api, &SolanaApi::minimumBalanceReady, guard,
                     [guard, done, callbacks](quint64 lamports) {
                         if (*done) {
                             return;
                         }
                         *done = true;
                         if (callbacks.onReady) {
                             callbacks.onReady(lamports);
                         }
                         guard->deleteLater();
                     });

    QObject::connect(
        request.api, &SolanaApi::requestFailed, guard,
        [guard, done, callbacks, failMethod](const QString& method, const QString& error) {
            if (*done || method != failMethod) {
                return;
            }
            *done = true;
            if (callbacks.onFailed) {
                callbacks.onFailed(error);
            }
            guard->deleteLater();
        });

    request.api->fetchMinimumBalanceForRentExemption(request.dataSize);
}

// ── Transaction flow (build helpers) ─────────────────────────

bool buildExecutionRequest(const TransactionExecutionBuildInput& input,
                           TransactionExecutionRequest* request, QString* error) {
    if (!request || !error) {
        return false;
    }

    *request = {};
    error->clear();

    if (!input.api || !input.context || !input.signer || input.feePayer.isEmpty()) {
        *error = QStringLiteral("invalid_execution_context");
        return false;
    }
    if (input.instructions.isEmpty()) {
        *error = QStringLiteral("no_instructions");
        return false;
    }

    TransactionBuilder builder;
    builder.setFeePayer(input.feePayer);
    if (input.useNonce) {
        if (input.nonceAddress.isEmpty() || input.nonceAuthority.isEmpty() ||
            input.nonceValue.isEmpty()) {
            *error = QStringLiteral("invalid_nonce_context");
            return false;
        }
        builder.useNonce(input.nonceAddress, input.nonceAuthority, input.nonceValue);
    } else {
        if (input.blockhash.isEmpty()) {
            *error = QStringLiteral("missing_blockhash");
            return false;
        }
        builder.setRecentBlockhash(input.blockhash);
    }

    for (const auto& ix : input.instructions) {
        builder.addInstruction(ix);
    }

    request->api = input.api;
    request->context = input.context;
    request->submitTimeoutMs = input.submitTimeoutMs > 0
                                   ? input.submitTimeoutMs
                                   : TxExecutionConstants::TxConfirmationTimeoutMs;
    request->submitFailMethod =
        input.submitFailMethod.isEmpty()
            ? QString::fromLatin1(TxExecutionConstants::MethodSendTransaction)
            : input.submitFailMethod;
    request->signing.builder = builder;
    request->signing.signer = input.signer;
    request->signing.context = input.context;
    request->signing.appendSignatures = input.appendSignatures;
    return true;
}

TransactionSigningErrorCategory classifySigningFailure(const TransactionSigningFailure& failure) {
    switch (failure.kind) {
        case TransactionSigningFailureKind::BuildMessage:
        case TransactionSigningFailureKind::BuildSigned:
        case TransactionSigningFailureKind::ExtraSignatures:
            return TransactionSigningErrorCategory::Build;
        case TransactionSigningFailureKind::MissingSigner:
            return TransactionSigningErrorCategory::MissingSigner;
        case TransactionSigningFailureKind::PrimarySignature:
        default:
            return TransactionSigningErrorCategory::Signing;
    }
}

QString signingFailureMessage(const TransactionSigningFailure& failure) {
    switch (classifySigningFailure(failure)) {
        case TransactionSigningErrorCategory::Build:
            return failure.detail.isEmpty()
                       ? QStringLiteral("Error building transaction.")
                       : QStringLiteral("Error building transaction: %1").arg(failure.detail);
        case TransactionSigningErrorCategory::MissingSigner:
            return QStringLiteral("Wallet not available for signing.");
        case TransactionSigningErrorCategory::Signing:
        default:
            return failure.detail.isEmpty() ? QStringLiteral("Signing failed.") : failure.detail;
    }
}

// ── Simple transaction execution ─────────────────────────────

void executeSimpleTransactionAsync(const SimpleTransactionInput& input,
                                   const SimpleTransactionCallbacks& callbacks) {
    const auto fail = [callbacks](const QString& msg) {
        if (callbacks.onFailed) {
            callbacks.onFailed(msg);
        }
    };

    if (!input.api || !input.context || !input.signer) {
        fail(QStringLiteral("Wallet not available for signing."));
        return;
    }

    auto buildAndSubmit = [input, callbacks, fail](const QString& blockhash) {
        TransactionExecutionBuildInput buildInput;
        buildInput.feePayer = input.feePayer;
        buildInput.blockhash = blockhash;
        buildInput.instructions = input.instructions;
        buildInput.useNonce = input.useNonce;
        buildInput.nonceAddress = input.nonceAddress;
        buildInput.nonceAuthority = input.nonceAuthority;
        buildInput.nonceValue = input.nonceValue;
        buildInput.api = input.api;
        buildInput.context = input.context;
        buildInput.signer = input.signer;
        buildInput.appendSignatures = input.appendSignatures;
        buildInput.submitTimeoutMs = TxExecutionConstants::TxConfirmationTimeoutMs;

        TransactionExecutionRequest execRequest;
        QString buildError;
        if (!buildExecutionRequest(buildInput, &execRequest, &buildError)) {
            fail(QStringLiteral("Error building transaction."));
            return;
        }

        TransactionExecutionCallbacks execCallbacks;
        execCallbacks.onSigned = [callbacks](const QByteArray&, const QByteArray&) {
            if (callbacks.onBroadcasting) {
                callbacks.onBroadcasting();
            }
        };
        execCallbacks.onSent = [callbacks](const QString& signature) {
            if (callbacks.onSent) {
                callbacks.onSent(signature);
            }
        };
        execCallbacks.onSubmitFailed = [fail](const QString& error) { fail(error); };
        execCallbacks.onSubmitTimedOut = [fail]() {
            fail(QStringLiteral("Timed out waiting for confirmation."));
        };
        execCallbacks.onSigningFailed = [fail](const TransactionSigningFailure& failure) {
            fail(signingFailureMessage(failure));
        };
        signAndSubmitTransactionAsync(execRequest, execCallbacks);
    };

    if (input.useNonce && !input.nonceValue.isEmpty()) {
        buildAndSubmit(input.nonceValue);
        return;
    }

    LatestBlockhashRequest blockhashRequest;
    blockhashRequest.api = input.api;
    blockhashRequest.context = input.context;
    blockhashRequest.timeoutMs = TxExecutionConstants::RpcLookupTimeoutMs;
    blockhashRequest.commitment = QString::fromLatin1(TxExecutionConstants::FinalizedCommitment);

    LatestBlockhashCallbacks blockhashCallbacks;
    blockhashCallbacks.onReady = [buildAndSubmit](const QString& blockhash, quint64) {
        buildAndSubmit(blockhash);
    };
    blockhashCallbacks.onFailed = [fail](const QString& error) {
        fail(QStringLiteral("Failed to fetch blockhash: %1").arg(error));
    };
    blockhashCallbacks.onTimedOut = [fail]() {
        fail(QStringLiteral("Timed out fetching blockhash."));
    };
    fetchLatestBlockhashWithTimeout(blockhashRequest, blockhashCallbacks);
}

// ── Additional signer helper ─────────────────────────────────

namespace AdditionalSigner {
    TransactionAppendSignaturesCallback keypairSignatureAppender(const Keypair& keypair,
                                                                 const QString& errorText) {
        return [keypair, errorText](const QByteArray& message, QList<QByteArray>* signatures,
                                    QString* error) {
            if (!signatures) {
                if (error) {
                    *error = QStringLiteral("Missing signature list");
                }
                return false;
            }

            const QByteArray sig = keypair.sign(message);
            if (sig.isEmpty()) {
                if (error) {
                    *error = errorText;
                }
                return false;
            }

            signatures->append(sig);
            return true;
        };
    }
} // namespace AdditionalSigner
