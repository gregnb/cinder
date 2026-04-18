#ifndef CREATETOKENHANDLER_H
#define CREATETOKENHANDLER_H

#include "features/sendreceive/SendReceiveHandler.h"
#include "models/SendReceive.h"
#include <QStringList>

class QObject;
class SolanaApi;
class Signer;
class Keypair;

class CreateTokenHandler {
  public:
    struct UploadCostState {
        quint64 storageLamports = 0;
        quint64 rentLamports = 0;
        bool storageReady = false;
        bool rentReady = false;
        bool storageFailed = false;
        bool rentFailed = false;
    };

    struct UploadCostCallbacks {
        std::function<void(const QString& html, const QString& warningText, bool showWarning,
                           quint64 storageLamports)>
            onUpdated;
    };
    struct ReviewCostCallbacks {
        std::function<void(const SendReceiveCreateTokenCostSummary& summary)> onReady;
        std::function<void(const QString& errorText)> onFailed;
    };

    explicit CreateTokenHandler(SendReceiveHandler* sharedHandler);

    QString formatCryptoAmount(double amount) const;
    quint64 computeMintAccountSize(const SendReceiveMintSizeInput& input) const;
    SendReceiveWalletSolContext loadWalletSolContext(const QString& walletAddress) const;
    SendReceiveCreateTokenCostSummary buildCreateTokenCostSummary(const QString& walletAddress,
                                                                  quint64 rentLamports,
                                                                  quint64 uploadLamports) const;
    SendReceiveCreateTokenFormState
    parseCreateTokenFormState(const SendReceiveCreateTokenBuildInput& input) const;
    SendReceiveCreateTokenValidationResult
    validateCreateTokenForm(const SendReceiveCreateTokenFormState& state) const;
    SendReceiveCreateTokenRequest
    buildCreateTokenRequest(const SendReceiveCreateTokenFormState& state) const;
    QStringList buildExtensionSummary(bool hasTransferFee, int feeBasisPoints, double feeMax,
                                      bool hasNonTransferable, bool hasMintClose,
                                      bool hasPermanentDelegate) const;
    void executeCreateTokenFlow(const SendReceiveCreateTokenRequest& request,
                                const Keypair& mintKeypair, SolanaApi* solanaApi, Signer* signer,
                                QObject* context,
                                const SendReceiveHandler::CreateTokenCallbacks& callbacks) const;
    void fetchUploadAndRentCosts(const QString& imagePath, const QString& walletAddress,
                                 quint64 mintAccountSize, SolanaApi* solanaApi, QObject* context,
                                 const UploadCostCallbacks& callbacks) const;
    void fetchReviewCostSummary(const QString& walletAddress, quint64 mintAccountSize,
                                quint64 uploadLamports, SolanaApi* solanaApi, QObject* context,
                                const ReviewCostCallbacks& callbacks) const;

  private:
    void emitUploadCostUpdate(const QString& walletAddress, const UploadCostState& state,
                              const UploadCostCallbacks& callbacks) const;

    SendReceiveHandler* m_sharedHandler = nullptr;
};

#endif // CREATETOKENHANDLER_H
