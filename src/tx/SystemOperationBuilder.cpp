#include "tx/SystemOperationBuilder.h"

#include "tx/StakeInstruction.h"
#include "tx/SystemInstruction.h"

// ── Nonce operations ────────────────────────────────────────

NonceCreateOperationResult
SystemOperationBuilder::buildNonceCreateAccount(const NonceCreateOperationInput& input) {
    NonceCreateOperationResult result;
    if (input.walletAddress.isEmpty() || input.nonceAddress.isEmpty() ||
        input.authorityAddress.isEmpty() || input.rentLamports == 0) {
        result.error = QStringLiteral("invalid_input");
        return result;
    }

    result.instructions = SystemInstruction::createNonceAccount(
        input.walletAddress, input.nonceAddress, input.authorityAddress, input.rentLamports);
    result.ok = !result.instructions.isEmpty();
    if (!result.ok) {
        result.error = QStringLiteral("empty_instruction_set");
    }
    return result;
}

TransactionInstruction SystemOperationBuilder::buildNonceAdvance(const QString& nonceAddress,
                                                                 const QString& authorityAddress) {
    return SystemInstruction::nonceAdvance(nonceAddress, authorityAddress);
}

TransactionInstruction SystemOperationBuilder::buildNonceWithdraw(const QString& nonceAddress,
                                                                  const QString& toAddress,
                                                                  const QString& authorityAddress,
                                                                  quint64 lamports) {
    return SystemInstruction::nonceWithdraw(nonceAddress, authorityAddress, toAddress, lamports);
}

// ── Stake operations ────────────────────────────────────────

StakeCreateAndDelegateResult
SystemOperationBuilder::buildStakeCreateAndDelegate(const StakeCreateAndDelegateInput& input) {
    StakeCreateAndDelegateResult result;
    if (input.walletAddress.isEmpty() || input.stakeAddress.isEmpty() ||
        input.voteAccount.isEmpty() || input.stakeLamports == 0 || input.rentLamports == 0) {
        result.error = QStringLiteral("invalid_input");
        return result;
    }

    result.instructions = StakeInstruction::createAndDelegate(
        input.walletAddress, input.stakeAddress, input.voteAccount, input.stakeLamports,
        input.rentLamports);
    result.ok = !result.instructions.isEmpty();
    if (!result.ok) {
        result.error = QStringLiteral("empty_instruction_set");
    }
    return result;
}

TransactionInstruction
SystemOperationBuilder::buildStakeDeactivate(const QString& stakeAddress,
                                             const QString& authorityAddress) {
    return StakeInstruction::deactivate(stakeAddress, authorityAddress);
}

TransactionInstruction SystemOperationBuilder::buildStakeWithdraw(const QString& stakeAddress,
                                                                  const QString& toAddress,
                                                                  const QString& authorityAddress,
                                                                  quint64 lamports) {
    return StakeInstruction::withdraw(stakeAddress, toAddress, authorityAddress, lamports);
}
