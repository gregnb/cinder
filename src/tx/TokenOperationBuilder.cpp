#include "tx/TokenOperationBuilder.h"

#include "tx/AssociatedTokenInstruction.h"
#include "tx/KnownTokens.h"
#include "tx/ProgramIds.h"
#include "tx/SystemInstruction.h"
#include "tx/Token2022Instruction.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TokenInstruction.h"
#include "tx/TokenMetadataInstruction.h"

TransferInstructionBuildResult
TokenOperationBuilder::buildTransfer(const TransferInstructionBuildInput& input) {
    TransferInstructionBuildResult result;

    if (input.walletAddress.isEmpty() || input.mint.isEmpty() || input.tokenProgram.isEmpty() ||
        input.recipients.isEmpty()) {
        result.error = "invalid_input";
        return result;
    }

    const bool isSol = (input.mint == WSOL_MINT);
    const int batchEnd = qMin(input.recipients.size(), static_cast<qsizetype>(input.maxRecipients));

    for (int i = 0; i < batchEnd; ++i) {
        const TransferInstructionRecipient& recipient = input.recipients[i];
        quint64 rawAmount = 0;

        if (isSol) {
            if (!TokenAmountCodec::toRaw(recipient.amount, 9, &rawAmount)) {
                result.error = recipient.address.left(12);
                result.instructions.clear();
                return result;
            }

            result.instructions.append(
                SystemInstruction::transfer(input.walletAddress, recipient.address, rawAmount));
            continue;
        }

        const QString recipientAta = AssociatedTokenInstruction::deriveAddress(
            recipient.address, input.mint, input.tokenProgram);
        if (recipientAta.isEmpty()) {
            result.error = recipient.address.left(12);
            result.instructions.clear();
            return result;
        }

        result.instructions.append(AssociatedTokenInstruction::createIdempotent(
            input.walletAddress, recipientAta, recipient.address, input.mint, input.tokenProgram));

        if (!TokenAmountCodec::toRaw(recipient.amount, input.decimals, &rawAmount)) {
            result.error = recipient.address.left(12);
            result.instructions.clear();
            return result;
        }

        if (SolanaPrograms::isToken2022(input.tokenProgram) && input.transferFeeBasisPoints > 0) {
            quint64 fee = rawAmount * input.transferFeeBasisPoints / 10000;
            if (fee > input.transferFeeMax) {
                fee = input.transferFeeMax;
            }
            result.instructions.append(Token2022Instruction::TransferFee::transferCheckedWithFee(
                input.sourceTokenAccount, input.mint, recipientAta, input.walletAddress, rawAmount,
                static_cast<quint8>(input.decimals), fee));
        } else {
            result.instructions.append(TokenInstruction::transferChecked(
                input.sourceTokenAccount, input.mint, recipientAta, input.walletAddress, rawAmount,
                static_cast<quint8>(input.decimals), input.tokenProgram));
        }
    }

    result.ok = true;
    return result;
}

CreateTokenInstructionBuildResult
TokenOperationBuilder::buildCreateToken(const CreateTokenInstructionBuildInput& input) {
    CreateTokenInstructionBuildResult result;

    if (input.walletAddress.isEmpty() || input.mintAddress.isEmpty() || input.name.isEmpty() ||
        input.symbol.isEmpty() || input.mintAccountSize == 0 || input.rentLamports == 0) {
        result.error = "invalid_input";
        return result;
    }

    // Token-2022 InitializeMint2 rejects accounts with trailing zeros after declared TLV
    // extensions. TokenMetadata uses auto-realloc, so createAccount.space must include only
    // fixed-size extensions. Rent lamports still covers the full final size (after realloc).
    using namespace Token2022AccountSize;
    quint64 createAccountSpace = kMintBaseWithExtensions + kTlvHeaderLen + kMetadataPointerDataLen;
    if (input.hasTransferFee) {
        createAccountSpace += kTlvHeaderLen + kTransferFeeDataLen;
    }
    if (input.hasNonTransferable) {
        createAccountSpace += kTlvHeaderLen + kNonTransferableDataLen;
    }
    if (input.hasMintClose) {
        createAccountSpace += kTlvHeaderLen + kMintCloseDataLen;
    }
    if (input.hasPermanentDelegate) {
        createAccountSpace += kTlvHeaderLen + kPermanentDelegateDataLen;
    }

    result.instructions.append(
        SystemInstruction::createAccount(input.walletAddress, input.mintAddress, input.rentLamports,
                                         createAccountSpace, SolanaPrograms::Token2022Program));

    result.instructions.append(Token2022Instruction::MetadataPointer::initialize(
        input.mintAddress, input.walletAddress, input.mintAddress));

    if (input.hasTransferFee) {
        result.instructions.append(Token2022Instruction::TransferFee::initializeTransferFeeConfig(
            input.mintAddress, input.walletAddress, input.walletAddress, input.feeBasisPoints,
            input.feeMaxRaw));
    }
    if (input.hasMintClose) {
        result.instructions.append(Token2022Instruction::initializeMintCloseAuthority(
            input.mintAddress, input.walletAddress));
    }
    if (input.hasNonTransferable) {
        result.instructions.append(
            Token2022Instruction::initializeNonTransferableMint(input.mintAddress));
    }
    if (input.hasPermanentDelegate) {
        result.instructions.append(Token2022Instruction::initializePermanentDelegate(
            input.mintAddress, input.walletAddress));
    }

    result.instructions.append(TokenInstruction::initializeMint2(
        input.mintAddress, static_cast<quint8>(input.decimals), input.walletAddress,
        input.freezeAuthority, SolanaPrograms::Token2022Program));

    result.instructions.append(TokenMetadataInstruction::initialize(
        input.mintAddress, input.walletAddress, input.mintAddress, input.walletAddress, input.name,
        input.symbol, input.uri));

    if (input.rawSupply > 0) {
        const QString walletAta = AssociatedTokenInstruction::deriveAddress(
            input.walletAddress, input.mintAddress, SolanaPrograms::Token2022Program);
        if (walletAta.isEmpty()) {
            result.error = "derive_wallet_ata_failed";
            result.instructions.clear();
            return result;
        }

        result.instructions.append(AssociatedTokenInstruction::createIdempotent(
            input.walletAddress, walletAta, input.walletAddress, input.mintAddress,
            SolanaPrograms::Token2022Program));
        result.instructions.append(TokenInstruction::mintTo(input.mintAddress, walletAta,
                                                            input.walletAddress, input.rawSupply,
                                                            SolanaPrograms::Token2022Program));
    }

    result.ok = true;
    return result;
}

MintInstructionBuildResult
TokenOperationBuilder::buildMint(const MintInstructionBuildInput& input) {
    MintInstructionBuildResult result;

    if (input.walletAddress.isEmpty() || input.mint.isEmpty() || input.tokenProgram.isEmpty() ||
        input.rawAmount == 0) {
        result.error = "invalid_input";
        return result;
    }

    const QString destinationAta = AssociatedTokenInstruction::deriveAddress(
        input.walletAddress, input.mint, input.tokenProgram);
    if (destinationAta.isEmpty()) {
        result.error = "derive_destination_failed";
        return result;
    }

    result.instructions.append(AssociatedTokenInstruction::createIdempotent(
        input.walletAddress, destinationAta, input.walletAddress, input.mint, input.tokenProgram));
    result.instructions.append(TokenInstruction::mintTo(
        input.mint, destinationAta, input.walletAddress, input.rawAmount, input.tokenProgram));
    result.ok = true;
    return result;
}

BurnInstructionBuildResult
TokenOperationBuilder::buildBurn(const BurnInstructionBuildInput& input) {
    BurnInstructionBuildResult result;

    if (input.walletAddress.isEmpty() || input.mint.isEmpty() || input.tokenProgram.isEmpty() ||
        input.sourceTokenAccount.isEmpty() || input.rawAmount == 0) {
        result.error = "invalid_input";
        return result;
    }

    result.instructions.append(TokenInstruction::burnChecked(input.sourceTokenAccount, input.mint,
                                                             input.walletAddress, input.rawAmount,
                                                             input.decimals, input.tokenProgram));
    result.ok = true;
    return result;
}
