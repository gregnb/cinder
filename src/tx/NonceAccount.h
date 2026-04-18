#ifndef NONCEACCOUNT_H
#define NONCEACCOUNT_H

#include "tx/Base58.h"
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QtEndian>

// Nonce account data layout (80 bytes):
//   [0..4)   version    u32 LE
//   [4..8)   state      u32 LE  (0=Uninitialized, 1=Initialized)
//   [8..40)  authority  32 bytes (pubkey)
//   [40..72) nonce      32 bytes (durable nonce value, base58-like)
//   [72..80) fee        u64 LE  (lamportsPerSignature at time of storage)

struct NonceAccount {
    quint32 version = 0;
    quint32 state = 0; // 0 = Uninitialized, 1 = Initialized
    QString authority; // nonce authority pubkey
    QString nonce;     // the durable nonce value (used as recentBlockhash)
    quint64 lamportsPerSignature = 0;

    bool isInitialized() const { return state == 1; }

    // Parse from raw account data (80 bytes, base64-decoded)
    static NonceAccount fromAccountData(const QByteArray& data) {
        NonceAccount na;
        if (data.size() < 80)
            return na;

        const auto* raw = reinterpret_cast<const uchar*>(data.constData());

        na.version = qFromLittleEndian<quint32>(raw);
        na.state = qFromLittleEndian<quint32>(raw + 4);
        na.authority = Base58::encode(data.mid(8, 32));
        na.nonce = Base58::encode(data.mid(40, 32));
        na.lamportsPerSignature = qFromLittleEndian<quint64>(raw + 72);

        return na;
    }
};

Q_DECLARE_METATYPE(NonceAccount)

#endif // NONCEACCOUNT_H
