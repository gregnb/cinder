#ifndef LATTICECRYPTO_H
#define LATTICECRYPTO_H

#include <QByteArray>
#include <QString>

namespace LatticeCrypto {

    constexpr int kP256PubKeySize = 65; // uncompressed: 04 || x(32) || y(32)
    constexpr int kP256PrivKeySize = 32;
    constexpr int kSharedSecretSize = 32;
    constexpr int kAesKeySize = 32;
    constexpr int kAesIvSize = 16;
    constexpr int kEd25519KeySize = 32;
    constexpr int kEd25519SignatureSize = 64;

    struct EcKeyPair {
        QByteArray privateKey; // 32 bytes
        QByteArray publicKey;  // 65 bytes (uncompressed P-256)
        bool isValid() const {
            return privateKey.size() == kP256PrivKeySize && publicKey.size() == kP256PubKeySize;
        }
    };

    // Generate a new P-256 (secp256r1) keypair
    EcKeyPair generateP256Keypair();

    // Derive a shared secret via ECDH P-256
    QByteArray deriveSharedSecret(const QByteArray& ourPrivKey, const QByteArray& theirPub65);

    // AES-256-CBC encrypt (PKCS7 padding). IV is prepended to output.
    QByteArray aesCbcEncrypt(const QByteArray& plaintext, const QByteArray& key32);

    // AES-256-CBC decrypt. Expects IV prepended to ciphertext.
    QByteArray aesCbcDecrypt(const QByteArray& ciphertext, const QByteArray& key32);

    // SHA-256 hash
    QByteArray sha256(const QByteArray& data);

    // CRC32 checksum
    uint32_t crc32(const QByteArray& data);

    // ECDSA P-256 sign, returns DER-encoded signature
    QByteArray ecdsaSignDer(const QByteArray& hash32, const QByteArray& privKey32);

    // Generate pairing secret: SHA-256(deviceId || pairingCode || appName)
    QByteArray generatePairingHash(const QString& deviceId, const QString& pairingCode,
                                   const QString& appName);

} // namespace LatticeCrypto

#endif // LATTICECRYPTO_H
