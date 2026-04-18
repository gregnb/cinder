#ifndef PDADERIVATION_H
#define PDADERIVATION_H

#include "tx/Base58.h"
#include <QByteArray>
#include <QList>
#include <QString>
#include <sodium.h>

namespace PdaDerivation {

    // Try to find a program-derived address (PDA) for the given seeds and program ID.
    // Returns the PDA as a base58 string, or empty string if no valid bump found.
    inline QString findProgramAddress(const QList<QByteArray>& seeds, const QString& programId) {
        QByteArray programBytes = Base58::decode(programId);
        if (programBytes.size() != 32) {
            return {};
        }

        static const QByteArray suffix = QByteArrayLiteral("ProgramDerivedAddress");

        for (int bump = 255; bump >= 0; --bump) {
            // Build hash input: seeds || [bump] || programId || "ProgramDerivedAddress"
            QByteArray hashInput;
            for (const auto& seed : seeds) {
                hashInput.append(seed);
            }
            hashInput.append(static_cast<char>(bump));
            hashInput.append(programBytes);
            hashInput.append(suffix);

            // SHA-256 hash
            unsigned char hash[crypto_hash_sha256_BYTES];
            crypto_hash_sha256(hash, reinterpret_cast<const unsigned char*>(hashInput.constData()),
                               hashInput.size());

            // A valid PDA must NOT be on the Ed25519 curve.
            // We check by trying to decompress as an Ed25519 point — if it fails,
            // the hash is off-curve and thus a valid PDA.
            // crypto_core_ed25519_is_valid_point returns 1 if on curve.
            if (crypto_core_ed25519_is_valid_point(hash) == 0) {
                QByteArray pdaBytes(reinterpret_cast<const char*>(hash), 32);
                return Base58::encode(pdaBytes);
            }
        }

        return {}; // No valid bump found (extremely unlikely)
    }

    // Derive the Metaplex Token Metadata PDA for a given mint address.
    // Seeds: ["metadata", METADATA_PROGRAM_ID, MINT_PUBKEY]
    inline QString metaplexMetadataPda(const QString& mintAddress) {
        static const QString METADATA_PROGRAM = "metaqbxxUerdq28cj1RbAWkYQm3ybzjb6a8bt518x1s";

        QByteArray seed1 = QByteArrayLiteral("metadata");
        QByteArray seed2 = Base58::decode(METADATA_PROGRAM);
        QByteArray seed3 = Base58::decode(mintAddress);

        if (seed2.size() != 32 || seed3.size() != 32) {
            return {};
        }

        return findProgramAddress({seed1, seed2, seed3}, METADATA_PROGRAM);
    }

} // namespace PdaDerivation

#endif // PDADERIVATION_H
