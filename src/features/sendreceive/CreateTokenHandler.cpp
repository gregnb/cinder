#include "features/sendreceive/CreateTokenHandler.h"

#include "services/IrysApi.h"
#include "services/SolanaApi.h"
#include "tx/TokenEncodingUtils.h"

#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <memory>

namespace {
    constexpr double kLamportsPerSol = 1e9;
    constexpr double kNetworkFeeSol = 0.000005;
    constexpr char kMethodMinimumBalance[] = "getMinimumBalanceForRentExemption";
    constexpr int kRentLookupTimeoutMs = 10000;

    QString buildUploadCostHtml(const CreateTokenHandler::UploadCostState& state,
                                double solPriceUsd) {
        const double storageSol = static_cast<double>(state.storageLamports) / kLamportsPerSol;
        const double rentSol = static_cast<double>(state.rentLamports) / kLamportsPerSol;

        QString html = "<table cellspacing='0' cellpadding='1'>";

        if (state.storageReady) {
            html += QString("<tr>"
                            "<td style='color:rgba(255,255,255,0.5); font-size:12px; "
                            "padding-right:12px;'>Storage</td>"
                            "<td style='color:white; font-size:12px;' align='right'>%1 SOL</td>"
                            "</tr>")
                        .arg(QString::number(storageSol, 'f', 9));
        } else if (state.storageFailed) {
            html += "<tr><td style='color:rgba(255,255,255,0.5); font-size:12px;'>Storage</td>"
                    "<td style='color:#ef4444; font-size:12px;' align='right'>unavailable</td>"
                    "</tr>";
        }

        if (state.rentReady) {
            html += QString("<tr>"
                            "<td style='color:rgba(255,255,255,0.5); font-size:12px; "
                            "padding-right:12px;'>Rent</td>"
                            "<td style='color:white; font-size:12px;' align='right'>%1 SOL</td>"
                            "</tr>")
                        .arg(QString::number(rentSol, 'f', 6));
        } else if (state.rentFailed) {
            html += "<tr><td style='color:rgba(255,255,255,0.5); font-size:12px;'>Rent</td>"
                    "<td style='color:#ef4444; font-size:12px;' align='right'>unavailable</td>"
                    "</tr>";
        } else {
            html += "<tr><td style='color:rgba(255,255,255,0.5); font-size:12px;'>Rent</td>"
                    "<td style='color:rgba(255,255,255,0.3); font-size:12px;' "
                    "align='right'>fetching...</td></tr>";
        }

        html += QString("<tr>"
                        "<td style='color:rgba(255,255,255,0.5); font-size:12px; "
                        "padding-right:12px;'>Network fee</td>"
                        "<td style='color:white; font-size:12px;' align='right'>~%1 SOL</td>"
                        "</tr>")
                    .arg(QString::number(kNetworkFeeSol, 'f', 6));

        const bool allDone =
            (state.storageReady || state.storageFailed) && (state.rentReady || state.rentFailed);
        if (allDone) {
            const double totalSol = storageSol + rentSol + kNetworkFeeSol;
            html += "<tr><td colspan='2' style='color:rgba(255,255,255,0.15); "
                    "font-size:8px; padding-top:2px;'>"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                    "</td></tr>";

            const QString totalStr = QString::number(totalSol, 'f', 6);
            if (solPriceUsd > 0.0) {
                const double usd = totalSol * solPriceUsd;
                html += QString("<tr>"
                                "<td style='color:white; font-size:13px; font-weight:600; "
                                "padding-right:12px;'>Total</td>"
                                "<td style='color:white; font-size:13px; font-weight:600;' "
                                "align='right'>%1 SOL "
                                "<span style='color:rgba(255,255,255,0.45);'>($%2)</span>"
                                "</td></tr>")
                            .arg(totalStr, QString::number(usd, 'f', 2));
            } else {
                html += QString("<tr>"
                                "<td style='color:white; font-size:13px; font-weight:600; "
                                "padding-right:12px;'>Total</td>"
                                "<td style='color:white; font-size:13px; font-weight:600;' "
                                "align='right'>%1 SOL</td></tr>")
                            .arg(totalStr);
            }
        }

        html += "</table>";
        return html;
    }
} // namespace

CreateTokenHandler::CreateTokenHandler(SendReceiveHandler* sharedHandler)
    : m_sharedHandler(sharedHandler) {}

QString CreateTokenHandler::formatCryptoAmount(double amount) const {
    return m_sharedHandler ? m_sharedHandler->formatCryptoAmount(amount)
                           : QString::number(amount, 'f', 9);
}

quint64 CreateTokenHandler::computeMintAccountSize(const SendReceiveMintSizeInput& input) const {
    return m_sharedHandler ? m_sharedHandler->computeMintAccountSize(input) : 0;
}

SendReceiveWalletSolContext
CreateTokenHandler::loadWalletSolContext(const QString& walletAddress) const {
    if (!m_sharedHandler) {
        return {};
    }
    return m_sharedHandler->loadWalletSolContext(walletAddress);
}

SendReceiveCreateTokenCostSummary
CreateTokenHandler::buildCreateTokenCostSummary(const QString& walletAddress, quint64 rentLamports,
                                                quint64 uploadLamports) const {
    if (!m_sharedHandler) {
        return {};
    }
    return m_sharedHandler->buildCreateTokenCostSummary(walletAddress, rentLamports,
                                                        uploadLamports);
}

SendReceiveCreateTokenFormState
CreateTokenHandler::parseCreateTokenFormState(const SendReceiveCreateTokenBuildInput& input) const {
    SendReceiveCreateTokenFormState state;
    state.walletAddress = input.walletAddress;
    state.mintAddress = input.mintAddress;
    state.name = input.name.trimmed();
    state.symbol = input.symbol.trimmed();
    state.uri = input.uri.trimmed();
    state.decimals = input.decimals;
    state.freezeAuthorityEnabled = input.freezeAuthorityEnabled;
    state.hasTransferFee = input.hasTransferFee;
    state.feeBasisPoints = input.feeBasisPoints;
    state.hasNonTransferable = input.hasNonTransferable;
    state.hasMintClose = input.hasMintClose;
    state.hasPermanentDelegate = input.hasPermanentDelegate;
    state.mintAccountSize = input.mintAccountSize;

    bool supplyOk = false;
    const double supply = input.supplyText.trimmed().toDouble(&supplyOk);
    if (supplyOk && supply > 0.0) {
        state.initialSupply = supply;
        state.hasInitialSupply = true;
    }

    bool feeMaxOk = false;
    const double feeMax = input.feeMaxText.trimmed().toDouble(&feeMaxOk);
    if (feeMaxOk && feeMax > 0.0) {
        state.feeMax = feeMax;
        state.hasFeeMax = true;
    }

    return state;
}

SendReceiveCreateTokenValidationResult
CreateTokenHandler::validateCreateTokenForm(const SendReceiveCreateTokenFormState& state) const {
    SendReceiveCreateTokenValidationResult result;
    if (state.name.isEmpty() || state.symbol.isEmpty()) {
        result.errorCode = "missing_required_fields";
        return result;
    }
    if (state.hasTransferFee && state.feeBasisPoints <= 0) {
        result.errorCode = "invalid_transfer_fee";
        return result;
    }
    result.ok = true;
    return result;
}

SendReceiveCreateTokenRequest
CreateTokenHandler::buildCreateTokenRequest(const SendReceiveCreateTokenFormState& state) const {
    SendReceiveCreateTokenRequest request;
    request.walletAddress = state.walletAddress;
    request.mintAddress = state.mintAddress;
    request.name = state.name;
    request.symbol = state.symbol;
    request.uri = state.uri;
    request.decimals = state.decimals;
    request.freezeAuthority = state.freezeAuthorityEnabled ? request.walletAddress : QString();
    request.hasTransferFee = state.hasTransferFee;
    request.feeBasisPoints = static_cast<quint16>(state.feeBasisPoints);
    request.hasNonTransferable = state.hasNonTransferable;
    request.hasMintClose = state.hasMintClose;
    request.hasPermanentDelegate = state.hasPermanentDelegate;
    request.mintAccountSize = state.mintAccountSize;

    if (state.hasInitialSupply) {
        quint64 rawSupply = 0;
        if (TokenAmountCodec::toRaw(state.initialSupply, request.decimals, &rawSupply)) {
            request.rawSupply = rawSupply;
        }
    }
    if (request.hasTransferFee && state.hasFeeMax) {
        quint64 feeMaxRaw = 0;
        if (TokenAmountCodec::toRaw(state.feeMax, request.decimals, &feeMaxRaw)) {
            request.feeMaxRaw = feeMaxRaw;
        }
    }

    return request;
}

QStringList CreateTokenHandler::buildExtensionSummary(bool hasTransferFee, int feeBasisPoints,
                                                      double feeMax, bool hasNonTransferable,
                                                      bool hasMintClose,
                                                      bool hasPermanentDelegate) const {
    QStringList extensions;
    if (hasTransferFee) {
        const double pct = static_cast<double>(feeBasisPoints) / 100.0;
        extensions << QCoreApplication::translate("SendReceivePage", "Transfer Fee: %1% (max %2)")
                          .arg(QString::number(pct, 'f', 2))
                          .arg(formatCryptoAmount(feeMax));
    }
    if (hasNonTransferable) {
        extensions << QCoreApplication::translate("SendReceivePage",
                                                  "Non-Transferable (Soulbound)");
    }
    if (hasMintClose) {
        extensions << QCoreApplication::translate("SendReceivePage", "Mint Close Authority");
    }
    if (hasPermanentDelegate) {
        extensions << QCoreApplication::translate("SendReceivePage", "Permanent Delegate");
    }
    return extensions;
}

void CreateTokenHandler::executeCreateTokenFlow(
    const SendReceiveCreateTokenRequest& request, const Keypair& mintKeypair, SolanaApi* solanaApi,
    Signer* signer, QObject* context,
    const SendReceiveHandler::CreateTokenCallbacks& callbacks) const {
    if (!m_sharedHandler) {
        return;
    }
    m_sharedHandler->executeCreateTokenFlow(request, mintKeypair, solanaApi, signer, context,
                                            callbacks);
}

void CreateTokenHandler::fetchUploadAndRentCosts(const QString& imagePath,
                                                 const QString& walletAddress,
                                                 quint64 mintAccountSize, SolanaApi* solanaApi,
                                                 QObject* context,
                                                 const UploadCostCallbacks& callbacks) const {
    if (!context || !callbacks.onUpdated) {
        return;
    }

    const qint64 fileSize = QFile(imagePath).size();
    if (fileSize <= 0) {
        return;
    }

    auto state = std::make_shared<UploadCostState>();

    auto* irysApi = new IrysApi(context);
    irysApi->fetchSolanaStoragePriceLamports(
        fileSize,
        [this, walletAddress, state, callbacks, irysApi](quint64 lamports) {
            state->storageLamports = lamports;
            state->storageReady = true;
            emitUploadCostUpdate(walletAddress, *state, callbacks);
            irysApi->deleteLater();
        },
        [this, walletAddress, state, callbacks, irysApi](const QString&) {
            state->storageFailed = true;
            emitUploadCostUpdate(walletAddress, *state, callbacks);
            irysApi->deleteLater();
        });

    if (solanaApi) {
        auto* guard = new QObject(context);
        QObject::connect(solanaApi, &SolanaApi::minimumBalanceReady, guard,
                         [this, walletAddress, state, callbacks, guard](quint64 lamports) {
                             state->rentLamports = lamports;
                             state->rentReady = true;
                             guard->deleteLater();
                             emitUploadCostUpdate(walletAddress, *state, callbacks);
                         });
        QObject::connect(
            solanaApi, &SolanaApi::requestFailed, guard,
            [this, walletAddress, state, callbacks, guard](const QString& method, const QString&) {
                if (method != kMethodMinimumBalance) {
                    return;
                }
                state->rentFailed = true;
                guard->deleteLater();
                emitUploadCostUpdate(walletAddress, *state, callbacks);
            });
        solanaApi->fetchMinimumBalanceForRentExemption(mintAccountSize);
    }
}

void CreateTokenHandler::fetchReviewCostSummary(const QString& walletAddress,
                                                quint64 mintAccountSize, quint64 uploadLamports,
                                                SolanaApi* solanaApi, QObject* context,
                                                const ReviewCostCallbacks& callbacks) const {
    if (!solanaApi || !context) {
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "Could not fetch rent cost."));
        }
        return;
    }

    auto* guard = new QObject(context);
    auto done = std::make_shared<bool>(false);

    QTimer::singleShot(kRentLookupTimeoutMs, guard, [guard, done, callbacks]() {
        if (*done) {
            return;
        }
        *done = true;
        if (callbacks.onFailed) {
            callbacks.onFailed(
                QCoreApplication::translate("SendReceivePage", "Could not fetch rent cost."));
        }
        guard->deleteLater();
    });

    QObject::connect(
        solanaApi, &SolanaApi::minimumBalanceReady, guard,
        [this, guard, done, callbacks, walletAddress, uploadLamports](quint64 lamports) {
            if (*done) {
                return;
            }
            *done = true;
            if (callbacks.onReady) {
                callbacks.onReady(
                    buildCreateTokenCostSummary(walletAddress, lamports, uploadLamports));
            }
            guard->deleteLater();
        });

    QObject::connect(solanaApi, &SolanaApi::requestFailed, guard,
                     [guard, done, callbacks](const QString& method, const QString&) {
                         if (*done || method != kMethodMinimumBalance) {
                             return;
                         }
                         *done = true;
                         if (callbacks.onFailed) {
                             callbacks.onFailed(QCoreApplication::translate(
                                 "SendReceivePage", "Could not fetch rent cost."));
                         }
                         guard->deleteLater();
                     });

    solanaApi->fetchMinimumBalanceForRentExemption(mintAccountSize);
}

void CreateTokenHandler::emitUploadCostUpdate(const QString& walletAddress,
                                              const UploadCostState& state,
                                              const UploadCostCallbacks& callbacks) const {
    if (!callbacks.onUpdated) {
        return;
    }

    const SendReceiveWalletSolContext walletContext = loadWalletSolContext(walletAddress);
    const QString html = buildUploadCostHtml(state, walletContext.solPriceUsd);

    bool showWarning = false;
    QString warningText;
    const bool allDone =
        (state.storageReady || state.storageFailed) && (state.rentReady || state.rentFailed);
    if (allDone) {
        const double storageSol = static_cast<double>(state.storageLamports) / kLamportsPerSol;
        const double rentSol = static_cast<double>(state.rentLamports) / kLamportsPerSol;
        const double totalSol = storageSol + rentSol + kNetworkFeeSol;
        if (walletContext.walletSolBalance < totalSol) {
            showWarning = true;
            warningText = QCoreApplication::translate(
                              "SendReceivePage", "Insufficient balance — need %1 SOL, have %2 SOL")
                              .arg(QString::number(totalSol, 'f', 6),
                                   QString::number(walletContext.walletSolBalance, 'f', 6));
        }
    }

    callbacks.onUpdated(html, warningText, showWarning, state.storageLamports);
}
