#ifndef PBKDF2_H
#define PBKDF2_H

#include <QByteArray>
#include <sodium.h>

namespace Pbkdf2 {

    // PBKDF2-HMAC-SHA512 as specified by RFC 2898 / BIP39.
    // Returns `dkLen` bytes of derived key material.
    inline QByteArray hmacSha512(const QByteArray& password, const QByteArray& salt, int iterations,
                                 int dkLen = 64) {
        QByteArray result(dkLen, '\0');
        int hLen = crypto_auth_hmacsha512_BYTES; // 64

        int blocks = (dkLen + hLen - 1) / hLen;

        for (int block = 1; block <= blocks; ++block) {
            // U_1 = PRF(password, salt || INT_32_BE(block))
            QByteArray saltBlock = salt;
            unsigned char be[4];
            be[0] = static_cast<unsigned char>((block >> 24) & 0xFF);
            be[1] = static_cast<unsigned char>((block >> 16) & 0xFF);
            be[2] = static_cast<unsigned char>((block >> 8) & 0xFF);
            be[3] = static_cast<unsigned char>(block & 0xFF);
            saltBlock.append(reinterpret_cast<const char*>(be), 4);

            // Initialize HMAC state with password as key
            crypto_auth_hmacsha512_state state;
            unsigned char u[crypto_auth_hmacsha512_BYTES];
            unsigned char t[crypto_auth_hmacsha512_BYTES];

            // U_1
            crypto_auth_hmacsha512_init(
                &state, reinterpret_cast<const unsigned char*>(password.constData()),
                static_cast<size_t>(password.size()));
            crypto_auth_hmacsha512_update(
                &state, reinterpret_cast<const unsigned char*>(saltBlock.constData()),
                static_cast<size_t>(saltBlock.size()));
            crypto_auth_hmacsha512_final(&state, u);

            memcpy(t, u, hLen);

            // U_2 .. U_c
            for (int i = 1; i < iterations; ++i) {
                crypto_auth_hmacsha512_init(
                    &state, reinterpret_cast<const unsigned char*>(password.constData()),
                    static_cast<size_t>(password.size()));
                crypto_auth_hmacsha512_update(&state, u, hLen);
                crypto_auth_hmacsha512_final(&state, u);

                for (int j = 0; j < hLen; ++j)
                    t[j] ^= u[j];
            }

            // Copy this block's output into result
            int offset = (block - 1) * hLen;
            int copyLen = qMin(hLen, dkLen - offset);
            memcpy(result.data() + offset, t, copyLen);

            sodium_memzero(u, sizeof(u));
            sodium_memzero(t, sizeof(t));
            sodium_memzero(&state, sizeof(state));
            sodium_memzero(saltBlock.data(), static_cast<size_t>(saltBlock.size()));
        }

        return result;
    }

} // namespace Pbkdf2

#endif // PBKDF2_H
