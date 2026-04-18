#ifndef SYSTEMOPERATIONBUILDER_H
#define SYSTEMOPERATIONBUILDER_H

#include "tx/TransactionInstruction.h"

#include <QList>
#include <QString>
#include <QtGlobal>

// ── Nonce operations ────────────────────────────────────────

struct NonceCreateOperationInput {
    QString walletAddress;
    QString nonceAddress;
    QString authorityAddress;
    quint64 rentLamports = 0;
};

struct NonceCreateOperationResult {
    bool ok = false;
    QString error;
    QList<TransactionInstruction> instructions;
};

// ── Stake operations ────────────────────────────────────────

struct StakeCreateAndDelegateInput {
    QString walletAddress;
    QString stakeAddress;
    QString voteAccount;
    quint64 stakeLamports = 0;
    quint64 rentLamports = 0;
};

struct StakeCreateAndDelegateResult {
    bool ok = false;
    QString error;
    QList<TransactionInstruction> instructions;
};

// ── Builder ─────────────────────────────────────────────────

class SystemOperationBuilder {
  public:
    // Nonce
    static NonceCreateOperationResult
    buildNonceCreateAccount(const NonceCreateOperationInput& input);
    static TransactionInstruction buildNonceAdvance(const QString& nonceAddress,
                                                    const QString& authorityAddress);
    static TransactionInstruction buildNonceWithdraw(const QString& nonceAddress,
                                                     const QString& toAddress,
                                                     const QString& authorityAddress,
                                                     quint64 lamports);

    // Stake
    static StakeCreateAndDelegateResult
    buildStakeCreateAndDelegate(const StakeCreateAndDelegateInput& input);
    static TransactionInstruction buildStakeDeactivate(const QString& stakeAddress,
                                                       const QString& authorityAddress);
    static TransactionInstruction buildStakeWithdraw(const QString& stakeAddress,
                                                     const QString& toAddress,
                                                     const QString& authorityAddress,
                                                     quint64 lamports);
};

#endif // SYSTEMOPERATIONBUILDER_H
