#ifndef TREZORTRANSPORT_H
#define TREZORTRANSPORT_H

#include <QByteArray>
#include <QString>
#include <cstdint>

struct TrezorResponse {
    uint16_t msgType = 0;
    QByteArray data;
    bool isValid() const { return msgType != 0 || !data.isEmpty(); }
};

// ── Abstract base class for Trezor transports ──────────────
// Both HID (Trezor One) and USB/WebUSB (Safe 3, Model T) implement
// the same wire protocol (V1: '?##' framing) over different USB backends.
// The shared framing is implemented here; subclasses only provide raw packet I/O.

class TrezorTransport {
  public:
    TrezorTransport() = default;
    virtual ~TrezorTransport() = default;

    TrezorTransport(const TrezorTransport&) = delete;
    TrezorTransport& operator=(const TrezorTransport&) = delete;

    virtual bool open(const QString& devicePath) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Send a protobuf message and read the response (shared V1 wire framing).
    TrezorResponse call(uint16_t msgType, const QByteArray& protobuf, int timeoutMs = 30000);

    QString lastError() const { return m_lastError; }

  protected:
    // Subclass raw packet I/O — only thing that differs between HID and USB.
    virtual bool writePacket(const uint8_t* data, int size) = 0;
    virtual int readPacket(uint8_t* buf, int bufSize, int timeoutMs) = 0;

    // Packet geometry — subclasses override to specify sizes.
    virtual int writePacketSize() const = 0; // 64 for USB, 65 for HID
    virtual int readPacketSize() const = 0;  // 64 for both
    virtual int writeDataOffset() const = 0; // 1 for USB (marker), 2 for HID (reportId + marker)

    QString m_lastError;

  private:
    bool sendMessage(uint16_t msgType, const QByteArray& data);
    TrezorResponse readMessage(int timeoutMs);
};

#endif // TREZORTRANSPORT_H
