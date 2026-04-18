#ifndef TOKENGROUPINSTRUCTION_H
#define TOKENGROUPINSTRUCTION_H

#include "tx/ProgramIds.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionInstruction.h"
#include <QByteArray>

// SPL Token Group Interface — hash-discriminated instructions.
// Sent to the Token-2022 program when the mint has a GroupPointer extension.
//
// Discriminator = SHA-256("spl_token_group_interface:<method>")[0..8]

namespace TokenGroupInstruction {

    // ── Discriminators (verified against SHA-256) ─────────────

    inline const QByteArray DISC_INITIALIZE_GROUP = QByteArray::fromHex("79716c2736330004");
    inline const QByteArray DISC_UPDATE_MAX_SIZE = QByteArray::fromHex("6c25ab8ff81e126e");
    inline const QByteArray DISC_UPDATE_AUTHORITY = QByteArray::fromHex("a1695801edddd8cb");
    inline const QByteArray DISC_INITIALIZE_MEMBER = QByteArray::fromHex("9820deb0dfed7486");

    // ── Instructions ──────────────────────────────────────────

    // InitializeGroup: create a token group.
    // disc + {updateAuthority: OptionalNonZeroPubkey(32), maxSize: u64 LE}
    inline TransactionInstruction initializeGroup(const QString& group, const QString& mint,
                                                  const QString& mintAuthority,
                                                  const QString& updateAuthority, quint64 maxSize) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(group, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(mintAuthority, true),
        };
        QByteArray data = DISC_INITIALIZE_GROUP;
        data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(updateAuthority);
        data += TokenEncoding::encodeU64(maxSize);
        ix.data = data;
        return ix;
    }

    // UpdateGroupMaxSize: change the maximum group size.
    // disc + {maxSize: u64 LE}
    inline TransactionInstruction
    updateGroupMaxSize(const QString& group, const QString& updateAuthority, quint64 maxSize) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(group, false),
            AccountMeta::readonly(updateAuthority, true),
        };
        QByteArray data = DISC_UPDATE_MAX_SIZE;
        data += TokenEncoding::encodeU64(maxSize);
        ix.data = data;
        return ix;
    }

    // UpdateGroupAuthority: change or revoke the group update authority.
    // disc + {newAuthority: OptionalNonZeroPubkey(32)}
    inline TransactionInstruction updateGroupAuthority(const QString& group,
                                                       const QString& currentAuthority,
                                                       const QString& newAuthority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(group, false),
            AccountMeta::readonly(currentAuthority, true),
        };
        QByteArray data = DISC_UPDATE_AUTHORITY;
        data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(newAuthority);
        ix.data = data;
        return ix;
    }

    // InitializeMember: add a member to a group.
    // disc only (no additional data)
    inline TransactionInstruction initializeMember(const QString& member, const QString& memberMint,
                                                   const QString& memberMintAuthority,
                                                   const QString& group,
                                                   const QString& groupUpdateAuthority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(member, false),
            AccountMeta::readonly(memberMint, false),
            AccountMeta::readonly(memberMintAuthority, true),
            AccountMeta::writable(group, false),
            AccountMeta::readonly(groupUpdateAuthority, true),
        };
        ix.data = DISC_INITIALIZE_MEMBER;
        return ix;
    }

} // namespace TokenGroupInstruction

#endif // TOKENGROUPINSTRUCTION_H
