#include "Mnemonic.h"
#include "crypto/Pbkdf2.h"
#include "crypto/bip39_wordlist.h"
#include <QMap>
#include <QStringList>
#include <sodium.h>

// ── Helpers ──────────────────────────────────────────────

static QMap<QString, int> buildWordIndex() {
    QMap<QString, int> map;
    for (int i = 0; i < Bip39::WORDLIST_SIZE; ++i) {
        map[QString::fromLatin1(Bip39::WORDLIST[i])] = i;
    }
    return map;
}

static const QMap<QString, int>& wordIndex() {
    static const QMap<QString, int> idx = buildWordIndex();
    return idx;
}

// Get bit `bitPos` from a byte array (MSB-first)
static int getBit(const unsigned char* data, int bitPos) {
    return (data[bitPos / 8] >> (7 - (bitPos % 8))) & 1;
}

// Extract 11-bit value starting at `bitOffset` from combined entropy+checksum
static int extract11Bits(const unsigned char* data, int bitOffset) {
    int val = 0;
    for (int i = 0; i < 11; ++i) {
        val = (val << 1) | getBit(data, bitOffset + i);
    }
    return val;
}

// ── Generate ─────────────────────────────────────────────

QString Mnemonic::generate(int wordCount) {
    // 12 words = 128 bits entropy, 24 words = 256 bits entropy
    int entropyBits;
    if (wordCount == 12) {
        entropyBits = 128;
    } else if (wordCount == 24) {
        entropyBits = 256;
    } else {
        return {};
    }

    int entropyBytes = entropyBits / 8;
    int checksumBits = entropyBits / 32;

    // Generate cryptographic entropy using libsodium's CSPRNG.
    // On macOS this reads from arc4random (backed by the kernel's
    // ChaCha20-based CSPRNG seeded from hardware entropy).
    // On Linux it reads from /dev/urandom after getrandom().
    unsigned char entropy[32]; // max 256 bits
    randombytes_buf(entropy, static_cast<size_t>(entropyBytes));

    // SHA-256 for checksum
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, entropy, static_cast<unsigned long long>(entropyBytes));

    // Build combined: entropy || first checksumBits of hash
    // Total bits = entropyBits + checksumBits = wordCount * 11
    int totalBytes = (entropyBits + checksumBits + 7) / 8;
    unsigned char combined[33] = {}; // MUST zero-init: |=/&= below read before write
    memcpy(combined, entropy, static_cast<size_t>(entropyBytes));

    // Append checksum bits
    for (int i = 0; i < checksumBits; ++i) {
        int bit = getBit(hash, i);
        int pos = entropyBits + i;
        if (bit) {
            combined[pos / 8] |= (1 << (7 - (pos % 8)));
        } else {
            combined[pos / 8] &= ~(1 << (7 - (pos % 8)));
        }
    }

    // Split into 11-bit groups and look up words
    QStringList words;
    for (int i = 0; i < wordCount; ++i) {
        int idx = extract11Bits(combined, i * 11);
        words.append(QString::fromLatin1(Bip39::WORDLIST[idx]));
    }

    sodium_memzero(entropy, sizeof(entropy));
    sodium_memzero(hash, sizeof(hash));
    sodium_memzero(combined, sizeof(combined));

    return words.join(QChar(' '));
}

// ── Generate + Derive ────────────────────────────────────

Mnemonic::MnemonicAndSeed Mnemonic::generateWithSeed(int wordCount, const QString& passphrase) {
    QString mnemonic = generate(wordCount);
    if (mnemonic.isEmpty()) {
        return {};
    }

    QByteArray seed = toSeed(mnemonic, passphrase);
    return {std::move(mnemonic), std::move(seed)};
}

// ── Validate ─────────────────────────────────────────────

bool Mnemonic::validate(const QString& mnemonic) {
    QStringList words = mnemonic.simplified().split(' ', Qt::SkipEmptyParts);
    int wordCount = words.size();

    if (wordCount != 12 && wordCount != 15 && wordCount != 18 && wordCount != 21 &&
        wordCount != 24) {
        return false;
    }

    const auto& idx = wordIndex();

    // Look up each word and reconstruct the bit stream
    int totalBits = wordCount * 11;
    int entropyBits = totalBits - totalBits / 33; // checksumBits = totalBits / 33
    int checksumBits = totalBits / 33;

    unsigned char data[33] = {}; // max 264 bits
    for (int w = 0; w < wordCount; ++w) {
        auto it = idx.find(words[w].toLower());
        if (it == idx.end()) {
            return false;
        }

        int val = it.value();
        int bitPos = w * 11;
        for (int b = 0; b < 11; ++b) {
            if (val & (1 << (10 - b))) {
                data[(bitPos + b) / 8] |= (1 << (7 - ((bitPos + b) % 8)));
            }
        }
    }

    // Extract entropy bytes
    int entropyBytes = entropyBits / 8;
    unsigned char entropy[32];
    memcpy(entropy, data, static_cast<size_t>(entropyBytes));

    // Recompute checksum
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, entropy, static_cast<unsigned long long>(entropyBytes));

    // Compare checksum bits
    bool valid = true;
    for (int i = 0; i < checksumBits; ++i) {
        int expected = getBit(hash, i);
        int actual = getBit(data, entropyBits + i);
        if (expected != actual) {
            valid = false;
            break;
        }
    }

    sodium_memzero(data, sizeof(data));
    sodium_memzero(entropy, sizeof(entropy));
    sodium_memzero(hash, sizeof(hash));

    return valid;
}

// ── Mnemonic to Seed ─────────────────────────────────────

QByteArray Mnemonic::toSeed(const QString& mnemonic, const QString& passphrase) {
    // BIP39 §5: mnemonic and passphrase MUST be NFKD-normalized before UTF-8 encoding.
    // For English ASCII mnemonics NFKD is a no-op, but passphrases may contain
    // composed characters (e.g. ñ = U+00F1) that must decompose for interop.
    QByteArray password = mnemonic.normalized(QString::NormalizationForm_KD).toUtf8();
    QByteArray salt =
        QByteArray("mnemonic") + passphrase.normalized(QString::NormalizationForm_KD).toUtf8();

    QByteArray seed = Pbkdf2::hmacSha512(password, salt, 2048, 64);

    sodium_memzero(password.data(), static_cast<size_t>(password.size()));
    sodium_memzero(salt.data(), static_cast<size_t>(salt.size()));

    return seed;
}
