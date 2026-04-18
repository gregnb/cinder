#ifndef MOCKTREZORTRANSPORT_H
#define MOCKTREZORTRANSPORT_H

#include "crypto/plugin/trezor/TrezorProtobuf.h"
#include "crypto/plugin/trezor/TrezorTransport.h"

#include <QByteArray>
#include <QList>
#include <cstring>

// Mock transport that captures written packets and returns canned responses.
// Used by test_trezor_wire_framing.cpp to test the shared V1 wire framing
// in TrezorTransport without any USB/HID hardware.

class MockTrezorTransport : public TrezorTransport {
  public:
    MockTrezorTransport() = default;

    bool open(const QString&) override {
        m_open = true;
        m_lastError.clear();
        return true;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }

    // ── Test setup ───────────────────────────────────────
    // Queue a canned response that readPacket will return.
    // The response is framed as a proper V1 wire packet (with ?## header).
    void enqueueResponse(uint16_t msgType, const QByteArray& protobufData) {
        // Build the wire-framed packets for this response
        QByteArray header;
        header.append(static_cast<char>(TrezorWire::kMarker));
        header.append(TrezorWire::kHeaderMagic0);
        header.append(TrezorWire::kHeaderMagic1);
        header.append(static_cast<char>((msgType >> 8) & 0xFF));
        header.append(static_cast<char>(msgType & 0xFF));
        uint32_t dataLen = static_cast<uint32_t>(protobufData.size());
        header.append(static_cast<char>((dataLen >> 24) & 0xFF));
        header.append(static_cast<char>((dataLen >> 16) & 0xFF));
        header.append(static_cast<char>((dataLen >> 8) & 0xFF));
        header.append(static_cast<char>(dataLen & 0xFF));

        QByteArray payload = header + protobufData;
        int offset = 0;

        // First packet: already has marker in the header we built
        QByteArray pkt(TrezorWire::kUsbPacketSize, '\0');
        int firstChunk = qMin(payload.size(), TrezorWire::kUsbPacketSize);
        std::memcpy(pkt.data(), payload.constData(), firstChunk);
        m_readQueue.append(pkt);
        offset = firstChunk;

        // Continuation packets
        while (offset < payload.size()) {
            QByteArray cont(TrezorWire::kUsbPacketSize, '\0');
            cont[0] = static_cast<char>(TrezorWire::kMarker);
            int chunkSize = qMin(TrezorWire::kPayloadPerPacket, payload.size() - offset);
            std::memcpy(cont.data() + 1, payload.constData() + offset, chunkSize);
            m_readQueue.append(cont);
            offset += chunkSize;
        }
    }

    // ── Inspection ───────────────────────────────────────
    QList<QByteArray> writtenPackets() const { return m_writtenPackets; }
    void clearWrittenPackets() { m_writtenPackets.clear(); }

    // Extract the wire header from the first written packet.
    // Returns {msgType, dataLen} or {0, 0} on failure.
    struct WireHeader {
        uint16_t msgType = 0;
        uint32_t dataLen = 0;
    };
    WireHeader parseFirstWrittenHeader() const {
        if (m_writtenPackets.isEmpty())
            return {};
        const QByteArray& pkt = m_writtenPackets.first();
        // USB packets: [0x3F] [##] [type2] [len4] [payload...]
        if (pkt.size() < 9)
            return {};
        if (static_cast<uint8_t>(pkt[0]) != TrezorWire::kMarker)
            return {};
        if (pkt[1] != TrezorWire::kHeaderMagic0 || pkt[2] != TrezorWire::kHeaderMagic1)
            return {};
        WireHeader h;
        h.msgType = static_cast<uint16_t>((static_cast<uint8_t>(pkt[3]) << 8) |
                                          static_cast<uint8_t>(pkt[4]));
        h.dataLen = (static_cast<uint32_t>(static_cast<uint8_t>(pkt[5])) << 24) |
                    (static_cast<uint32_t>(static_cast<uint8_t>(pkt[6])) << 16) |
                    (static_cast<uint32_t>(static_cast<uint8_t>(pkt[7])) << 8) |
                    static_cast<uint32_t>(static_cast<uint8_t>(pkt[8]));
        return h;
    }

    // Reassemble all written payload bytes (strip marker + header framing).
    QByteArray reassembleWrittenPayload() const {
        QByteArray result;
        for (int i = 0; i < m_writtenPackets.size(); ++i) {
            const QByteArray& pkt = m_writtenPackets[i];
            if (i == 0) {
                // First packet: skip marker(1) + header(8)
                if (pkt.size() > 9)
                    result.append(pkt.mid(9));
            } else {
                // Continuation: skip marker(1)
                if (pkt.size() > 1)
                    result.append(pkt.mid(1));
            }
        }
        return result;
    }

  protected:
    bool writePacket(const uint8_t* data, int size) override {
        m_writtenPackets.append(QByteArray(reinterpret_cast<const char*>(data), size));
        return true;
    }

    int readPacket(uint8_t* buf, int bufSize, int) override {
        if (m_readQueue.isEmpty()) {
            m_lastError = QStringLiteral("No more canned responses");
            return -1;
        }
        QByteArray pkt = m_readQueue.takeFirst();
        int copySize = qMin(pkt.size(), bufSize);
        std::memcpy(buf, pkt.constData(), copySize);
        return copySize;
    }

    int writePacketSize() const override { return TrezorWire::kUsbPacketSize; }
    int readPacketSize() const override { return TrezorWire::kUsbPacketSize; }
    int writeDataOffset() const override { return 1; } // USB-style: marker only

  private:
    bool m_open = false;
    QList<QByteArray> m_writtenPackets;
    QList<QByteArray> m_readQueue;
};

#endif // MOCKTREZORTRANSPORT_H
