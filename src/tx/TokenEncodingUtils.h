#ifndef TOKENENCODINGUTILS_H
#define TOKENENCODINGUTILS_H

#include "tx/Base58.h"
#include <QByteArray>
#include <QString>
#include <QtEndian>
#include <cmath>
#include <limits>
#include <optional>

namespace TokenEncoding {

    // ── Little-endian integer encoding ────────────────────────

    inline QByteArray encodeU8(quint8 value) { return QByteArray(1, static_cast<char>(value)); }

    inline QByteArray encodeU16(quint16 value) {
        QByteArray buf(2, '\0');
        qToLittleEndian(value, reinterpret_cast<uchar*>(buf.data()));
        return buf;
    }

    inline QByteArray encodeI16(qint16 value) {
        QByteArray buf(2, '\0');
        qToLittleEndian(static_cast<quint16>(value), reinterpret_cast<uchar*>(buf.data()));
        return buf;
    }

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

    // ── COption<Pubkey> — 4-byte tag (LE u32) ──────────────────
    // Used by: on-chain account STATE (Mint/Account struct Pack trait).
    // NOT used by instruction data — see encodeInstructionPubkeyOption below.
    // None = [0x00,0x00,0x00,0x00]          (4 bytes)
    // Some = [0x01,0x00,0x00,0x00][key:32]  (36 bytes)

    inline QByteArray encodeCOptionPubkeyNone() { return QByteArray(4, '\0'); }

    inline QByteArray encodeCOptionPubkey(const QString& pubkey) {
        QByteArray tag(4, '\0');
        tag[0] = 0x01;
        return tag + Base58::decode(pubkey);
    }

    // ── Instruction pubkey option — 1-byte tag ──────────────────
    // Used by: SPL Token instruction data (pack_pubkey_option/unpack_pubkey_option).
    // InitializeMint, InitializeMint2, SetAuthority, MintCloseAuthority, etc.
    // None = [0x00]          (1 byte)
    // Some = [0x01][key:32]  (33 bytes)

    inline QByteArray encodeInstructionPubkeyOptionNone() { return QByteArray(1, '\0'); }

    inline QByteArray encodeInstructionPubkeyOption(const QString& pubkey) {
        QByteArray tag(1, '\x01');
        return tag + Base58::decode(pubkey);
    }

    inline QByteArray encodeInstructionPubkeyOptionOpt(const QString& pubkey) {
        return pubkey.isEmpty() ? encodeInstructionPubkeyOptionNone()
                                : encodeInstructionPubkeyOption(pubkey);
    }

    // ── OptionalNonZeroPubkey — fixed 32 bytes ────────────────
    // Used by: all extension init/update instructions, metadata, groups
    // None = 32 zero bytes
    // Some = 32-byte pubkey

    inline QByteArray encodeOptionalNonZeroPubkeyNone() { return QByteArray(32, '\0'); }

    inline QByteArray encodeOptionalNonZeroPubkey(const QString& pubkey) {
        return Base58::decode(pubkey);
    }

    // Helper: encode optional pubkey (empty string → None)
    inline QByteArray encodeOptionalNonZeroPubkeyOpt(const QString& pubkey) {
        return pubkey.isEmpty() ? encodeOptionalNonZeroPubkeyNone()
                                : encodeOptionalNonZeroPubkey(pubkey);
    }

    // ── Borsh encoding ────────────────────────────────────────

    // Borsh String: [u32 LE length][UTF-8 bytes]
    inline QByteArray encodeBorshString(const QString& str) {
        QByteArray utf8 = str.toUtf8();
        return encodeU32(static_cast<quint32>(utf8.size())) + utf8;
    }

    // Borsh Option<u64>: [0x00] = None, [0x01][u64 LE] = Some
    inline QByteArray encodeBorshOptionU64(std::optional<quint64> value) {
        if (!value.has_value())
            return QByteArray(1, '\0');
        return QByteArray(1, '\x01') + encodeU64(value.value());
    }

} // namespace TokenEncoding

namespace TokenAmountCodec {

    inline bool toRaw(double amount, int decimals, quint64* rawAmount) {
        if (!rawAmount || !std::isfinite(amount) || amount <= 0.0 || decimals < 0) {
            return false;
        }

        const long double scale = std::pow(10.0L, static_cast<long double>(decimals));
        const long double scaled = static_cast<long double>(amount) * scale;
        if (!std::isfinite(static_cast<double>(scaled)) || scaled < 0.0L ||
            scaled > static_cast<long double>(std::numeric_limits<quint64>::max())) {
            return false;
        }

        *rawAmount = static_cast<quint64>(scaled);
        return true;
    }

} // namespace TokenAmountCodec

namespace SolAmount {
    inline bool toLamports(double sol, quint64* lamports) {
        return TokenAmountCodec::toRaw(sol, 9, lamports);
    }
} // namespace SolAmount

// Token-2022 account size constants for TLV extensions
namespace Token2022AccountSize {
    // [Mint:82][Padding:83][AccountType:1][TLV extensions...]
    constexpr quint64 kMintBaseWithExtensions = 166;  // max(Mint::LEN=82, Account::LEN=165) + 1
    constexpr quint64 kTlvHeaderLen = 4;              // 2 type + 2 length per TLV entry
    constexpr quint64 kMetadataPointerDataLen = 64;   // authority(32) + metadataAddress(32)
    constexpr quint64 kTransferFeeDataLen = 108;      // 2 authorities + withheld + 2 TransferFee
    constexpr quint64 kNonTransferableDataLen = 0;    // empty — presence alone is the flag
    constexpr quint64 kMintCloseDataLen = 32;         // close_authority pubkey
    constexpr quint64 kPermanentDelegateDataLen = 32; // delegate pubkey
    constexpr quint64 kPubkeyLen = 32;
    constexpr quint64 kBorshStringLenPrefix = 4;
    constexpr quint64 kMetadataAdditionalKvVecLen = 4;
} // namespace Token2022AccountSize

#endif // TOKENENCODINGUTILS_H
