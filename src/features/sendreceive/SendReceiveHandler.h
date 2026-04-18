#ifndef SENDRECEIVEHANDLER_H
#define SENDRECEIVEHANDLER_H

#include "models/SendReceive.h"
#include "services/model/TransactionResponse.h"
#include "tx/TransactionBuilder.h"
#include <QMap>
#include <functional>

class QObject;
class SolanaApi;
class Signer;
class Keypair;

class SendReceiveHandler {
  public:
    struct CreateTokenCallbacks {
        std::function<void(const QString& text, bool isError)> onStatus;
        std::function<void(const QString& signature)> onSuccess;
        std::function<void()> onFinished;
    };
    struct CreateNonceCallbacks {
        std::function<void(const QString& phaseText)> onProgress;
        std::function<void(const QString& errorText)> onError;
        std::function<void(const QString& signature, const QString& nonceAddress)>
            onTransactionSent;
    };
    struct PollNonceCallbacks {
        std::function<void(int attempt, int maxAttempts)> onAttempt;
        std::function<void(const QString& errorText)> onFailed;
        std::function<void(const QString& nonceAddress, const QString& nonceValue)> onSuccess;
    };
    struct RefreshStoredNonceCallbacks {
        std::function<void(const QString& nonceValue)> onUpdated;
    };
    struct FetchNonceRentCallbacks {
        std::function<void(quint64 lamports)> onReady;
        std::function<void(const QString& errorText)> onFailed;
    };
    struct FetchTransferFeeCallbacks {
        std::function<void(const SendReceiveTransferFeeInfo& feeInfo)> onReady;
        std::function<void(const QString& errorText)> onFailed;
    };
    struct ResolveNonceToggleCallbacks {
        std::function<void(const QString& address, const QString& nonceValue)> onStoredNonce;
        std::function<void(const QString& nonceValue)> onStoredNonceUpdated;
        std::function<void()> onPendingNonceRetry;
        std::function<void()> onCreateRequired;
        std::function<void(quint64 lamports)> onRentReady;
        std::function<void(const QString& errorText)> onRentFailed;
    };
    struct CreateAndPollNonceCallbacks {
        std::function<void(const QString& phaseText)> onProgress;
        std::function<void(const QString& txSignature, const QString& nonceAddress)>
            onTransactionSent;
        std::function<void(int attempt, int maxAttempts)> onPollAttempt;
        std::function<void(const QString& errorText)> onFailed;
        std::function<void(const QString& nonceAddress, const QString& nonceValue)> onSuccess;
    };
    struct ExecuteSendCallbacks {
        std::function<void(const QString& text, bool isError)> onStatus;
        std::function<void(const QString& signature)> onSuccess;
        std::function<void()> onFinished;
    };
    struct PollTransactionCallbacks {
        std::function<void(int attempt, int maxAttempts)> onAttempt;
        std::function<void()> onStatusConfirmed;                           // status = "confirmed"
        std::function<void()> onStatusFinalized;                           // status = "finalized"
        std::function<void(const TransactionResponse& tx)> onDetailsReady; // getTransaction data
        std::function<void(const TransactionResponse& tx)> onFailed;
        std::function<void(const QString& errorText)> onTimeout;
    };

    static constexpr int RECIPIENTS_PER_TX = 20;

    QString formatCryptoAmount(double amount) const;
    int batchCount(int recipientCount) const;

    SendReceiveTokenRefreshResult refreshTokenRows(const QString& walletAddress,
                                                   const QString& currentIcon) const;

    QList<SendReceiveRecipientReview>
    collectReviewRecipients(const QList<SendReceiveRecipientInput>& inputs) const;
    SendReceiveReviewData buildReviewData(const SendReceiveReviewBuildRequest& request) const;
    QList<SendReceiveRecipient>
    collectValidRecipients(const QList<SendReceiveRecipientInput>& inputs) const;
    bool hasValidRecipient(const QList<SendReceiveRecipientInput>& inputs) const;
    SendReceivePrepareSendResult
    prepareSendExecution(const SendReceivePrepareSendRequest& request) const;
    bool buildSendInstructions(const QString& walletAddress, const SendReceiveTokenMeta& meta,
                               const QList<SendReceiveRecipient>& recipients,
                               quint16 transferFeeBasisPoints, quint64 transferFeeMax,
                               QList<TransactionInstruction>* instructions, QString* error) const;
    SendReceiveWalletSolContext loadWalletSolContext(const QString& walletAddress) const;
    SendReceiveCreateTokenCostSummary buildCreateTokenCostSummary(const QString& walletAddress,
                                                                  quint64 rentLamports,
                                                                  quint64 uploadLamports) const;
    quint64 computeMintAccountSize(const SendReceiveMintSizeInput& input) const;
    void executeCreateTokenFlow(const SendReceiveCreateTokenRequest& request,
                                const Keypair& mintKeypair, SolanaApi* solanaApi, Signer* signer,
                                QObject* context, const CreateTokenCallbacks& callbacks) const;
    void executeCreateNonceFlow(const QString& walletAddress, quint64 rentLamports,
                                SolanaApi* solanaApi, Signer* signer, QObject* context,
                                const CreateNonceCallbacks& callbacks) const;
    void pollNonceAccountFlow(const QString& nonceAddress, const QString& authorityAddress,
                              SolanaApi* solanaApi, QObject* context,
                              const PollNonceCallbacks& callbacks) const;
    SendReceiveStoredNonce lookupStoredNonce(const QString& walletAddress) const;
    void refreshStoredNonceValue(const QString& nonceAddress, SolanaApi* solanaApi,
                                 QObject* context,
                                 const RefreshStoredNonceCallbacks& callbacks) const;
    void fetchNonceRentCost(SolanaApi* solanaApi, QObject* context,
                            const FetchNonceRentCallbacks& callbacks) const;
    void fetchTransferFeeInfo(const SendReceiveTokenMeta& meta, double totalAmount,
                              SolanaApi* solanaApi, QObject* context,
                              const FetchTransferFeeCallbacks& callbacks) const;
    void resolveNonceToggleEnabled(const QString& walletAddress, const QString& pendingNonceAddress,
                                   SolanaApi* solanaApi, QObject* context,
                                   const ResolveNonceToggleCallbacks& callbacks) const;
    void executeCreateAndPollNonceFlow(const QString& walletAddress, quint64 rentLamports,
                                       SolanaApi* solanaApi, Signer* signer, QObject* context,
                                       const CreateAndPollNonceCallbacks& callbacks) const;
    void executeSendFlow(const SendReceiveExecutionRequest& request, SolanaApi* solanaApi,
                         Signer* signer, QObject* context,
                         const ExecuteSendCallbacks& callbacks) const;
    void pollTransactionConfirmationFlow(const QString& signature, SolanaApi* solanaApi,
                                         QObject* context,
                                         const PollTransactionCallbacks& callbacks) const;
    QString buildReviewCsv(const QList<SendReceiveRecipientInput>& recipientInputs,
                           const QString& tokenSymbol, const QString& walletAddress,
                           const QMap<QString, SendReceiveTokenMeta>& tokenMeta,
                           const QString& currentTokenIcon) const;

    SendReceiveTransferFeeInfo parseTransferFeeInfo(const QByteArray& mintAccountData,
                                                    double totalAmount, int tokenDecimals) const;

  private:
    bool buildCreateTokenInstructions(const SendReceiveCreateTokenRequest& request,
                                      quint64 rentLamports,
                                      QList<TransactionInstruction>* instructions,
                                      QString* error) const;
};

#endif // SENDRECEIVEHANDLER_H
