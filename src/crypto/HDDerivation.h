#ifndef HDDERIVATION_H
#define HDDERIVATION_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <sodium.h>

namespace HDDerivation {

    // SLIP-0010 Ed25519 hierarchical deterministic key derivation.
    // Input: 64-byte BIP39 seed (from Mnemonic::toSeed)
    // Output: 32-byte Ed25519 seed (use with Keypair::fromSeed)
    //
    // Default path m/44'/501'/0'/0' is Phantom/Ledger compatible.
    // All segments are hardened (required for Ed25519).

    inline QByteArray derive(const QByteArray& seed64, const QList<quint32>& pathSegments) {
        if (seed64.size() != 64) {
            return {};
        }

        // Step 1: Master key from seed
        // I = HMAC-SHA512(key="ed25519 seed", data=seed)
        const char* masterKey = "ed25519 seed";
        unsigned char I[64];

        crypto_auth_hmacsha512_state state;
        crypto_auth_hmacsha512_init(&state, reinterpret_cast<const unsigned char*>(masterKey), 12);
        crypto_auth_hmacsha512_update(
            &state, reinterpret_cast<const unsigned char*>(seed64.constData()), 64);
        crypto_auth_hmacsha512_final(&state, I);

        // I_L = private key (32 bytes), I_R = chain code (32 bytes)
        unsigned char key[32];
        unsigned char chainCode[32];
        memcpy(key, I, 32);
        memcpy(chainCode, I + 32, 32);

        // Step 2: Derive child keys for each path segment
        for (quint32 segment : pathSegments) {
            // Ensure hardened (set bit 31)
            quint32 index = segment | 0x80000000u;

            // data = 0x00 || key(32) || index(4, big-endian)
            unsigned char data[37];
            data[0] = 0x00;
            memcpy(data + 1, key, 32);
            data[33] = static_cast<unsigned char>((index >> 24) & 0xFF);
            data[34] = static_cast<unsigned char>((index >> 16) & 0xFF);
            data[35] = static_cast<unsigned char>((index >> 8) & 0xFF);
            data[36] = static_cast<unsigned char>(index & 0xFF);

            // I = HMAC-SHA512(key=chainCode, data)
            crypto_auth_hmacsha512_init(&state, chainCode, 32);
            crypto_auth_hmacsha512_update(&state, data, 37);
            crypto_auth_hmacsha512_final(&state, I);

            memcpy(key, I, 32);
            memcpy(chainCode, I + 32, 32);

            sodium_memzero(data, sizeof(data));
        }

        QByteArray result(reinterpret_cast<const char*>(key), 32);

        sodium_memzero(I, sizeof(I));
        sodium_memzero(key, sizeof(key));
        sodium_memzero(chainCode, sizeof(chainCode));
        sodium_memzero(&state, sizeof(state));

        return result;
    }

    // Parse path string like "m/44'/501'/0'/0'" into segment list
    inline QByteArray derive(const QByteArray& seed64, const QString& path = "m/44'/501'/0'/0'") {
        QString p = path;
        if (p.startsWith("m/")) {
            p = p.mid(2);
        }

        QStringList parts = p.split('/', Qt::SkipEmptyParts);
        QList<quint32> segments;

        for (const QString& part : parts) {
            QString num = part;
            num.remove('\'');
            bool ok = false;
            quint32 val = num.toUInt(&ok);
            if (!ok) {
                return {};
            }
            segments.append(val);
        }

        return derive(seed64, segments);
    }

} // namespace HDDerivation

#endif // HDDERIVATION_H
