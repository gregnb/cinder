#ifndef WALLETCRYPTO_H
#define WALLETCRYPTO_H

#include <QByteArray>
#include <QString>

namespace WalletCrypto {

    struct EncryptedBlob {
        QByteArray salt;       // crypto_pwhash_SALTBYTES (16 bytes)
        QByteArray nonce;      // crypto_secretbox_NONCEBYTES (24 bytes)
        QByteArray ciphertext; // encrypted secret key + MAC tag (80 bytes for 64-byte key)
    };

    // Encrypt a 64-byte secret key with a user password.
    // Uses Argon2id (INTERACTIVE params) for key derivation and
    // XSalsa20-Poly1305 for authenticated encryption.
    EncryptedBlob encrypt(const QByteArray& secretKey64, const QString& password);

    // Decrypt an EncryptedBlob back to the 64-byte secret key.
    // Returns empty QByteArray on wrong password (MAC verification failure).
    // CALLER MUST wipe the returned QByteArray when done.
    QByteArray decrypt(const EncryptedBlob& blob, const QString& password);

} // namespace WalletCrypto

#endif // WALLETCRYPTO_H
