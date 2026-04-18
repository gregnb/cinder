#include "WalletCrypto.h"
#include <sodium.h>

WalletCrypto::EncryptedBlob WalletCrypto::encrypt(const QByteArray& secretKey64,
                                                  const QString& password) {
    EncryptedBlob blob;

    // 1. Random salt
    blob.salt.resize(crypto_pwhash_SALTBYTES);
    randombytes_buf(blob.salt.data(), crypto_pwhash_SALTBYTES);

    // 2. Derive encryption key from password via Argon2id
    QByteArray passwordUtf8 = password.toUtf8();
    unsigned char derivedKey[crypto_secretbox_KEYBYTES]; // 32

    if (crypto_pwhash(derivedKey, sizeof(derivedKey), passwordUtf8.constData(), passwordUtf8.size(),
                      reinterpret_cast<const unsigned char*>(blob.salt.constData()),
                      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        sodium_memzero(passwordUtf8.data(), passwordUtf8.size());
        return {};
    }
    sodium_memzero(passwordUtf8.data(), passwordUtf8.size());

    // 3. Random nonce
    blob.nonce.resize(crypto_secretbox_NONCEBYTES);
    randombytes_buf(blob.nonce.data(), crypto_secretbox_NONCEBYTES);

    // 4. Encrypt via XSalsa20-Poly1305
    int ctLen = secretKey64.size() + crypto_secretbox_MACBYTES; // 64 + 16 = 80
    blob.ciphertext.resize(ctLen);

    crypto_secretbox_easy(
        reinterpret_cast<unsigned char*>(blob.ciphertext.data()),
        reinterpret_cast<const unsigned char*>(secretKey64.constData()), secretKey64.size(),
        reinterpret_cast<const unsigned char*>(blob.nonce.constData()), derivedKey);

    // 5. Wipe derived key
    sodium_memzero(derivedKey, sizeof(derivedKey));

    return blob;
}

// Try decryption with specific Argon2id params.
static QByteArray tryDecrypt(const WalletCrypto::EncryptedBlob& blob,
                             const QByteArray& passwordUtf8, unsigned long long opsLimit,
                             size_t memLimit) {
    unsigned char derivedKey[crypto_secretbox_KEYBYTES];

    if (crypto_pwhash(derivedKey, sizeof(derivedKey), passwordUtf8.constData(), passwordUtf8.size(),
                      reinterpret_cast<const unsigned char*>(blob.salt.constData()), opsLimit,
                      memLimit, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        return {};
    }

    int ptLen = blob.ciphertext.size() - crypto_secretbox_MACBYTES;
    QByteArray plaintext(ptLen, '\0');

    if (crypto_secretbox_open_easy(
            reinterpret_cast<unsigned char*>(plaintext.data()),
            reinterpret_cast<const unsigned char*>(blob.ciphertext.constData()),
            blob.ciphertext.size(), reinterpret_cast<const unsigned char*>(blob.nonce.constData()),
            derivedKey) != 0) {
        sodium_memzero(derivedKey, sizeof(derivedKey));
        return {};
    }

    sodium_memzero(derivedKey, sizeof(derivedKey));
    return plaintext;
}

QByteArray WalletCrypto::decrypt(const EncryptedBlob& blob, const QString& password) {
    QByteArray passwordUtf8 = password.toUtf8();

    // Current wallets derive with INTERACTIVE parameters only.
    QByteArray result = tryDecrypt(blob, passwordUtf8, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                   crypto_pwhash_MEMLIMIT_INTERACTIVE);

    sodium_memzero(passwordUtf8.data(), passwordUtf8.size());
    return result; // Caller MUST wipe when done
}
