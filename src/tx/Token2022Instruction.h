#ifndef TOKEN2022INSTRUCTION_H
#define TOKEN2022INSTRUCTION_H

#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/Token2022Types.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionInstruction.h"
#include <QByteArray>
#include <QList>
#include <QtEndian>

namespace Token2022Instruction {

    // ══════════════════════════════════════════════════════════
    // Standalone Token-2022 instructions (no sub-instructions)
    // ══════════════════════════════════════════════════════════

    // InitializeMintCloseAuthority (index 25/0x19)
    // Must be called before InitializeMint/InitializeMint2.
    inline TransactionInstruction
    initializeMintCloseAuthority(const QString& mint, const QString& closeAuthority = QString()) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(mint, false),
        };
        QByteArray data(1, '\x19');
        data += TokenEncoding::encodeInstructionPubkeyOptionOpt(closeAuthority);
        ix.data = data;
        return ix;
    }

    // InitializeImmutableOwner (index 22/0x16)
    inline TransactionInstruction initializeImmutableOwner(const QString& account) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(account, false),
        };
        ix.data = QByteArray(1, '\x16');
        return ix;
    }

    // InitializeNonTransferableMint (index 32/0x20)
    // Must be called before InitializeMint/InitializeMint2.
    inline TransactionInstruction initializeNonTransferableMint(const QString& mint) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(mint, false),
        };
        ix.data = QByteArray(1, '\x20');
        return ix;
    }

    // InitializePermanentDelegate (index 35/0x23)
    // Must be called before InitializeMint/InitializeMint2.
    inline TransactionInstruction initializePermanentDelegate(const QString& mint,
                                                              const QString& delegate) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(mint, false),
        };
        ix.data = QByteArray(1, '\x23') + Base58::decode(delegate);
        return ix;
    }

    // Reallocate (index 29/0x1D)
    // Grows an account to accommodate new extensions.
    inline TransactionInstruction reallocate(const QString& account, const QString& payer,
                                             const QString& owner,
                                             const QList<quint16>& extensionTypes) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(account, false),
            AccountMeta::writable(payer, true),
            AccountMeta::readonly(SolanaPrograms::SystemProgram, false),
            AccountMeta::readonly(owner, true),
        };
        QByteArray data(1, '\x1D');
        for (quint16 ext : extensionTypes)
            data += TokenEncoding::encodeU16(ext);
        ix.data = data;
        return ix;
    }

    // WithdrawExcessLamports (index 38/0x26)
    inline TransactionInstruction withdrawExcessLamports(const QString& source,
                                                         const QString& destination,
                                                         const QString& authority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(source, false),
            AccountMeta::writable(destination, false),
            AccountMeta::readonly(authority, true),
        };
        ix.data = QByteArray(1, '\x26');
        return ix;
    }

    // ══════════════════════════════════════════════════════════
    // Transfer Fee Extension (tag 0x1A)
    // ══════════════════════════════════════════════════════════

    namespace TransferFee {

        // Sub 0: InitializeTransferFeeConfig
        inline TransactionInstruction initializeTransferFeeConfig(const QString& mint,
                                                                  const QString& configAuthority,
                                                                  const QString& withdrawAuthority,
                                                                  quint16 basisPoints,
                                                                  quint64 maxFee) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x1A');
            data.append('\x00');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(configAuthority);
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(withdrawAuthority);
            data += TokenEncoding::encodeU16(basisPoints);
            data += TokenEncoding::encodeU64(maxFee);
            ix.data = data;
            return ix;
        }

        // Sub 1: TransferCheckedWithFee
        inline TransactionInstruction transferCheckedWithFee(const QString& source,
                                                             const QString& mint,
                                                             const QString& destination,
                                                             const QString& owner, quint64 amount,
                                                             quint8 decimals, quint64 fee) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(source, false),
                AccountMeta::readonly(mint, false),
                AccountMeta::writable(destination, false),
                AccountMeta::readonly(owner, true),
            };
            QByteArray data;
            data.append('\x1A');
            data.append('\x01');
            data += TokenEncoding::encodeU64(amount);
            data.append(static_cast<char>(decimals));
            data += TokenEncoding::encodeU64(fee);
            ix.data = data;
            return ix;
        }

        // Sub 2: WithdrawWithheldTokensFromMint
        inline TransactionInstruction withdrawWithheldTokensFromMint(const QString& mint,
                                                                     const QString& destination,
                                                                     const QString& authority) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::writable(destination, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x1A');
            data.append('\x02');
            ix.data = data;
            return ix;
        }

        // Sub 3: WithdrawWithheldTokensFromAccounts
        inline TransactionInstruction
        withdrawWithheldTokensFromAccounts(const QString& mint, const QString& destination,
                                           const QString& authority,
                                           const QList<QString>& sourceAccounts) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::readonly(mint, false),
                AccountMeta::writable(destination, false),
                AccountMeta::readonly(authority, true),
            };
            for (const QString& src : sourceAccounts)
                ix.accounts.append(AccountMeta::writable(src, false));
            QByteArray data;
            data.append('\x1A');
            data.append('\x03');
            data.append(static_cast<char>(sourceAccounts.size()));
            ix.data = data;
            return ix;
        }

        // Sub 4: HarvestWithheldTokensToMint
        inline TransactionInstruction
        harvestWithheldTokensToMint(const QString& mint, const QList<QString>& sourceAccounts) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            for (const QString& src : sourceAccounts)
                ix.accounts.append(AccountMeta::writable(src, false));
            QByteArray data;
            data.append('\x1A');
            data.append('\x04');
            ix.data = data;
            return ix;
        }

        // Sub 5: SetTransferFee
        inline TransactionInstruction setTransferFee(const QString& mint, const QString& authority,
                                                     quint16 basisPoints, quint64 maxFee) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x1A');
            data.append('\x05');
            data += TokenEncoding::encodeU16(basisPoints);
            data += TokenEncoding::encodeU64(maxFee);
            ix.data = data;
            return ix;
        }

    } // namespace TransferFee

    // ══════════════════════════════════════════════════════════
    // Default Account State Extension (tag 0x1C)
    // ══════════════════════════════════════════════════════════

    namespace DefaultAccountState {

        // Sub 0: Initialize
        inline TransactionInstruction initialize(const QString& mint, quint8 accountState) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x1C');
            data.append('\x00');
            data.append(static_cast<char>(accountState));
            ix.data = data;
            return ix;
        }

        // Sub 1: Update
        inline TransactionInstruction update(const QString& mint, const QString& authority,
                                             quint8 accountState) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x1C');
            data.append('\x01');
            data.append(static_cast<char>(accountState));
            ix.data = data;
            return ix;
        }

    } // namespace DefaultAccountState

    // ══════════════════════════════════════════════════════════
    // Memo Transfer Extension (tag 0x1E)
    // ══════════════════════════════════════════════════════════

    namespace MemoTransfer {

        // Sub 0: Enable
        inline TransactionInstruction enable(const QString& account, const QString& owner) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(account, false),
                AccountMeta::readonly(owner, true),
            };
            QByteArray data;
            data.append('\x1E');
            data.append('\x00');
            ix.data = data;
            return ix;
        }

        // Sub 1: Disable
        inline TransactionInstruction disable(const QString& account, const QString& owner) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(account, false),
                AccountMeta::readonly(owner, true),
            };
            QByteArray data;
            data.append('\x1E');
            data.append('\x01');
            ix.data = data;
            return ix;
        }

    } // namespace MemoTransfer

    // ══════════════════════════════════════════════════════════
    // Interest Bearing Mint Extension (tag 0x21)
    // ══════════════════════════════════════════════════════════

    namespace InterestBearingMint {

        // Sub 0: Initialize
        inline TransactionInstruction initialize(const QString& mint, const QString& rateAuthority,
                                                 qint16 rate) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x21');
            data.append('\x00');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(rateAuthority);
            data += TokenEncoding::encodeI16(rate);
            ix.data = data;
            return ix;
        }

        // Sub 1: UpdateRate
        inline TransactionInstruction updateRate(const QString& mint, const QString& rateAuthority,
                                                 qint16 rate) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(rateAuthority, true),
            };
            QByteArray data;
            data.append('\x21');
            data.append('\x01');
            data += TokenEncoding::encodeI16(rate);
            ix.data = data;
            return ix;
        }

    } // namespace InterestBearingMint

    // ══════════════════════════════════════════════════════════
    // CPI Guard Extension (tag 0x22)
    // ══════════════════════════════════════════════════════════

    namespace CpiGuard {

        // Sub 0: Enable
        inline TransactionInstruction enable(const QString& account, const QString& owner) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(account, false),
                AccountMeta::readonly(owner, true),
            };
            QByteArray data;
            data.append('\x22');
            data.append('\x00');
            ix.data = data;
            return ix;
        }

        // Sub 1: Disable
        inline TransactionInstruction disable(const QString& account, const QString& owner) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(account, false),
                AccountMeta::readonly(owner, true),
            };
            QByteArray data;
            data.append('\x22');
            data.append('\x01');
            ix.data = data;
            return ix;
        }

    } // namespace CpiGuard

    // ══════════════════════════════════════════════════════════
    // Transfer Hook Extension (tag 0x24)
    // ══════════════════════════════════════════════════════════

    namespace TransferHook {

        // Sub 0: Initialize
        inline TransactionInstruction initialize(const QString& mint, const QString& authority,
                                                 const QString& hookProgramId) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x24');
            data.append('\x00');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(authority);
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(hookProgramId);
            ix.data = data;
            return ix;
        }

        // Sub 1: Update
        inline TransactionInstruction update(const QString& mint, const QString& authority,
                                             const QString& hookProgramId) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x24');
            data.append('\x01');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(hookProgramId);
            ix.data = data;
            return ix;
        }

    } // namespace TransferHook

    // ══════════════════════════════════════════════════════════
    // Metadata Pointer Extension (tag 0x27)
    // ══════════════════════════════════════════════════════════

    namespace MetadataPointer {

        // Sub 0: Initialize
        inline TransactionInstruction initialize(const QString& mint, const QString& authority,
                                                 const QString& metadataAddress) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x27');
            data.append('\x00');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(authority);
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(metadataAddress);
            ix.data = data;
            return ix;
        }

        // Sub 1: Update
        inline TransactionInstruction update(const QString& mint, const QString& authority,
                                             const QString& metadataAddress) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x27');
            data.append('\x01');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(metadataAddress);
            ix.data = data;
            return ix;
        }

    } // namespace MetadataPointer

    // ══════════════════════════════════════════════════════════
    // Group Pointer Extension (tag 0x28)
    // ══════════════════════════════════════════════════════════

    namespace GroupPointer {

        // Sub 0: Initialize
        inline TransactionInstruction initialize(const QString& mint, const QString& authority,
                                                 const QString& groupAddress) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x28');
            data.append('\x00');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(authority);
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(groupAddress);
            ix.data = data;
            return ix;
        }

        // Sub 1: Update
        inline TransactionInstruction update(const QString& mint, const QString& authority,
                                             const QString& groupAddress) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x28');
            data.append('\x01');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(groupAddress);
            ix.data = data;
            return ix;
        }

    } // namespace GroupPointer

    // ══════════════════════════════════════════════════════════
    // Group Member Pointer Extension (tag 0x29)
    // ══════════════════════════════════════════════════════════

    namespace GroupMemberPointer {

        // Sub 0: Initialize
        inline TransactionInstruction initialize(const QString& mint, const QString& authority,
                                                 const QString& memberAddress) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
            };
            QByteArray data;
            data.append('\x29');
            data.append('\x00');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(authority);
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(memberAddress);
            ix.data = data;
            return ix;
        }

        // Sub 1: Update
        inline TransactionInstruction update(const QString& mint, const QString& authority,
                                             const QString& memberAddress) {
            TransactionInstruction ix;
            ix.programId = SolanaPrograms::Token2022Program;
            ix.accounts = {
                AccountMeta::writable(mint, false),
                AccountMeta::readonly(authority, true),
            };
            QByteArray data;
            data.append('\x29');
            data.append('\x01');
            data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(memberAddress);
            ix.data = data;
            return ix;
        }

    } // namespace GroupMemberPointer

} // namespace Token2022Instruction

#endif // TOKEN2022INSTRUCTION_H
