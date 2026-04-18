#ifndef METAPLEXMETADATA_H
#define METAPLEXMETADATA_H

#include <QByteArray>
#include <QString>

// Parses Metaplex Token Metadata account data (Borsh-encoded).
//
// Account layout (Borsh):
//   1 byte:  key (enum discriminator, 4 = MetadataV1)
//  32 bytes: update_authority
//  32 bytes: mint
//   4 bytes + N: name   (u32 length + UTF-8, null-padded)
//   4 bytes + N: symbol (u32 length + UTF-8, null-padded)
//   4 bytes + N: uri    (u32 length + UTF-8, null-padded)
//   ... (remaining fields not needed)

struct MetaplexMetadata {
    QString name;
    QString symbol;
    QString uri;
    bool valid = false;

    static MetaplexMetadata fromAccountData(const QByteArray& data) {
        MetaplexMetadata result;

        // Minimum size: 1 + 32 + 32 + 4 + 4 + 4 = 77
        if (data.size() < 77) {
            return result;
        }

        const auto* d = reinterpret_cast<const uint8_t*>(data.constData());
        int offset = 0;

        // Key discriminator — must be 4 (MetadataV1)
        uint8_t key = d[offset];
        if (key != 4) {
            return result;
        }
        offset += 1;

        // Skip update_authority (32 bytes) + mint (32 bytes)
        offset += 64;

        auto readBorshString = [&](QString& out) -> bool {
            if (offset + 4 > data.size()) {
                return false;
            }
            uint32_t len = static_cast<uint32_t>(d[offset]) |
                           (static_cast<uint32_t>(d[offset + 1]) << 8) |
                           (static_cast<uint32_t>(d[offset + 2]) << 16) |
                           (static_cast<uint32_t>(d[offset + 3]) << 24);
            offset += 4;

            if (len > 1024 || offset + static_cast<int>(len) > data.size()) {
                return false;
            }

            QByteArray raw = data.mid(offset, static_cast<int>(len));
            offset += static_cast<int>(len);

            // Trim null padding
            auto end = raw.indexOf('\0');
            if (end >= 0) {
                raw.truncate(end);
            }
            out = QString::fromUtf8(raw).trimmed();
            return true;
        };

        if (!readBorshString(result.name)) {
            return result;
        }
        if (!readBorshString(result.symbol)) {
            return result;
        }
        if (!readBorshString(result.uri)) {
            return result;
        }

        result.valid = true;
        return result;
    }
};

#endif // METAPLEXMETADATA_H
