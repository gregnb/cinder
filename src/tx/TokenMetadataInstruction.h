#ifndef TOKENMETADATAINSTRUCTION_H
#define TOKENMETADATAINSTRUCTION_H

#include "tx/ProgramIds.h"
#include "tx/Token2022Types.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TransactionInstruction.h"
#include <QByteArray>
#include <optional>

// SPL Token Metadata Interface — hash-discriminated instructions.
// These are sent to the Token-2022 program when the mint has a
// MetadataPointer extension pointing to itself.
//
// Discriminator = SHA-256("<interface>:<method>")[0..8]

namespace TokenMetadataInstruction {

    // ── Discriminators (verified against SHA-256) ─────────────

    inline const QByteArray DISC_INITIALIZE = QByteArray::fromHex("d2e11ea258b84d8d");
    inline const QByteArray DISC_UPDATE_FIELD = QByteArray::fromHex("dde9312db5cadcc8");
    inline const QByteArray DISC_REMOVE_KEY = QByteArray::fromHex("ea122038598d25b5");
    inline const QByteArray DISC_UPDATE_AUTHORITY = QByteArray::fromHex("d7e4a6e45464567b");
    inline const QByteArray DISC_EMIT = QByteArray::fromHex("faa6b4fa0d0cb846");

    // ── Instructions ──────────────────────────────────────────

    // Initialize: create metadata for a mint.
    // disc + Borsh{name: String, symbol: String, uri: String}
    inline TransactionInstruction initialize(const QString& metadata,
                                             const QString& updateAuthority, const QString& mint,
                                             const QString& mintAuthority, const QString& name,
                                             const QString& symbol, const QString& uri) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(metadata, false),
            AccountMeta::readonly(updateAuthority, false),
            AccountMeta::readonly(mint, false),
            AccountMeta::readonly(mintAuthority, true),
        };
        QByteArray data = DISC_INITIALIZE;
        data += TokenEncoding::encodeBorshString(name);
        data += TokenEncoding::encodeBorshString(symbol);
        data += TokenEncoding::encodeBorshString(uri);
        ix.data = data;
        return ix;
    }

    // UpdateField: update a standard field (Name, Symbol, Uri).
    // disc + Borsh{field: u8 variant, value: String}
    inline TransactionInstruction updateField(const QString& metadata,
                                              const QString& updateAuthority, quint8 fieldVariant,
                                              const QString& value) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(metadata, false),
            AccountMeta::readonly(updateAuthority, true),
        };
        QByteArray data = DISC_UPDATE_FIELD;
        data += TokenEncoding::encodeU8(fieldVariant);
        data += TokenEncoding::encodeBorshString(value);
        ix.data = data;
        return ix;
    }

    // UpdateField with custom key (variant 3).
    // disc + Borsh{field: Key(String), value: String}
    inline TransactionInstruction updateFieldCustomKey(const QString& metadata,
                                                       const QString& updateAuthority,
                                                       const QString& key, const QString& value) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(metadata, false),
            AccountMeta::readonly(updateAuthority, true),
        };
        QByteArray data = DISC_UPDATE_FIELD;
        data += TokenEncoding::encodeU8(3); // Key variant
        data += TokenEncoding::encodeBorshString(key);
        data += TokenEncoding::encodeBorshString(value);
        ix.data = data;
        return ix;
    }

    // RemoveKey: remove a custom metadata key.
    // disc + Borsh{idempotent: bool(u8), key: String}
    inline TransactionInstruction removeKey(const QString& metadata, const QString& updateAuthority,
                                            bool idempotent, const QString& key) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(metadata, false),
            AccountMeta::readonly(updateAuthority, true),
        };
        QByteArray data = DISC_REMOVE_KEY;
        data.append(idempotent ? '\x01' : '\x00');
        data += TokenEncoding::encodeBorshString(key);
        ix.data = data;
        return ix;
    }

    // UpdateAuthority: change or revoke the update authority.
    // disc + {newAuthority: OptionalNonZeroPubkey(32)}
    inline TransactionInstruction updateAuthority(const QString& metadata,
                                                  const QString& currentAuthority,
                                                  const QString& newAuthority) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::writable(metadata, false),
            AccountMeta::readonly(currentAuthority, true),
        };
        QByteArray data = DISC_UPDATE_AUTHORITY;
        data += TokenEncoding::encodeOptionalNonZeroPubkeyOpt(newAuthority);
        ix.data = data;
        return ix;
    }

    // Emit: log metadata via return data.
    // disc + Borsh{start: Option<u64>, end: Option<u64>}
    inline TransactionInstruction emitMetadata(const QString& metadata,
                                               std::optional<quint64> start = std::nullopt,
                                               std::optional<quint64> end = std::nullopt) {
        TransactionInstruction ix;
        ix.programId = SolanaPrograms::Token2022Program;
        ix.accounts = {
            AccountMeta::readonly(metadata, false),
        };
        QByteArray data = DISC_EMIT;
        data += TokenEncoding::encodeBorshOptionU64(start);
        data += TokenEncoding::encodeBorshOptionU64(end);
        ix.data = data;
        return ix;
    }

} // namespace TokenMetadataInstruction

#endif // TOKENMETADATAINSTRUCTION_H
