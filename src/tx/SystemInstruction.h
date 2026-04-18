#ifndef SYSTEMINSTRUCTION_H
#define SYSTEMINSTRUCTION_H

#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/TransactionInstruction.h"
#include <QByteArray>
#include <QList>
#include <QtEndian>

namespace SystemInstruction {

    inline QByteArray encodeU32(quint32 value) {
        QByteArray buf(4, '\0');
        qToLittleEndian(value, reinterpret_cast<uchar*>(buf.data()));
        return buf;
    }

    inline QByteArray encodeU64(quint64 value) {
        QByteArray buf(8, '\0');
        qToLittleEndian(value, reinterpret_cast<uchar*>(buf.data()));
        return buf;
    }

    // Transfer (index 2): SOL transfer
    inline TransactionInstruction transfer(const QString& from, const QString& to,
                                           quint64 lamports) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::SystemProgram;
        ix.accounts = {
            AccountMeta::writable(from, true),
            AccountMeta::writable(to, false),
        };
        ix.data = encodeU32(2) + encodeU64(lamports);
        return ix;
    }

    // CreateAccount (index 0)
    inline TransactionInstruction createAccount(const QString& from, const QString& newAccount,
                                                quint64 lamports, quint64 space,
                                                const QString& owner) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::SystemProgram;
        ix.accounts = {
            AccountMeta::writable(from, true),
            AccountMeta::writable(newAccount, true),
        };
        ix.data = encodeU32(0) + encodeU64(lamports) + encodeU64(space) + Base58::decode(owner);
        return ix;
    }

    // AdvanceNonceAccount (index 4)
    inline TransactionInstruction nonceAdvance(const QString& noncePubkey,
                                               const QString& authority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::SystemProgram;
        ix.accounts = {
            AccountMeta::writable(noncePubkey, false),
            AccountMeta::readonly(SolanaPrograms::RecentBlockhashesSysvar, false),
            AccountMeta::readonly(authority, true),
        };
        ix.data = encodeU32(4);
        return ix;
    }

    // WithdrawNonceAccount (index 5)
    inline TransactionInstruction nonceWithdraw(const QString& noncePubkey,
                                                const QString& authority, const QString& to,
                                                quint64 lamports) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::SystemProgram;
        ix.accounts = {
            AccountMeta::writable(noncePubkey, false),
            AccountMeta::writable(to, false),
            AccountMeta::readonly(SolanaPrograms::RecentBlockhashesSysvar, false),
            AccountMeta::readonly(SolanaPrograms::RentSysvar, false),
            AccountMeta::readonly(authority, true),
        };
        ix.data = encodeU32(5) + encodeU64(lamports);
        return ix;
    }

    // InitializeNonceAccount (index 6)
    inline TransactionInstruction nonceInitialize(const QString& noncePubkey,
                                                  const QString& authority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::SystemProgram;
        ix.accounts = {
            AccountMeta::writable(noncePubkey, false),
            AccountMeta::readonly(SolanaPrograms::RecentBlockhashesSysvar, false),
            AccountMeta::readonly(SolanaPrograms::RentSysvar, false),
        };
        ix.data = encodeU32(6) + Base58::decode(authority);
        return ix;
    }

    // Convenience: returns the two instructions needed to create a nonce account
    inline QList<TransactionInstruction> createNonceAccount(const QString& from,
                                                            const QString& noncePubkey,
                                                            const QString& authority,
                                                            quint64 lamports) {
        return {
            createAccount(from, noncePubkey, lamports, 80, SolanaPrograms::SystemProgram),
            nonceInitialize(noncePubkey, authority),
        };
    }

} // namespace SystemInstruction

#endif // SYSTEMINSTRUCTION_H
