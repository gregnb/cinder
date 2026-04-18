#ifndef TRANSACTIONEXECUTOR_H
#define TRANSACTIONEXECUTOR_H

#include "crypto/Keypair.h"
#include "crypto/Signer.h"
#include "tx/TransactionBuilder.h"
#include "tx/TransactionInstruction.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

class SolanaApi;

enum class TransactionSigningFailureKind {
    MissingSigner,
    BuildMessage,
    PrimarySignature,
    ExtraSignatures,
    BuildSigned
};

struct TransactionSigningFailure {
    TransactionSigningFailureKind kind = TransactionSigningFailureKind::BuildMessage;
    QString detail;
};

using TransactionAppendSignaturesCallback =
    std::function<bool(const QByteArray& message, QList<QByteArray>* signatures, QString* error)>;

struct TransactionSigningRequest {
    TransactionBuilder builder;
    Signer* signer = nullptr;
    QObject* context = nullptr;
    TransactionAppendSignaturesCallback appendSignatures;
};

using TransactionSigningSuccess =
    std::function<void(const QByteArray& signedTx, const QByteArray& message)>;
using TransactionSigningError = std::function<void(const TransactionSigningFailure& failure)>;

void signTransactionAsync(TransactionSigningRequest request,
                          const TransactionSigningSuccess& onSuccess,
                          const TransactionSigningError& onError);

struct TransactionSubmitRequest {
    SolanaApi* api = nullptr;
    QObject* context = nullptr;
    QByteArray signedTransaction;
    int timeoutMs = 30000;
    QString expectedFailMethod;
};

struct TransactionSubmitCallbacks {
    std::function<void(const QString& signature)> onSent;
    std::function<void(const QString& error)> onFailed;
    std::function<void()> onTimedOut;
};

void submitSignedTransactionAsync(const TransactionSubmitRequest& request,
                                  const TransactionSubmitCallbacks& callbacks);

struct TransactionExecutionRequest {
    TransactionSigningRequest signing;
    SolanaApi* api = nullptr;
    QObject* context = nullptr;
    int submitTimeoutMs = 0;
    QString submitFailMethod;
};

struct TransactionExecutionCallbacks {
    std::function<void(const QByteArray& signedTx, const QByteArray& message)> onSigned;
    std::function<void(const TransactionSigningFailure& failure)> onSigningFailed;
    std::function<void(const QString& signature)> onSent;
    std::function<void(const QString& error)> onSubmitFailed;
    std::function<void()> onSubmitTimedOut;
};

void signAndSubmitTransactionAsync(const TransactionExecutionRequest& request,
                                   const TransactionExecutionCallbacks& callbacks);

struct LatestBlockhashRequest {
    SolanaApi* api = nullptr;
    QObject* context = nullptr;
    int timeoutMs = 0;
    QString commitment;
    QString failMethod;
};

struct LatestBlockhashCallbacks {
    std::function<void(const QString& blockhash, quint64 lastValidBlockHeight)> onReady;
    std::function<void(const QString& error)> onFailed;
    std::function<void()> onTimedOut;
};

void fetchLatestBlockhashWithTimeout(const LatestBlockhashRequest& request,
                                     const LatestBlockhashCallbacks& callbacks);

struct MinimumBalanceRequest {
    SolanaApi* api = nullptr;
    QObject* context = nullptr;
    quint64 dataSize = 0;
    int timeoutMs = 0;
    QString failMethod;
};

struct MinimumBalanceCallbacks {
    std::function<void(quint64 lamports)> onReady;
    std::function<void(const QString& error)> onFailed;
    std::function<void()> onTimedOut;
};

void fetchMinimumBalanceWithTimeout(const MinimumBalanceRequest& request,
                                    const MinimumBalanceCallbacks& callbacks);

// ── Transaction flow (build helpers) ─────────────────────────

enum class TransactionSigningErrorCategory { Build, Signing, MissingSigner };

struct TransactionExecutionBuildInput {
    QString feePayer;
    QString blockhash;
    QList<TransactionInstruction> instructions;
    bool useNonce = false;
    QString nonceAddress;
    QString nonceAuthority;
    QString nonceValue;

    SolanaApi* api = nullptr;
    QObject* context = nullptr;
    Signer* signer = nullptr;
    TransactionAppendSignaturesCallback appendSignatures;

    int submitTimeoutMs = 0;
    QString submitFailMethod;
};

bool buildExecutionRequest(const TransactionExecutionBuildInput& input,
                           TransactionExecutionRequest* request, QString* error);

TransactionSigningErrorCategory classifySigningFailure(const TransactionSigningFailure& failure);

QString signingFailureMessage(const TransactionSigningFailure& failure);

// ── Simple transaction execution ─────────────────────────────

struct SimpleTransactionInput {
    QString feePayer;
    QList<TransactionInstruction> instructions;
    SolanaApi* api = nullptr;
    QObject* context = nullptr;
    Signer* signer = nullptr;
    TransactionAppendSignaturesCallback appendSignatures;

    bool useNonce = false;
    QString nonceAddress;
    QString nonceAuthority;
    QString nonceValue;
};

struct SimpleTransactionCallbacks {
    std::function<void()> onBroadcasting;
    std::function<void(const QString& signature)> onSent;
    std::function<void(const QString& error)> onFailed;
};

void executeSimpleTransactionAsync(const SimpleTransactionInput& input,
                                   const SimpleTransactionCallbacks& callbacks);

// ── Additional signer helper ─────────────────────────────────

namespace AdditionalSigner {
    TransactionAppendSignaturesCallback keypairSignatureAppender(const Keypair& keypair,
                                                                 const QString& errorText);
}

#endif // TRANSACTIONEXECUTOR_H
