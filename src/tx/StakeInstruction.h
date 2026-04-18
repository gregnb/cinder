#ifndef STAKEINSTRUCTION_H
#define STAKEINSTRUCTION_H

#include <QByteArray>
#include <QList>
#include <QtEndian>

#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/TransactionInstruction.h"

namespace StakeInstruction {

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

    // Initialize (index 0)
    // Authorized: staker(32) + withdrawer(32)
    // Lockup: unix_timestamp(i64=0) + epoch(u64=0) + custodian(pubkey=zeros) = 48 zero bytes
    inline TransactionInstruction initialize(const QString& stakeAccount, const QString& staker,
                                             const QString& withdrawer) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::StakeProgram;
        ix.accounts = {
            AccountMeta::writable(stakeAccount, false),
            AccountMeta::readonly(SolanaPrograms::RentSysvar, false),
        };
        ix.data = encodeU32(0) + Base58::decode(staker) + Base58::decode(withdrawer) +
                  QByteArray(48, '\0');
        return ix;
    }

    // Delegate (index 2)
    inline TransactionInstruction delegate(const QString& stakeAccount, const QString& voteAccount,
                                           const QString& authority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::StakeProgram;
        ix.accounts = {
            AccountMeta::writable(stakeAccount, false),
            AccountMeta::readonly(voteAccount, false),
            AccountMeta::readonly(SolanaPrograms::ClockSysvar, false),
            AccountMeta::readonly(SolanaPrograms::StakeHistorySysvar, false),
            AccountMeta::readonly(SolanaPrograms::StakeConfigAddress, false),
            AccountMeta::readonly(authority, true),
        };
        ix.data = encodeU32(2);
        return ix;
    }

    // Withdraw (index 4)
    inline TransactionInstruction withdraw(const QString& stakeAccount, const QString& to,
                                           const QString& authority, quint64 lamports) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::StakeProgram;
        ix.accounts = {
            AccountMeta::writable(stakeAccount, false),
            AccountMeta::writable(to, false),
            AccountMeta::readonly(SolanaPrograms::ClockSysvar, false),
            AccountMeta::readonly(SolanaPrograms::StakeHistorySysvar, false),
            AccountMeta::readonly(authority, true),
        };
        ix.data = encodeU32(4) + encodeU64(lamports);
        return ix;
    }

    // Deactivate (index 5)
    inline TransactionInstruction deactivate(const QString& stakeAccount,
                                             const QString& authority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::StakeProgram;
        ix.accounts = {
            AccountMeta::writable(stakeAccount, false),
            AccountMeta::readonly(SolanaPrograms::ClockSysvar, false),
            AccountMeta::readonly(authority, true),
        };
        ix.data = encodeU32(5);
        return ix;
    }

    // Convenience: Create + Initialize + Delegate in one go.
    // Returns 3 instructions for a single transaction.
    inline QList<TransactionInstruction> createAndDelegate(const QString& from,
                                                           const QString& stakeAccount,
                                                           const QString& voteAccount,
                                                           quint64 lamports, quint64 rentExempt) {
        // Step 1: SystemInstruction::createAccount for 200-byte stake account
        TransactionInstruction createIx;
        createIx.programId = SolanaPrograms::SystemProgram;
        createIx.accounts = {
            AccountMeta::writable(from, true),
            AccountMeta::writable(stakeAccount, true),
        };
        createIx.data = encodeU32(0) + encodeU64(lamports + rentExempt) + encodeU64(200) +
                        Base58::decode(SolanaPrograms::StakeProgram);

        return {
            createIx,
            initialize(stakeAccount, from, from),
            delegate(stakeAccount, voteAccount, from),
        };
    }

} // namespace StakeInstruction

#endif // STAKEINSTRUCTION_H
