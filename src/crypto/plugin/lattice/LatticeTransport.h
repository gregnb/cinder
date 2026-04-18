#ifndef LATTICETRANSPORT_H
#define LATTICETRANSPORT_H

#include "crypto/plugin/lattice/LatticeCrypto.h"

#include <QByteArray>
#include <QString>

class QNetworkAccessManager;

namespace LatticeWire {
    constexpr uint8_t kProtocolVersion = 0x01;
    constexpr uint8_t kMsgTypeConnect = 0x01;
    constexpr uint8_t kMsgTypeEncrypted = 0x02;
    constexpr int kHeaderSize = 8; // version(1) + type(1) + msgId(4) + payloadLen(2)
    constexpr int kChecksumSize = 4;
    constexpr int kConnectDataSize = 65; // uncompressed P-256 pubkey
    constexpr int kEphemeralPubSize = 65;
    constexpr int kEphemeralIdSize = 4;
} // namespace LatticeWire

namespace LatticeRequestType {
    constexpr uint8_t kFinalizePairing = 0x00;
    constexpr uint8_t kGetAddresses = 0x01;
    constexpr uint8_t kSign = 0x03;
    constexpr uint8_t kGetWallets = 0x04;
} // namespace LatticeRequestType

namespace LatticeTimeout {
    constexpr int kConnect = 15000;
    constexpr int kPair = 30000;
    constexpr int kGetAddress = 15000;
    constexpr int kSign = 60000;
} // namespace LatticeTimeout

namespace LatticeAddressFlag {
    constexpr uint8_t kNone = 0;
    constexpr uint8_t kEd25519Pubkey = 4;
} // namespace LatticeAddressFlag

struct LatticeResponse {
    uint8_t responseCode = 0;
    QByteArray payload;
    bool valid = false;
    bool isValid() const { return valid && responseCode == 0x00; }
};

class LatticeTransport {
  public:
    LatticeTransport();
    ~LatticeTransport();

    // Open connection to relay server for a given device
    bool open(const QString& deviceId);
    void close();
    bool isOpen() const;

    // Phase 1: ECDH key exchange (unencrypted)
    bool connect(const QString& appName);
    bool isPaired() const;

    // Phase 2: Pairing with 6-digit code
    bool pair(const QString& pairingCode, const QString& appName);

    // Phase 3+: Encrypted request/response
    LatticeResponse request(uint8_t requestType, const QByteArray& payload, int timeoutMs);

    // Get the active wallet UID (needed for address/sign requests)
    QByteArray walletUID() const;

    QString lastError() const;

    // Persistent state for session restoration
    void setStoredKeyPair(const LatticeCrypto::EcKeyPair& keypair);
    LatticeCrypto::EcKeyPair storedKeyPair() const;
    void setStoredSharedSecret(const QByteArray& secret);
    void setStoredEphemeralPub(const QByteArray& pub);

  private:
    // HTTP POST to relay
    QByteArray httpPost(const QByteArray& wireFrame, int timeoutMs);

    // Wire framing
    QByteArray frameMessage(uint8_t msgType, const QByteArray& payload);
    LatticeResponse parseResponse(const QByteArray& raw);

    // Encryption
    QByteArray encryptPayload(uint8_t requestType, const QByteArray& data);
    QByteArray decryptPayload(const QByteArray& encrypted);

    QString m_baseUrl;
    QString m_deviceId;
    QString m_lastError;
    bool m_connected = false;
    bool m_paired = false;

    LatticeCrypto::EcKeyPair m_appKey;
    QByteArray m_sharedSecret;
    QByteArray m_deviceEphemeralPub;
    QByteArray m_walletUID;
    uint32_t m_ephemeralId = 0;

    QNetworkAccessManager* m_network = nullptr;
};

#endif // LATTICETRANSPORT_H
