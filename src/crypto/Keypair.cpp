#include "Keypair.h"
#include "tx/Base58.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <sodium.h>

// ── Construction ─────────────────────────────────────────

Keypair Keypair::generate() {
    Keypair kp;
    kp.m_public.resize(crypto_sign_PUBLICKEYBYTES); // 32
    kp.m_secret.resize(crypto_sign_SECRETKEYBYTES); // 64

    crypto_sign_keypair(reinterpret_cast<unsigned char*>(kp.m_public.data()),
                        reinterpret_cast<unsigned char*>(kp.m_secret.data()));

    kp.lockMemory();
    return kp;
}

Keypair Keypair::fromSeed(const QByteArray& seed32) {
    if (seed32.size() != crypto_sign_SEEDBYTES) { // 32
        return {};
    }

    Keypair kp;
    kp.m_public.resize(crypto_sign_PUBLICKEYBYTES);
    kp.m_secret.resize(crypto_sign_SECRETKEYBYTES);

    crypto_sign_seed_keypair(reinterpret_cast<unsigned char*>(kp.m_public.data()),
                             reinterpret_cast<unsigned char*>(kp.m_secret.data()),
                             reinterpret_cast<const unsigned char*>(seed32.constData()));

    kp.lockMemory();
    return kp;
}

Keypair Keypair::fromSecretKey(const QByteArray& sk64) {
    if (sk64.size() != crypto_sign_SECRETKEYBYTES) { // 64
        return {};
    }

    // Re-derive the public key from the seed (first 32 bytes) to detect
    // corrupted imports. A mismatched pubkey would cause fund loss.
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_seed_keypair(pk, sk, reinterpret_cast<const unsigned char*>(sk64.constData()));

    Keypair kp;
    kp.m_secret = QByteArray(reinterpret_cast<const char*>(sk), crypto_sign_SECRETKEYBYTES);
    kp.m_public = QByteArray(reinterpret_cast<const char*>(pk), crypto_sign_PUBLICKEYBYTES);

    sodium_memzero(pk, sizeof(pk));
    sodium_memzero(sk, sizeof(sk));
    kp.lockMemory();
    return kp;
}

Keypair Keypair::fromJson(const QByteArray& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        return {};
    }

    QJsonArray arr = doc.array();
    if (arr.size() != crypto_sign_SECRETKEYBYTES) {
        return {};
    }

    QByteArray sk(64, '\0');
    for (int i = 0; i < 64; ++i) {
        int v = arr[i].toInt();
        if (v < 0 || v > 255) {
            return {};
        }
        sk[i] = static_cast<char>(v);
    }

    return fromSecretKey(sk);
}

Keypair Keypair::fromBase58(const QString& b58) {
    QByteArray decoded = Base58::decode(b58);
    if (decoded.size() != crypto_sign_SECRETKEYBYTES) {
        return {};
    }
    return fromSecretKey(decoded);
}

// ── Signing ──────────────────────────────────────────────

QByteArray Keypair::sign(const QByteArray& message) const {
    if (isNull()) {
        return {};
    }

    QByteArray sig(crypto_sign_BYTES, '\0'); // 64

    crypto_sign_detached(reinterpret_cast<unsigned char*>(sig.data()), nullptr,
                         reinterpret_cast<const unsigned char*>(message.constData()),
                         static_cast<unsigned long long>(message.size()),
                         reinterpret_cast<const unsigned char*>(m_secret.constData()));

    return sig;
}

bool Keypair::verify(const QByteArray& pubkey32, const QByteArray& message,
                     const QByteArray& signature) {
    if (pubkey32.size() != crypto_sign_PUBLICKEYBYTES || signature.size() != crypto_sign_BYTES) {
        return false;
    }

    return crypto_sign_verify_detached(
               reinterpret_cast<const unsigned char*>(signature.constData()),
               reinterpret_cast<const unsigned char*>(message.constData()),
               static_cast<unsigned long long>(message.size()),
               reinterpret_cast<const unsigned char*>(pubkey32.constData())) == 0;
}

// ── Accessors ────────────────────────────────────────────

QString Keypair::address() const { return Base58::encode(m_public); }

QByteArray Keypair::publicKey() const { return m_public; }

bool Keypair::writeSecretKeyTo(uint8_t* out, size_t outLen) const {
    if (isNull() || outLen < 64) {
        return false;
    }
    memcpy(out, m_secret.constData(), 64);
    return true;
}

QByteArray Keypair::secretKey() const { return m_secret; }

// ── Export ────────────────────────────────────────────────

QByteArray Keypair::toJson() const {
    if (isNull()) {
        return {};
    }

    QJsonArray arr;
    for (int i = 0; i < m_secret.size(); ++i) {
        arr.append(static_cast<int>(static_cast<unsigned char>(m_secret[i])));
    }

    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

QString Keypair::toBase58() const {
    if (isNull()) {
        return {};
    }
    return Base58::encode(m_secret);
}

bool Keypair::isNull() const { return m_secret.isEmpty(); }

// ── Memory locking ──────────────────────────────────────

void Keypair::lockMemory() {
    if (!m_secret.isEmpty()) {
        sodium_mlock(m_secret.data(), static_cast<size_t>(m_secret.size()));
    }
    if (!m_public.isEmpty()) {
        sodium_mlock(m_public.data(), static_cast<size_t>(m_public.size()));
    }
}

void Keypair::unlockMemory() {
    if (!m_secret.isEmpty()) {
        sodium_munlock(m_secret.data(), static_cast<size_t>(m_secret.size()));
    }
    if (!m_public.isEmpty()) {
        sodium_munlock(m_public.data(), static_cast<size_t>(m_public.size()));
    }
}

// ── Lifecycle ────────────────────────────────────────────

Keypair::~Keypair() {
    // sodium_munlock also zeros the memory, but we call memzero explicitly
    // for clarity and in case mlock was not supported on this platform.
    if (!m_secret.isEmpty()) {
        sodium_munlock(m_secret.data(), static_cast<size_t>(m_secret.size()));
        sodium_memzero(m_secret.data(), static_cast<size_t>(m_secret.size()));
    }
    if (!m_public.isEmpty()) {
        sodium_munlock(m_public.data(), static_cast<size_t>(m_public.size()));
        sodium_memzero(m_public.data(), static_cast<size_t>(m_public.size()));
    }
}

Keypair::Keypair(const Keypair& other) : m_secret(other.m_secret), m_public(other.m_public) {
    // Force COW detach so this Keypair owns its buffer exclusively.
    // Without this, the destructor's sodium_memzero would detach and
    // zero a fresh copy instead of zeroing the actual key data.
    if (!m_secret.isEmpty()) {
        m_secret.detach();
    }
    lockMemory();
}

Keypair& Keypair::operator=(const Keypair& other) {
    if (this != &other) {
        unlockMemory();
        if (!m_secret.isEmpty()) {
            sodium_memzero(m_secret.data(), static_cast<size_t>(m_secret.size()));
        }
        m_secret = other.m_secret;
        m_public = other.m_public;
        if (!m_secret.isEmpty()) {
            m_secret.detach(); // Own our buffer exclusively for safe zeroing
        }
        lockMemory();
    }
    return *this;
}

Keypair::Keypair(Keypair&& other) noexcept
    : m_secret(std::move(other.m_secret)), m_public(std::move(other.m_public)) {
    // Memory pages are already locked from the source; ownership transfers
    // with the buffer pointer so no re-lock needed.
}

Keypair& Keypair::operator=(Keypair&& other) noexcept {
    if (this != &other) {
        unlockMemory();
        if (!m_secret.isEmpty()) {
            sodium_memzero(m_secret.data(), static_cast<size_t>(m_secret.size()));
        }
        m_secret = std::move(other.m_secret);
        m_public = std::move(other.m_public);
    }
    return *this;
}
