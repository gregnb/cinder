#include "AssociatedTokenInstruction.h"
#include "tx/Base58.h"
#include <sodium.h>

// ── Ed25519 on-curve check ──────────────────────────────────────
//
// Solana's PDA check uses curve25519-dalek's CompressedEdwardsY::decompress()
// which only verifies the point decompresses (is on the curve). Libsodium's
// crypto_sign_ed25519_pk_to_curve25519 additionally rejects small-order and
// non-main-subgroup points, causing PDA derivation mismatches. We implement
// the pure decompression check to match Solana exactly.

namespace {

    // 256-bit unsigned integer (4 × 64-bit limbs, little-endian)
    struct U256 {
        uint64_t d[4] = {};
    };

    // p = 2^255 - 19
    static constexpr U256 P = {{0xffffffffffffffedULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
                                0x7fffffffffffffffULL}};

    // d = -121665/121666 mod p
    // = 37095705934669439343138083508754565189542113879843219016388785533085940283555
    static constexpr U256 ED25519_D = {{0x75eb4dca135978a3ULL, 0x00700a4d4141d8abULL,
                                        0x8cc740797779e898ULL, 0x52036cee2b6ffe73ULL}};

    inline bool geq(const U256& a, const U256& b) {
        for (int i = 3; i >= 0; --i) {
            if (a.d[i] > b.d[i]) {
                return true;
            }
            if (a.d[i] < b.d[i]) {
                return false;
            }
        }
        return true; // equal
    }

    inline U256 sub256(const U256& a, const U256& b) {
        U256 r;
        __uint128_t borrow = 0;
        for (int i = 0; i < 4; ++i) {
            __uint128_t diff = (__uint128_t)a.d[i] - b.d[i] - borrow;
            r.d[i] = (uint64_t)diff;
            borrow = (diff >> 127) & 1; // borrow if negative
        }
        return r;
    }

    // Reduce mod p (assumes a < 2p)
    inline U256 reduce(const U256& a) { return geq(a, P) ? sub256(a, P) : a; }

    // Multiply mod p using __uint128_t and p-specific reduction (2^256 ≡ 38 mod p)
    U256 mulmod(const U256& a, const U256& b) {
        // Full 512-bit product in 8 limbs
        __uint128_t t[8] = {};
        for (int i = 0; i < 4; ++i) {
            __uint128_t carry = 0;
            for (int j = 0; j < 4; ++j) {
                __uint128_t prod = (__uint128_t)a.d[i] * b.d[j] + t[i + j] + carry;
                t[i + j] = (uint64_t)prod;
                carry = prod >> 64;
            }
            t[i + 4] = carry;
        }

        // Reduce: result = low + high * 38 mod p  (since 2^256 ≡ 38 mod p)
        // First pass: multiply high part (t[4..7]) by 38 and add to low part
        __uint128_t carry = 0;
        for (int i = 0; i < 4; ++i) {
            __uint128_t sum = t[i] + t[i + 4] * 38 + carry;
            t[i] = (uint64_t)sum;
            carry = sum >> 64;
        }

        // carry could still be up to ~38, multiply by 38 again and add
        U256 r = {{(uint64_t)t[0], (uint64_t)t[1], (uint64_t)t[2], (uint64_t)t[3]}};
        if (carry > 0) {
            __uint128_t extra = carry * 38;
            __uint128_t sum = (__uint128_t)r.d[0] + (uint64_t)extra;
            r.d[0] = (uint64_t)sum;
            carry = sum >> 64;
            for (int i = 1; i < 4 && carry; ++i) {
                sum = (__uint128_t)r.d[i] + carry;
                r.d[i] = (uint64_t)sum;
                carry = sum >> 64;
            }
        }

        // Final reductions (at most 2 needed)
        r = reduce(r);
        r = reduce(r);
        return r;
    }

    // Square mod p (uses mulmod for simplicity)
    inline U256 sqrmod(const U256& a) { return mulmod(a, a); }

    // Modular exponentiation: base^exp mod p
    U256 powmod(const U256& base, const U256& exp) {
        U256 result = {{1, 0, 0, 0}};
        U256 b = base;
        for (int i = 0; i < 4; ++i) {
            uint64_t e = exp.d[i];
            for (int bit = 0; bit < 64; ++bit) {
                if (e & 1) {
                    result = mulmod(result, b);
                }
                b = sqrmod(b);
                e >>= 1;
            }
        }
        return result;
    }

    // Check if 32-byte compressed Ed25519 point is on the curve.
    // Matches Solana's CompressedEdwardsY::decompress().is_some().
    bool isOnEd25519Curve(const unsigned char* bytes) {
        // Decode y (little-endian, clear sign bit)
        U256 y;
        memcpy(y.d, bytes, 32);
        y.d[3] &= 0x7fffffffffffffffULL; // clear bit 255

        // y must be < p
        if (geq(y, P)) {
            return false;
        }

        // y² mod p
        U256 y2 = sqrmod(y);

        // u = y² - 1 mod p
        U256 one = {{1, 0, 0, 0}};
        U256 u = geq(y2, one) ? sub256(y2, one) : sub256(reduce(sub256(P, sub256(one, y2))), {{0}});
        // Simplified: u = (y2 + p - 1) mod p  when y2 < 1
        if (!geq(y2, one)) {
            // y2 == 0, so u = p - 1
            u = sub256(P, one);
        }

        // v = d·y² + 1 mod p
        U256 dy2 = mulmod(ED25519_D, y2);
        // v = dy2 + 1, might need reduction
        __uint128_t sum = (__uint128_t)dy2.d[0] + 1;
        U256 v;
        v.d[0] = (uint64_t)sum;
        __uint128_t carry = sum >> 64;
        for (int i = 1; i < 4; ++i) {
            sum = (__uint128_t)dy2.d[i] + carry;
            v.d[i] = (uint64_t)sum;
            carry = sum >> 64;
        }
        v = reduce(v);

        // Check if u/v is a quadratic residue using Euler's criterion:
        // (u · v^(p-2))^((p-1)/2) ≡ 1 mod p  ⟺  u/v is a QR
        //
        // (p-2) = 2^255 - 21
        U256 pm2 = {{0xffffffffffffffebULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
                     0x7fffffffffffffffULL}};
        U256 v_inv = powmod(v, pm2); // v^(p-2) = v^(-1)
        U256 uv = mulmod(u, v_inv);  // u/v mod p

        // (p-1)/2 = 2^254 - 10
        U256 pm1_2 = {{0xfffffffffffffff6ULL, 0xffffffffffffffffULL, 0xffffffffffffffffULL,
                       0x3fffffffffffffffULL}};
        U256 euler = powmod(uv, pm1_2);

        // QR if euler == 1 or uv == 0 (zero is trivially a square)
        bool isQR = (euler.d[0] == 1 && euler.d[1] == 0 && euler.d[2] == 0 && euler.d[3] == 0);
        bool isZero = (uv.d[0] == 0 && uv.d[1] == 0 && uv.d[2] == 0 && uv.d[3] == 0);

        return isQR || isZero;
    }

} // anonymous namespace

namespace AssociatedTokenInstruction {

    QString deriveAddress(const QString& owner, const QString& mint,
                          const QString& tokenProgramId) {
        QByteArray ownerBytes = Base58::decode(owner);
        QByteArray programBytes = Base58::decode(tokenProgramId);
        QByteArray mintBytes = Base58::decode(mint);
        QByteArray ataProgramBytes = Base58::decode(SolanaPrograms::AssociatedTokenAccount);

        if (ownerBytes.size() != 32 || programBytes.size() != 32 || mintBytes.size() != 32 ||
            ataProgramBytes.size() != 32) {
            return {};
        }

        // findProgramAddress: try bump seeds from 255 down to 0
        for (int bump = 255; bump >= 0; --bump) {
            unsigned char hash[crypto_hash_sha256_BYTES];
            crypto_hash_sha256_state state;
            crypto_hash_sha256_init(&state);
            crypto_hash_sha256_update(
                &state, reinterpret_cast<const unsigned char*>(ownerBytes.data()), 32);
            crypto_hash_sha256_update(
                &state, reinterpret_cast<const unsigned char*>(programBytes.data()), 32);
            crypto_hash_sha256_update(&state,
                                      reinterpret_cast<const unsigned char*>(mintBytes.data()), 32);
            unsigned char bumpByte = static_cast<unsigned char>(bump);
            crypto_hash_sha256_update(&state, &bumpByte, 1);
            crypto_hash_sha256_update(
                &state, reinterpret_cast<const unsigned char*>(ataProgramBytes.data()), 32);
            const char* pda = "ProgramDerivedAddress";
            crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(pda), 21);
            crypto_hash_sha256_final(&state, hash);

            // Valid PDA must NOT be on the ed25519 curve
            if (!isOnEd25519Curve(hash)) {
                return Base58::encode(QByteArray(reinterpret_cast<const char*>(hash), 32));
            }
        }
        return {};
    }

} // namespace AssociatedTokenInstruction
