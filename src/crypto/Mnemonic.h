#ifndef MNEMONIC_H
#define MNEMONIC_H

#include <QByteArray>
#include <QString>

namespace Mnemonic {

    // ── Generation ──────────────────────────────────────────
    //
    // LIFECYCLE: The returned mnemonic QString contains secret material.
    // Display it once for the user to write down, derive the seed, then
    // immediately wipe the string buffer:
    //
    //   QString phrase = Mnemonic::generate();
    //   showToUser(phrase);                          // display once
    //   QByteArray seed = Mnemonic::toSeed(phrase);  // derive
    //   sodium_memzero(phrase.data(), phrase.size() * sizeof(QChar));  // wipe
    //
    // Prefer generateWithSeed() to avoid a second mnemonic copy through toSeed().

    // Generate a new BIP39 mnemonic (12 or 24 words).
    // Uses libsodium's randombytes_buf() for cryptographic entropy.
    QString generate(int wordCount = 12);

    struct MnemonicAndSeed {
        QString mnemonic;  // show to user, then wipe
        QByteArray seed64; // 64-byte BIP39 seed, ready for HDDerivation
    };

    // Generate a mnemonic AND derive its seed in one call.
    // Avoids passing the mnemonic through toSeed() separately (which
    // creates an extra UTF-8 copy internally). Wipes intermediates.
    MnemonicAndSeed generateWithSeed(int wordCount = 12, const QString& passphrase = QString());

    // ── Validation ──────────────────────────────────────────

    // Validate a BIP39 mnemonic (checksum + wordlist check).
    bool validate(const QString& mnemonic);

    // ── Seed derivation ─────────────────────────────────────

    // Convert mnemonic to 64-byte BIP39 seed via PBKDF2-HMAC-SHA512 (2048 rounds).
    QByteArray toSeed(const QString& mnemonic, const QString& passphrase = QString());

} // namespace Mnemonic

#endif // MNEMONIC_H
