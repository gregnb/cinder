#ifndef TOKEN2022METADATA_H
#define TOKEN2022METADATA_H

#include <QByteArray>
#include <QString>
#include <cstdint>

// Parses Token-2022 embedded metadata from a mint account's TLV extensions.
//
// Token-2022 mint layout:
//   82 bytes: base Mint data
//   83 bytes: zero padding
//   1 byte:  account type (at offset 165, must be 1 = Mint)
//   N bytes: TLV extensions (type u16 LE | length u16 LE | data)
//
// Extension type 19 = TokenMetadata:
//   32 bytes: update_authority
//   32 bytes: mint
//   4 bytes + N: name (u32 LE length + UTF-8)
//   4 bytes + N: symbol
//   4 bytes + N: uri

struct Token2022Metadata {
    QString name;
    QString symbol;
    QString uri;
    bool valid = false;

    static Token2022Metadata fromMintAccountData(const QByteArray& data) {
        Token2022Metadata result;

        // Minimum: 166 bytes (82 base + 83 padding + 1 account type) + 4 (first TLV header)
        if (data.size() < 170) {
            return result;
        }

        const auto* d = reinterpret_cast<const uint8_t*>(data.constData());

        // Check account type at offset 165
        if (d[165] != 1) {
            return result; // Not a Mint account
        }

        // Scan TLV extensions starting at offset 166
        int offset = 166;
        while (offset + 4 <= data.size()) {
            uint16_t extType =
                static_cast<uint16_t>(d[offset]) | (static_cast<uint16_t>(d[offset + 1]) << 8);
            uint16_t extLen =
                static_cast<uint16_t>(d[offset + 2]) | (static_cast<uint16_t>(d[offset + 3]) << 8);
            offset += 4;

            if (extType == 0 && extLen == 0) {
                break; // End of extensions
            }

            if (offset + extLen > data.size()) {
                break; // Malformed
            }

            if (extType == 19) { // TokenMetadata
                return parseTokenMetadata(data, offset, extLen);
            }

            offset += extLen;
        }

        return result;
    }

  private:
    static Token2022Metadata parseTokenMetadata(const QByteArray& data, int dataOffset,
                                                int dataLen) {
        Token2022Metadata result;
        const auto* d = reinterpret_cast<const uint8_t*>(data.constData());
        int offset = dataOffset;
        int end = dataOffset + dataLen;

        // Skip update_authority (32) + mint (32)
        offset += 64;
        if (offset >= end) {
            return result;
        }

        auto readString = [&](QString& out) -> bool {
            if (offset + 4 > end) {
                return false;
            }
            uint32_t len = static_cast<uint32_t>(d[offset]) |
                           (static_cast<uint32_t>(d[offset + 1]) << 8) |
                           (static_cast<uint32_t>(d[offset + 2]) << 16) |
                           (static_cast<uint32_t>(d[offset + 3]) << 24);
            offset += 4;

            if (len > 1024 || offset + static_cast<int>(len) > end) {
                return false;
            }

            QByteArray raw = data.mid(offset, static_cast<int>(len));
            offset += static_cast<int>(len);

            auto nullPos = raw.indexOf('\0');
            if (nullPos >= 0) {
                raw.truncate(nullPos);
            }
            out = QString::fromUtf8(raw).trimmed();
            return true;
        };

        if (!readString(result.name)) {
            return result;
        }
        if (!readString(result.symbol)) {
            return result;
        }
        if (!readString(result.uri)) {
            return result;
        }

        result.valid = true;
        return result;
    }
};

#endif // TOKEN2022METADATA_H
