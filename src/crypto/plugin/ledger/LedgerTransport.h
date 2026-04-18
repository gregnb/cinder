#ifndef LEDGERTRANSPORT_H
#define LEDGERTRANSPORT_H

#include <QByteArray>
#include <QString>
#include <cstdint>

struct hid_device_;
typedef struct hid_device_ hid_device;

// ── Ledger wire protocol constants ──────────────────────────────
namespace LedgerWire {
    constexpr uint16_t kChannel = 0x0101;
    constexpr uint8_t kTagApdu = 0x05;
    constexpr int kPacketSize = 64;
    // First packet: channel(2) + tag(1) + seq(2) + len(2) = 7 header bytes
    constexpr int kFirstPayload = kPacketSize - 7; // 57
    // Continuation: channel(2) + tag(1) + seq(2) = 5 header bytes
    constexpr int kContPayload = kPacketSize - 5; // 59
} // namespace LedgerWire

// ── Ledger APDU constants ───────────────────────────────────────
namespace LedgerApdu {
    constexpr uint8_t kCLA = 0xE0;
    constexpr uint8_t kINS_GET_APP_CONFIG = 0x04;
    constexpr uint8_t kINS_GET_PUBKEY = 0x05;
    constexpr uint8_t kINS_SIGN = 0x06;
    constexpr uint8_t kP2_LAST = 0x00;
    constexpr uint8_t kP2_MORE = 0x02;
    // Max message bytes in first sign APDU: 255 - 1(signer count) - path bytes
    constexpr int kFirstChunkMax = 237; // 255 - 1 - 17
    constexpr int kContChunkMax = 255;
} // namespace LedgerApdu

// ── Ledger crypto constants ─────────────────────────────────────
namespace LedgerCrypto {
    constexpr int kEd25519KeySize = 32;
    constexpr int kEd25519SignatureSize = 64;
    constexpr uint32_t kHardenedBit = 0x80000000;
} // namespace LedgerCrypto

// ── Ledger timeout constants ────────────────────────────────────
namespace LedgerTimeout {
    constexpr int kExchange = 30000;
    constexpr int kGetPubkey = 10000;
    constexpr int kSign = 60000;
} // namespace LedgerTimeout

class LedgerTransport {
  public:
    LedgerTransport();
    virtual ~LedgerTransport();

    LedgerTransport(const LedgerTransport&) = delete;
    LedgerTransport& operator=(const LedgerTransport&) = delete;

    bool open(const QString& devicePath);
    void close();
    bool isOpen() const;

    // Send APDU and read response. Returns response data (without status word)
    // on success, or empty QByteArray on failure. Check lastError()/lastStatusWord().
    QByteArray exchange(const QByteArray& apdu, int timeoutMs = LedgerTimeout::kExchange);

    QString lastError() const { return m_lastError; }
    uint16_t lastStatusWord() const { return m_statusWord; }

    static QString statusWordMessage(uint16_t sw);

  protected:
    // ── Subclass-overridable raw I/O (for testing) ──────────
    virtual int writePacket(const uint8_t* data, int size);
    virtual int readPacket(uint8_t* buf, int bufSize, int timeoutMs);

    QString m_lastError;
    uint16_t m_statusWord = 0;

  private:
    bool sendFrames(const QByteArray& apdu);
    QByteArray readFrames(int timeoutMs);

    hid_device* m_device = nullptr;
};

#endif // LEDGERTRANSPORT_H
