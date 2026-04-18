#include "crypto/plugin/lattice/LatticeCrypto.h"

#include <QDebug>
#include <QRandomGenerator>

#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonCrypto.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/Security.h>

namespace LatticeCrypto {

// ── P-256 keypair generation ──────────────────────────────────────

EcKeyPair generateP256Keypair() {
    EcKeyPair result;

    NSDictionary* attributes = @{
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits : @256,
    };

    CFErrorRef error = nullptr;
    SecKeyRef privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)attributes, &error);
    if (!privateKey) {
        if (error) {
            qWarning() << "[LatticeCrypto] P-256 keygen failed:"
                       << QString::fromCFString(CFErrorCopyDescription(error));
            CFRelease(error);
        }
        return result;
    }

    // Extract raw private key bytes
    CFDataRef privData = SecKeyCopyExternalRepresentation(privateKey, &error);
    if (!privData) {
        CFRelease(privateKey);
        return result;
    }

    // SEC1 format for P-256: 04 || x(32) || y(32) || d(32) = 97 bytes
    const uint8_t* bytes = CFDataGetBytePtr(privData);
    CFIndex len = CFDataGetLength(privData);
    if (len >= 97) {
        result.publicKey = QByteArray(reinterpret_cast<const char*>(bytes), 65);
        result.privateKey = QByteArray(reinterpret_cast<const char*>(bytes + 65), 32);
    }

    CFRelease(privData);
    CFRelease(privateKey);
    return result;
}

// ── ECDH shared secret ───────────────────────────────────────────

QByteArray deriveSharedSecret(const QByteArray& ourPrivKey, const QByteArray& theirPub65) {
    if (ourPrivKey.size() != kP256PrivKeySize || theirPub65.size() != kP256PubKeySize) {
        return {};
    }

    // Reconstruct our full SEC1 key (pub + priv) for SecKey import
    // We need to regenerate the public key from private, or store it alongside.
    // For simplicity, we rebuild the private SecKey from raw bytes.

    // Build SEC1 DER for the private key
    // Actually, the approach: import the peer's public key, then use SecKeyCopyKeyExchangeResult
    // But SecKey requires importing from external representation.

    // Import peer's public key
    NSDictionary* pubAttrs = @{
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass : (id)kSecAttrKeyClassPublic,
        (id)kSecAttrKeySizeInBits : @256,
    };

    CFDataRef pubData =
        CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const uint8_t*>(theirPub65.constData()),
                     theirPub65.size());
    CFErrorRef error = nullptr;
    SecKeyRef peerKey = SecKeyCreateWithData(pubData, (__bridge CFDictionaryRef)pubAttrs, &error);
    CFRelease(pubData);

    if (!peerKey) {
        if (error) {
            qWarning() << "[LatticeCrypto] Failed to import peer pubkey";
            CFRelease(error);
        }
        return {};
    }

    // We need our private SecKey. Reconstruct SEC1 format: 04 || x || y || d
    // Problem: we only stored d (32 bytes). We need to derive the public point.
    // Alternative: store the full 97-byte SEC1 blob in EcKeyPair and use it here.
    // For now, this function expects the caller to pass the private key that was part of the
    // original EcKeyPair, and we reconstruct by re-importing.

    // Build a temp keypair SEC1 blob: we stored privKey(32), need to prepend pubKey(65).
    // The caller should pass the full EcKeyPair. Let's adjust the API to accept both.
    // Actually let's use a different approach: create the key from raw d value.

    // Import private key from SEC1 external representation
    // We need the full 97-byte blob. This means we must store pubKey alongside privKey.
    // Let's store both in the transport and pass the full blob here.

    // For now, return empty — this needs the full keypair
    CFRelease(peerKey);
    qWarning() << "[LatticeCrypto] deriveSharedSecret needs full keypair — use "
                  "deriveSharedSecretFromKeypair instead";
    return {};
}

// Full version that takes the complete EcKeyPair
static QByteArray deriveSharedSecretImpl(const EcKeyPair& ourKey, const QByteArray& theirPub65) {
    if (!ourKey.isValid() || theirPub65.size() != kP256PubKeySize) {
        return {};
    }

    // Reconstruct SEC1 blob: pubKey(65) + privKey(32) = 97 bytes
    QByteArray sec1 = ourKey.publicKey + ourKey.privateKey;

    NSDictionary* privAttrs = @{
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass : (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits : @256,
    };

    CFDataRef privData = CFDataCreate(kCFAllocatorDefault,
                                      reinterpret_cast<const uint8_t*>(sec1.constData()),
                                      sec1.size());
    CFErrorRef error = nullptr;
    SecKeyRef ourSecKey = SecKeyCreateWithData(privData, (__bridge CFDictionaryRef)privAttrs, &error);
    CFRelease(privData);

    if (!ourSecKey) {
        if (error) {
            qWarning() << "[LatticeCrypto] Failed to import our private key";
            CFRelease(error);
        }
        return {};
    }

    // Import peer public key
    NSDictionary* pubAttrs = @{
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass : (id)kSecAttrKeyClassPublic,
        (id)kSecAttrKeySizeInBits : @256,
    };

    CFDataRef peerData =
        CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const uint8_t*>(theirPub65.constData()),
                     theirPub65.size());
    SecKeyRef peerKey = SecKeyCreateWithData(peerData, (__bridge CFDictionaryRef)pubAttrs, &error);
    CFRelease(peerData);

    if (!peerKey) {
        CFRelease(ourSecKey);
        return {};
    }

    // Perform ECDH
    NSDictionary* params = @{};
    CFDataRef sharedSecret = SecKeyCopyKeyExchangeResult(
        ourSecKey, kSecKeyAlgorithmECDHKeyExchangeStandard, peerKey,
        (__bridge CFDictionaryRef)params, &error);

    CFRelease(ourSecKey);
    CFRelease(peerKey);

    if (!sharedSecret) {
        if (error) {
            qWarning() << "[LatticeCrypto] ECDH failed";
            CFRelease(error);
        }
        return {};
    }

    QByteArray result(reinterpret_cast<const char*>(CFDataGetBytePtr(sharedSecret)),
                      static_cast<int>(CFDataGetLength(sharedSecret)));
    CFRelease(sharedSecret);

    // Truncate or hash to 32 bytes if needed
    if (result.size() > kSharedSecretSize) {
        result = sha256(result);
    }

    return result;
}

// ── AES-256-CBC ──────────────────────────────────────────────────

QByteArray aesCbcEncrypt(const QByteArray& plaintext, const QByteArray& key32) {
    if (key32.size() != kAesKeySize) {
        return {};
    }

    // Generate random IV
    QByteArray iv(kAesIvSize, 0);
    QRandomGenerator::global()->fillRange(reinterpret_cast<uint32_t*>(iv.data()),
                                          kAesIvSize / sizeof(uint32_t));

    // PKCS7 padding is handled by CommonCrypto with kCCOptionPKCS7Padding
    size_t outLen = 0;
    QByteArray ciphertext(plaintext.size() + kCCBlockSizeAES128, 0);

    CCCryptorStatus status = CCCrypt(
        kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
        key32.constData(), kAesKeySize,
        iv.constData(),
        plaintext.constData(), plaintext.size(),
        ciphertext.data(), ciphertext.size(),
        &outLen);

    if (status != kCCSuccess) {
        qWarning() << "[LatticeCrypto] AES encrypt failed:" << status;
        return {};
    }

    ciphertext.resize(static_cast<int>(outLen));
    return iv + ciphertext; // prepend IV
}

QByteArray aesCbcDecrypt(const QByteArray& ciphertext, const QByteArray& key32) {
    if (key32.size() != kAesKeySize || ciphertext.size() <= kAesIvSize) {
        return {};
    }

    QByteArray iv = ciphertext.left(kAesIvSize);
    QByteArray encrypted = ciphertext.mid(kAesIvSize);

    size_t outLen = 0;
    QByteArray plaintext(encrypted.size(), 0);

    CCCryptorStatus status = CCCrypt(
        kCCDecrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
        key32.constData(), kAesKeySize,
        iv.constData(),
        encrypted.constData(), encrypted.size(),
        plaintext.data(), plaintext.size(),
        &outLen);

    if (status != kCCSuccess) {
        qWarning() << "[LatticeCrypto] AES decrypt failed:" << status;
        return {};
    }

    plaintext.resize(static_cast<int>(outLen));
    return plaintext;
}

// ── SHA-256 ──────────────────────────────────────────────────────

QByteArray sha256(const QByteArray& data) {
    QByteArray hash(CC_SHA256_DIGEST_LENGTH, 0);
    CC_SHA256(data.constData(), static_cast<CC_LONG>(data.size()),
              reinterpret_cast<unsigned char*>(hash.data()));
    return hash;
}

// ── CRC32 ────────────────────────────────────────────────────────

uint32_t crc32(const QByteArray& data) {
    static const uint32_t table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
        0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91B, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBE, 0xE7B82D09, 0x90BF1D9F, 0x1DB71064, 0x6AB020F2,
        0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
        0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F0B5, 0x56B3C423,
        0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
        0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
        0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
        0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
        0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7822, 0x3B6E20C8, 0x4C69105E,
        0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75,
        0xDCD60DCF, 0xABD13D59,
    };

    uint32_t crc = 0xFFFFFFFF;
    const auto* bytes = reinterpret_cast<const uint8_t*>(data.constData());
    for (int i = 0; i < data.size(); ++i) {
        crc = (crc >> 8) ^ table[(crc ^ bytes[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// ── ECDSA P-256 sign ─────────────────────────────────────────────

QByteArray ecdsaSignDer(const QByteArray& hash32, const QByteArray& privKey32) {
    // Reconstruct private key — we need the full EcKeyPair for this.
    // This is a simplified version that requires the caller to manage key state.
    // In practice, the LatticeTransport stores the full EcKeyPair.
    Q_UNUSED(hash32);
    Q_UNUSED(privKey32);
    qWarning() << "[LatticeCrypto] ecdsaSignDer: use ecdsaSignDerWithKeypair instead";
    return {};
}

// Full version with keypair
static QByteArray ecdsaSignDerImpl(const QByteArray& hash32, const EcKeyPair& keypair) {
    if (hash32.size() != 32 || !keypair.isValid()) {
        return {};
    }

    QByteArray sec1 = keypair.publicKey + keypair.privateKey;
    NSDictionary* attrs = @{
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeyClass : (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits : @256,
    };

    CFDataRef keyData = CFDataCreate(kCFAllocatorDefault,
                                     reinterpret_cast<const uint8_t*>(sec1.constData()),
                                     sec1.size());
    CFErrorRef error = nullptr;
    SecKeyRef secKey = SecKeyCreateWithData(keyData, (__bridge CFDictionaryRef)attrs, &error);
    CFRelease(keyData);

    if (!secKey) {
        return {};
    }

    CFDataRef hashData = CFDataCreate(kCFAllocatorDefault,
                                      reinterpret_cast<const uint8_t*>(hash32.constData()),
                                      hash32.size());
    CFDataRef sigData = SecKeyCreateSignature(secKey, kSecKeyAlgorithmECDSASignatureDigestX962SHA256,
                                              hashData, &error);
    CFRelease(hashData);
    CFRelease(secKey);

    if (!sigData) {
        if (error) {
            CFRelease(error);
        }
        return {};
    }

    QByteArray result(reinterpret_cast<const char*>(CFDataGetBytePtr(sigData)),
                      static_cast<int>(CFDataGetLength(sigData)));
    CFRelease(sigData);
    return result;
}

// ── Pairing hash ─────────────────────────────────────────────────

QByteArray generatePairingHash(const QString& deviceId, const QString& pairingCode,
                               const QString& appName) {
    QByteArray preimage;
    preimage.append(deviceId.toUtf8());
    preimage.append(pairingCode.toUtf8());

    // App name is padded/truncated to 25 bytes (matching SDK behavior)
    QByteArray nameBytes = appName.toUtf8().left(25);
    nameBytes.append(QByteArray(25 - nameBytes.size(), 0));
    preimage.append(nameBytes);

    return sha256(preimage);
}

} // namespace LatticeCrypto
