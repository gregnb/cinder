#ifndef SENDRECEIVE_H
#define SENDRECEIVE_H

#include "tx/TransactionBuilder.h"
#include <QList>
#include <QMap>
#include <QString>

struct SendReceiveTokenMeta {
    QString mint;
    QString accountAddress;
    int decimals = 0;
    QString tokenProgram;
};

struct SendReceiveTokenRow {
    QString icon;
    QString display;
    QString balance;
    SendReceiveTokenMeta meta;
};

struct SendReceiveTokenRefreshResult {
    QList<SendReceiveTokenRow> rows;
    QString selectedIcon;
    QString selectedDisplay;
    QString selectedBalance;
};

struct SendReceiveRecipientInput {
    QString address;
    QString amountText;
};

struct SendReceiveRecipientReview {
    QString address;
    QString name;
    QString amountText;
    double amount = 0.0;
};

struct SendReceiveRecipient {
    QString address;
    double amount = 0.0;
};

struct SendReceiveReviewBuildRequest {
    QString currentTokenDisplayText;
    QString currentTokenIcon;
    QMap<QString, SendReceiveTokenMeta> tokenMeta;
    QList<SendReceiveRecipientInput> recipientInputs;
};

struct SendReceiveReviewData {
    bool ok = false;
    QString tokenSymbol;
    QString tokenIcon;
    SendReceiveTokenMeta meta;
    bool isSol = true;
    QList<SendReceiveRecipientReview> recipients;
    double totalAmount = 0.0;
    int totalRecipients = 0;
    int numBatches = 0;
};

struct SendReceiveTransferFeeInfo {
    quint16 basisPoints = 0;
    quint64 maxRaw = 0;
    double estimatedFee = 0.0;
    double maxHuman = 0.0;
    bool found = false;
};

struct SendReceiveMintSizeInput {
    QString name;
    QString symbol;
    QString uri;
    bool hasTransferFee = false;
    bool hasNonTransferable = false;
    bool hasMintClose = false;
    bool hasPermanentDelegate = false;
};

struct SendReceiveCreateTokenRequest {
    QString walletAddress;
    QString mintAddress;
    QString name;
    QString symbol;
    QString uri;
    QString freezeAuthority;
    int decimals = 0;
    quint64 rawSupply = 0;
    bool hasTransferFee = false;
    quint16 feeBasisPoints = 0;
    quint64 feeMaxRaw = 0;
    bool hasNonTransferable = false;
    bool hasMintClose = false;
    bool hasPermanentDelegate = false;
    quint64 mintAccountSize = 0;
};

struct SendReceiveCreateTokenBuildInput {
    QString walletAddress;
    QString mintAddress;
    QString name;
    QString symbol;
    QString uri;
    int decimals = 0;
    QString supplyText;
    bool freezeAuthorityEnabled = false;
    bool hasTransferFee = false;
    int feeBasisPoints = 0;
    QString feeMaxText;
    bool hasNonTransferable = false;
    bool hasMintClose = false;
    bool hasPermanentDelegate = false;
    quint64 mintAccountSize = 0;
};

struct SendReceiveCreateTokenFormState {
    QString walletAddress;
    QString mintAddress;
    QString name;
    QString symbol;
    QString uri;
    int decimals = 0;
    bool freezeAuthorityEnabled = false;
    bool hasTransferFee = false;
    int feeBasisPoints = 0;
    bool hasNonTransferable = false;
    bool hasMintClose = false;
    bool hasPermanentDelegate = false;
    quint64 mintAccountSize = 0;
    double initialSupply = 0.0;
    double feeMax = 0.0;
    bool hasInitialSupply = false;
    bool hasFeeMax = false;
};

struct SendReceiveCreateTokenValidationResult {
    bool ok = false;
    QString errorCode;
};

struct SendReceiveCreateTokenCostSummary {
    QString rentText;
    QString totalText;
    bool insufficientSol = false;
    QString insufficientSolText;
};

struct SendReceiveWalletSolContext {
    double solPriceUsd = 0.0;
    double walletSolBalance = 0.0;
};

struct SendReceiveStoredNonce {
    bool found = false;
    QString address;
    QString nonceValue;
};

struct SendReceiveExecutionRequest {
    QString walletAddress;
    QList<TransactionInstruction> instructions;
    quint64 priorityFeeMicroLamports = 0;
    bool nonceEnabled = false;
    QString nonceAddress;
    QString nonceValue;
};

struct SendReceivePrepareSendRequest {
    QString walletAddress;
    SendReceiveTokenMeta tokenMeta;
    QList<SendReceiveRecipientInput> recipientInputs;
    quint16 transferFeeBasisPoints = 0;
    quint64 transferFeeMax = 0;
    bool nonceEnabled = false;
    QString nonceAddress;
    QString nonceValue;
};

enum class SendReceivePrepareSendError {
    None = 0,
    NoValidRecipients,
    DeriveTokenAccountFailed,
};

struct SendReceivePrepareSendResult {
    bool ok = false;
    SendReceivePrepareSendError error = SendReceivePrepareSendError::None;
    QString errorDetail;
    SendReceiveExecutionRequest executionRequest;
};

struct SendReceiveSuccessPageInfo {
    QString title;
    QString amount;
    QString tokenSymbol;
    QString recipient;
    QString sender;
    QString signature;
    QString networkFee;
    QString txVersion;
    QString result;
    QString mintAddress;
};

#endif // SENDRECEIVE_H
