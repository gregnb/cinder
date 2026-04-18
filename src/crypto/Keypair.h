#ifndef KEYPAIR_H
#define KEYPAIR_H

#include <QByteArray>
#include <QString>
#include <cstdint>

class Keypair {
  public:
    Keypair() = default;

    // ── Construction ─────────────────────────────────────

    static Keypair generate();
    static Keypair fromSeed(const QByteArray& seed32);
    static Keypair fromSecretKey(const QByteArray& sk64);
    static Keypair fromJson(const QByteArray& json);
    static Keypair fromBase58(const QString& b58);

    // ── Signing ──────────────────────────────────────────

    QByteArray sign(const QByteArray& message) const;

    static bool verify(const QByteArray& pubkey32, const QByteArray& message,
                       const QByteArray& signature);

    // ── Accessors ────────────────────────────────────────

    QString address() const;
    QByteArray publicKey() const;

    // ── Secret key access ────────────────────────────────
    //
    // WARNING: These methods expose raw secret key material.
    //
    // Prefer writeSecretKeyTo() which writes into a caller-owned
    // buffer that you can wipe immediately after use.
    //
    // secretKey(), toJson(), and toBase58() return COPIES.
    // The caller is responsible for wiping them via
    //   sodium_memzero(ba.data(), ba.size())  // QByteArray
    // as soon as the data has been consumed.

    // Write the 64-byte secret key (seed||pubkey) into a caller-provided
    // buffer. Returns false if the keypair is null or outLen < 64.
    // The caller owns `out` and must wipe it when done.
    bool writeSecretKeyTo(uint8_t* out, size_t outLen) const;

    // Returns a heap-allocated COPY of the 64-byte secret key.
    // CALLER MUST WIPE the returned QByteArray when done.
    QByteArray secretKey() const;

    // ── Export ────────────────────────────────────────────
    //
    // Both methods return COPIES of secret key material.
    // CALLER MUST WIPE the returned buffer/string when done.

    // Returns [u8; 64] JSON array (solana-keygen CLI format).
    QByteArray toJson() const;

    // Returns base58 encoding of the 64-byte secret key (Phantom format).
    QString toBase58() const;

    bool isNull() const;

    ~Keypair();
    Keypair(const Keypair& other);
    Keypair& operator=(const Keypair& other);
    Keypair(Keypair&& other) noexcept;
    Keypair& operator=(Keypair&& other) noexcept;

  private:
    void lockMemory();   // sodium_mlock to prevent swap-to-disk
    void unlockMemory(); // sodium_munlock to release the lock
    QByteArray m_secret; // 64 bytes: seed(32) || pubkey(32)
    QByteArray m_public; // 32 bytes
};

#endif // KEYPAIR_H
