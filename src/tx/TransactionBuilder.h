#ifndef TRANSACTIONBUILDER_H
#define TRANSACTIONBUILDER_H

#include "tx/TransactionInstruction.h"
#include <QByteArray>
#include <QList>
#include <QString>
#include <optional>

class TransactionBuilder {
  public:
    enum class Version { Legacy, V0 };

    TransactionBuilder();

    // ── Fluent setters ───────────────────────────────────

    TransactionBuilder& setVersion(Version version);
    TransactionBuilder& setFeePayer(const QString& pubkey);
    TransactionBuilder& setRecentBlockhash(const QString& blockhash);

    // Sets blockhash to nonceValue and auto-prepends advanceNonce
    // as the first instruction.
    TransactionBuilder& useNonce(const QString& noncePubkey, const QString& authority,
                                 const QString& nonceValue);

    TransactionBuilder& addInstruction(const TransactionInstruction& ix);

    // ── Build outputs ────────────────────────────────────

    // Serialize the message (the data that signers will sign).
    QByteArray serializeMessage() const;

    // Attach signatures and produce the full serialized transaction.
    // Each signature must be exactly 64 bytes. One per required signer,
    // in compiled account order (fee payer first).
    QByteArray buildSigned(const QList<QByteArray>& signatures) const;

    QString lastError() const;
    int numRequiredSignatures() const;

  private:
    struct CompiledMessage {
        uint8_t numRequiredSignatures = 0;
        uint8_t numReadonlySignedAccounts = 0;
        uint8_t numReadonlyUnsignedAccounts = 0;
        QList<QByteArray> accountKeys; // 32 bytes each
        QByteArray recentBlockhash;    // 32 bytes
        struct CompiledInstruction {
            uint8_t programIdIndex;
            QList<uint8_t> accountIndexes;
            QByteArray data;
        };
        QList<CompiledInstruction> instructions;
    };

    std::optional<CompiledMessage> compile() const;
    QByteArray serializeCompiled(const CompiledMessage& msg) const;

    Version m_version = Version::Legacy;
    QString m_feePayer;
    QString m_recentBlockhash;
    QList<TransactionInstruction> m_instructions;

    mutable QString m_lastError;
};

#endif // TRANSACTIONBUILDER_H
