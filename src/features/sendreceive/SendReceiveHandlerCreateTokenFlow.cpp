#include "features/sendreceive/SendReceiveHandler.h"

#include "crypto/Keypair.h"
#include "crypto/Signer.h"
#include "services/SolanaApi.h"
#include "tx/TokenOperationBuilder.h"
#include "tx/TransactionExecutor.h"
#include "tx/TxExecutionConstants.h"

#include <QCoreApplication>
#include <QTimer>
#include <QtEndian>
#include <cmath>
#include <memory>

namespace {
    constexpr int kRpcLookupTimeoutMs = TxExecutionConstants::RpcLookupTimeoutMs;
    constexpr int kRpcShortLookupTimeoutMs = TxExecutionConstants::RpcShortLookupTimeoutMs;
    constexpr double kBasisPointsDivisor = 10000.0;

    constexpr int kMintDataMinSizeForExtensions = 170;
    constexpr int kMintExtStartOffset = 166;
    constexpr int kMintExtHeaderLen = 4;
    constexpr quint16 kTransferFeeConfigExtType = 1;
    constexpr quint16 kTransferFeeConfigMinLen = 108;
    constexpr int kTransferFeeMaxRawOffset = 98;
    constexpr int kTransferFeeBasisPointsOffset = 106;
    constexpr const char* kMethodAccountInfo = TxExecutionConstants::MethodAccountInfo;
} // namespace

bool SendReceiveHandler::buildCreateTokenInstructions(const SendReceiveCreateTokenRequest& request,
                                                      quint64 rentLamports,
                                                      QList<TransactionInstruction>* instructions,
                                                      QString* error) const {
    if (!instructions || !error) {
        return false;
    }

    instructions->clear();
    error->clear();

    CreateTokenInstructionBuildInput input;
    input.walletAddress = request.walletAddress;
    input.mintAddress = request.mintAddress;
    input.name = request.name;
    input.symbol = request.symbol;
    input.uri = request.uri;
    input.freezeAuthority = request.freezeAuthority;
    input.decimals = request.decimals;
    input.rawSupply = request.rawSupply;
    input.hasTransferFee = request.hasTransferFee;
    input.feeBasisPoints = request.feeBasisPoints;
    input.feeMaxRaw = request.feeMaxRaw;
    input.hasNonTransferable = request.hasNonTransferable;
    input.hasMintClose = request.hasMintClose;
    input.hasPermanentDelegate = request.hasPermanentDelegate;
    input.mintAccountSize = request.mintAccountSize;
    input.rentLamports = rentLamports;

    const CreateTokenInstructionBuildResult buildResult =
        TokenOperationBuilder::buildCreateToken(input);
    if (!buildResult.ok) {
        if (buildResult.error == "derive_wallet_ata_failed") {
            *error = QCoreApplication::translate("SendReceivePage",
                                                 "Failed to derive token account address.");
        } else {
            *error = buildResult.error;
        }
        instructions->clear();
        return false;
    }

    *instructions = buildResult.instructions;

    // Debug: log each instruction with full account details
    qDebug() << "[CreateToken] Built" << instructions->size() << "instructions"
             << "mintAccountSize:" << request.mintAccountSize << "rent:" << rentLamports;
    for (int i = 0; i < instructions->size(); ++i) {
        const auto& ix = instructions->at(i);
        qDebug() << "  Ix" << i << "program:" << ix.programId << "data_size:" << ix.data.size()
                 << "data_hex:" << ix.data.toHex() << "accounts:" << ix.accounts.size();
        for (int j = 0; j < ix.accounts.size(); ++j) {
            const auto& acc = ix.accounts[j];
            qDebug() << "    account" << j << acc.pubkey.left(16) << "..."
                     << (acc.isWritable ? "W" : "R") << (acc.isSigner ? "S" : "-");
        }
    }

    return true;
}

void SendReceiveHandler::executeCreateTokenFlow(const SendReceiveCreateTokenRequest& request,
                                                const Keypair& mintKeypair, SolanaApi* solanaApi,
                                                Signer* signer, QObject* context,
                                                const CreateTokenCallbacks& callbacks) const {
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

    status(QCoreApplication::translate("SendReceivePage", "Fetching rent cost..."), false);

    MinimumBalanceRequest rentRequest;
    rentRequest.api = solanaApi;
    rentRequest.context = context;
    rentRequest.dataSize = request.mintAccountSize;
    rentRequest.timeoutMs = kRpcLookupTimeoutMs;

    MinimumBalanceCallbacks rentCallbacks;
    rentCallbacks.onReady = [this, request, mintKeypair, solanaApi, signer, context, callbacks,
                             status, finish](quint64 rentLamports) {
        QList<TransactionInstruction> instructions;
        QString buildError;
        if (!buildCreateTokenInstructions(request, rentLamports, &instructions, &buildError)) {
            status(buildError.isEmpty() ? QCoreApplication::translate("SendReceivePage",
                                                                      "Error building transaction.")
                                        : buildError,
                   true);
            finish();
            return;
        }

        status(QCoreApplication::translate("SendReceivePage", "Fetching blockhash..."), false);

        SimpleTransactionInput txInput;
        txInput.feePayer = request.walletAddress;
        txInput.instructions = instructions;
        txInput.api = solanaApi;
        txInput.context = context;
        txInput.signer = signer;
        txInput.appendSignatures = AdditionalSigner::keypairSignatureAppender(
            mintKeypair, QStringLiteral("Failed to sign mint keypair"));

        SimpleTransactionCallbacks txCb;
        txCb.onBroadcasting = [status]() {
            status(QCoreApplication::translate("SendReceivePage", "Broadcasting transaction..."),
                   false);
        };
        txCb.onSent = [callbacks, finish](const QString& signature) {
            if (callbacks.onSuccess) {
                callbacks.onSuccess(signature);
            }
            finish();
        };
        txCb.onFailed = [status, finish](const QString& error) {
            status(error, true);
            finish();
        };
        executeSimpleTransactionAsync(txInput, txCb);
    };
    rentCallbacks.onFailed = [status, finish](const QString& error) {
        status(QCoreApplication::translate("SendReceivePage", "Failed to fetch rent cost: %1")
                   .arg(error),
               true);
        finish();
    };
    rentCallbacks.onTimedOut = [status, finish]() {
        status(QCoreApplication::translate("SendReceivePage", "Timed out fetching rent cost."),
               true);
        finish();
    };
    fetchMinimumBalanceWithTimeout(rentRequest, rentCallbacks);
}

SendReceiveTransferFeeInfo
SendReceiveHandler::parseTransferFeeInfo(const QByteArray& mintAccountData, double totalAmount,
                                         int tokenDecimals) const {
    SendReceiveTransferFeeInfo result;
    if (mintAccountData.size() < kMintDataMinSizeForExtensions) {
        return result;
    }

    int offset = kMintExtStartOffset;
    while (offset + kMintExtHeaderLen <= mintAccountData.size()) {
        const quint16 extType = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar*>(mintAccountData.data() + offset));
        const quint16 extLen = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar*>(mintAccountData.data() + offset + 2));
        offset += kMintExtHeaderLen;

        if (extType == kTransferFeeConfigExtType && extLen >= kTransferFeeConfigMinLen &&
            offset + extLen <= mintAccountData.size()) {
            const uchar* ext = reinterpret_cast<const uchar*>(mintAccountData.data() + offset);
            const quint16 basisPoints =
                qFromLittleEndian<quint16>(ext + kTransferFeeBasisPointsOffset);
            const quint64 maxRaw = qFromLittleEndian<quint64>(ext + kTransferFeeMaxRawOffset);
            if (basisPoints > 0) {
                result.basisPoints = basisPoints;
                result.maxRaw = maxRaw;
                result.maxHuman = maxRaw / std::pow(10.0, tokenDecimals);
                result.estimatedFee =
                    std::min(totalAmount * basisPoints / kBasisPointsDivisor, result.maxHuman);
                result.found = true;
            }
            return result;
        }
        offset += extLen;
    }

    return result;
}

void SendReceiveHandler::fetchTransferFeeInfo(const SendReceiveTokenMeta& meta, double totalAmount,
                                              SolanaApi* solanaApi, QObject* context,
                                              const FetchTransferFeeCallbacks& callbacks) const {
    if (!solanaApi || !context) {
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "Could not check transfer fees."));
        }
        return;
    }

    auto* guard = new QObject(context);
    auto done = std::make_shared<bool>(false);

    QTimer::singleShot(kRpcShortLookupTimeoutMs, guard, [guard, done, callbacks]() {
        if (*done) {
            return;
        }
        *done = true;
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "Could not check transfer fees."));
        }
        guard->deleteLater();
    });

    QObject::connect(
        solanaApi, &SolanaApi::accountInfoReady, guard,
        [this, guard, done, callbacks, meta,
         totalAmount](const QString& addr, const QByteArray& data, const QString&, quint64) {
            if (*done || addr != meta.mint) {
                return;
            }
            *done = true;

            if (callbacks.onReady) {
                callbacks.onReady(parseTransferFeeInfo(data, totalAmount, meta.decimals));
            }
            guard->deleteLater();
        });

    QObject::connect(solanaApi, &SolanaApi::requestFailed, guard,
                     [guard, done, callbacks](const QString& method, const QString&) {
                         if (*done || method != kMethodAccountInfo) {
                             return;
                         }
                         *done = true;
                         if (callbacks.onFailed) {
                             callbacks.onFailed(QCoreApplication::translate(
                                 "SendReceivePage", "Could not check transfer fees."));
                         }
                         guard->deleteLater();
                     });

    solanaApi->fetchAccountInfo(meta.mint, "base64");
}
