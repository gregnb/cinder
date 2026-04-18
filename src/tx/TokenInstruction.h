#ifndef TOKENINSTRUCTION_H
#define TOKENINSTRUCTION_H

#include "tx/ProgramIds.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionInstruction.h"
#include <QByteArray>
#include <QtEndian>

namespace TokenInstruction {

    inline QByteArray encodeU64(quint64 value) {
        QByteArray buf(8, '\0');
        qToLittleEndian(value, reinterpret_cast<uchar*>(buf.data()));
        return buf;
    }

    // Transfer (index 3)
    inline TransactionInstruction
    transfer(const QString& source, const QString& destination, const QString& owner,
             quint64 amount, const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(source, false),
            AccountMeta::writable(destination, false),
            AccountMeta::readonly(owner, true),
        };
        ix.data = QByteArray(1, '\x03') + encodeU64(amount);
        return ix;
    }

    // TransferChecked (index 12)
    inline TransactionInstruction
    transferChecked(const QString& source, const QString& mint, const QString& destination,
                    const QString& owner, quint64 amount, uint8_t decimals,
                    const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(source, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::writable(destination, false),
            AccountMeta::readonly(owner, true),
        };
        QByteArray data(1, '\x0C');
        data += encodeU64(amount);
        data.append(static_cast<char>(decimals));
        ix.data = data;
        return ix;
    }

    // Approve (index 4)
    inline TransactionInstruction approve(const QString& source, const QString& delegate,
                                          const QString& owner, quint64 amount,
                                          const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(source, false),
            AccountMeta::readonly(delegate, false),
            AccountMeta::readonly(owner, true),
        };
        ix.data = QByteArray(1, '\x04') + encodeU64(amount);
        return ix;
    }

    // Burn (index 8)
    inline TransactionInstruction burn(const QString& account, const QString& mint,
                                       const QString& owner, quint64 amount,
                                       const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::writable(mint, false),
            AccountMeta::readonly(owner, true),
        };
        ix.data = QByteArray(1, '\x08') + encodeU64(amount);
        return ix;
    }

    // CloseAccount (index 9)
    inline TransactionInstruction
    closeAccount(const QString& account, const QString& destination, const QString& owner,
                 const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::writable(destination, false),
            AccountMeta::readonly(owner, true),
        };
        ix.data = QByteArray(1, '\x09');
        return ix;
    }

    // InitializeAccount (index 1)
    inline TransactionInstruction
    initializeAccount(const QString& account, const QString& mint, const QString& owner,
                      const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(owner, false),
            AccountMeta::readonly(SolanaPrograms::RentSysvar, false),
        };
        ix.data = QByteArray(1, '\x01');
        return ix;
    }

    // Revoke (index 5)
    inline TransactionInstruction revoke(const QString& source, const QString& owner,
                                         const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(source, false),
            AccountMeta::readonly(owner, true),
        };
        ix.data = QByteArray(1, '\x05');
        return ix;
    }

    // SetAuthority (index 6)
    inline TransactionInstruction
    setAuthority(const QString& account, const QString& currentAuthority, quint8 authorityType,
                 const QString& newAuthority = QString(),
                 const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::readonly(currentAuthority, true),
        };
        QByteArray data(1, '\x06');
        data.append(static_cast<char>(authorityType));
        data += TokenEncoding::encodeInstructionPubkeyOptionOpt(newAuthority);
        ix.data = data;
        return ix;
    }

    // MintTo (index 7)
    inline TransactionInstruction mintTo(const QString& mint, const QString& destination,
                                         const QString& mintAuthority, quint64 amount,
                                         const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(mint, false),
            AccountMeta::writable(destination, false),
            AccountMeta::readonly(mintAuthority, true),
        };
        ix.data = QByteArray(1, '\x07') + encodeU64(amount);
        return ix;
    }

    // FreezeAccount (index 10)
    inline TransactionInstruction
    freezeAccount(const QString& account, const QString& mint, const QString& freezeAuthority,
                  const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(freezeAuthority, true),
        };
        ix.data = QByteArray(1, '\x0A');
        return ix;
    }

    // ThawAccount (index 11)
    inline TransactionInstruction
    thawAccount(const QString& account, const QString& mint, const QString& freezeAuthority,
                const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(freezeAuthority, true),
        };
        ix.data = QByteArray(1, '\x0B');
        return ix;
    }

    // ApproveChecked (index 13)
    inline TransactionInstruction
    approveChecked(const QString& source, const QString& mint, const QString& delegate,
                   const QString& owner, quint64 amount, quint8 decimals,
                   const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(source, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(delegate, false),
            AccountMeta::readonly(owner, true),
        };
        QByteArray data(1, '\x0D');
        data += encodeU64(amount);
        data.append(static_cast<char>(decimals));
        ix.data = data;
        return ix;
    }

    // MintToChecked (index 14)
    inline TransactionInstruction
    mintToChecked(const QString& mint, const QString& destination, const QString& mintAuthority,
                  quint64 amount, quint8 decimals,
                  const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(mint, false),
            AccountMeta::writable(destination, false),
            AccountMeta::readonly(mintAuthority, true),
        };
        QByteArray data(1, '\x0E');
        data += encodeU64(amount);
        data.append(static_cast<char>(decimals));
        ix.data = data;
        return ix;
    }

    // BurnChecked (index 15)
    inline TransactionInstruction
    burnChecked(const QString& account, const QString& mint, const QString& owner, quint64 amount,
                quint8 decimals, const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::writable(mint, false),
            AccountMeta::readonly(owner, true),
        };
        QByteArray data(1, '\x0F');
        data += encodeU64(amount);
        data.append(static_cast<char>(decimals));
        ix.data = data;
        return ix;
    }

    // SyncNative (index 17)
    inline TransactionInstruction
    syncNative(const QString& nativeAccount,
               const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(nativeAccount, false),
        };
        ix.data = QByteArray(1, '\x11');
        return ix;
    }

    // InitializeAccount3 (index 18) — no rent sysvar needed
    inline TransactionInstruction
    initializeAccount3(const QString& account, const QString& mint, const QString& owner,
                       const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::readonly(mint, false),
        };
        ix.data = QByteArray(1, '\x12') + Base58::decode(owner);
        return ix;
    }

    // InitializeMint2 (index 20) — no rent sysvar needed
    inline TransactionInstruction
    initializeMint2(const QString& mint, quint8 decimals, const QString& mintAuthority,
                    const QString& freezeAuthority = QString(),
                    const QString& programId = SolanaPrograms::TokenProgram) {
        TransactionInstruction ix;
        ix.programId = programId;
        ix.accounts = {
            AccountMeta::writable(mint, false),
        };
        QByteArray data(1, '\x14');
        data.append(static_cast<char>(decimals));
        data += Base58::decode(mintAuthority);
        data += TokenEncoding::encodeInstructionPubkeyOptionOpt(freezeAuthority);
        ix.data = data;
        return ix;
    }

} // namespace TokenInstruction

#endif // TOKENINSTRUCTION_H
