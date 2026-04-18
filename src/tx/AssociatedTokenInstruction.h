#ifndef ASSOCIATEDTOKENINSTRUCTION_H
#define ASSOCIATEDTOKENINSTRUCTION_H

#include "tx/ProgramIds.h"
#include "tx/TransactionInstruction.h"

namespace AssociatedTokenInstruction {

    // Derive the Associated Token Account address for owner + mint + token program.
    // Returns empty string on failure.
    QString deriveAddress(const QString& owner, const QString& mint,
                          const QString& tokenProgramId = SolanaPrograms::TokenProgram);

    // Create associated token account
    inline TransactionInstruction
    create(const QString& payer, const QString& associatedToken, const QString& owner,
           const QString& mint, const QString& tokenProgramId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::AssociatedTokenAccount;
        ix.accounts = {
            AccountMeta::writable(payer, true),
            AccountMeta::writable(associatedToken, false),
            AccountMeta::readonly(owner, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(SolanaPrograms::SystemProgram, false),
            AccountMeta::readonly(tokenProgramId, false),
        };
        return ix;
    }

    // CreateIdempotent (succeeds even if ATA already exists)
    inline TransactionInstruction
    createIdempotent(const QString& payer, const QString& associatedToken, const QString& owner,
                     const QString& mint,
                     const QString& tokenProgramId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::AssociatedTokenAccount;
        ix.accounts = {
            AccountMeta::writable(payer, true),
            AccountMeta::writable(associatedToken, false),
            AccountMeta::readonly(owner, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(SolanaPrograms::SystemProgram, false),
            AccountMeta::readonly(tokenProgramId, false),
        };
        ix.data = QByteArray(1, '\x01');
        return ix;
    }

} // namespace AssociatedTokenInstruction

#endif // ASSOCIATEDTOKENINSTRUCTION_H
