#ifndef MOCKLEDGERTRANSPORT_H
#define MOCKLEDGERTRANSPORT_H

#include "crypto/plugin/ledger/LedgerTransport.h"

#include <QByteArray>
#include <QList>
#include <cstring>

// Mock transport that captures written packets and returns canned responses.
// Used by test_ledger_transport.cpp to test the Ledger APDU framing
// without any USB/HID hardware.

class MockLedgerTransport : public LedgerTransport {
  public:
    MockLedgerTransport() = default;

    // Override open/close/isOpen to avoid actual HID interaction
    bool open(const QString&) {
        m_open = true;
        m_lastError.clear();
        return true;
    }
    void close() { m_open = false; }
    bool isOpen() const { return m_open; }

    // ── Test setup ───────────────────────────────────────

    // Queue a canned response with the given data and status word.
    // The response is framed as proper Ledger HID packets.
    void enqueueResponse(const QByteArray& responseData, uint16_t statusWord = 0x9000) {
        // Build full response: data + SW (2 bytes, big-endian)
        QByteArray fullResp = responseData;
        fullResp.append(static_cast<char>((statusWord >> 8) & 0xFF));
        fullResp.append(static_cast<char>(statusWord & 0xFF));

        // Frame into HID packets
        int offset = 0;
        uint16_t seq = 0;
        int totalLen = fullResp.size();

        while (offset < totalLen) {
            QByteArray pkt(LedgerWire::kPacketSize, '\0');
            int pos = 0;

            // Channel (big-endian)
            pkt[pos++] = static_cast<char>((LedgerWire::kChannel >> 8) & 0xFF);
            pkt[pos++] = static_cast<char>(LedgerWire::kChannel & 0xFF);
            // Tag
            pkt[pos++] = static_cast<char>(LedgerWire::kTagApdu);
            // Sequence (big-endian)
            pkt[pos++] = static_cast<char>((seq >> 8) & 0xFF);
            pkt[pos++] = static_cast<char>(seq & 0xFF);

            int payloadCapacity;
            if (seq == 0) {
                // First packet includes total length
                pkt[pos++] = static_cast<char>((totalLen >> 8) & 0xFF);
                pkt[pos++] = static_cast<char>(totalLen & 0xFF);
                payloadCapacity = LedgerWire::kFirstPayload;
            } else {
                payloadCapacity = LedgerWire::kContPayload;
            }

            int chunkSize = qMin(payloadCapacity, totalLen - offset);
            std::memcpy(pkt.data() + pos, fullResp.constData() + offset, chunkSize);

            m_readQueue.append(pkt);
            offset += chunkSize;
            seq++;
        }
    }

    // Queue an error response (no data, just status word)
    void enqueueError(uint16_t statusWord) { enqueueResponse(QByteArray(), statusWord); }

    // ── Inspection ───────────────────────────────────────

    QList<QByteArray> writtenPackets() const { return m_writtenPackets; }
    void clearWrittenPackets() { m_writtenPackets.clear(); }

    // Parse the APDU from written packets by reassembling and stripping framing.
    QByteArray reassembleWrittenApdu() const {
        QByteArray result;
        int totalLen = 0;

        for (int i = 0; i < m_writtenPackets.size(); ++i) {
            const QByteArray& pkt = m_writtenPackets[i];
            int pos = 5; // skip channel(2) + tag(1) + seq(2)

            if (i == 0) {
                // First packet has APDU length
                totalLen =
                    ((static_cast<uint8_t>(pkt[pos]) << 8) | static_cast<uint8_t>(pkt[pos + 1]));
                pos += 2;
            }

            int payloadCap = (i == 0) ? LedgerWire::kFirstPayload : LedgerWire::kContPayload;
            int remaining = totalLen - result.size();
            int chunkSize = qMin(payloadCap, remaining);
            if (pos + chunkSize <= pkt.size()) {
                result.append(pkt.mid(pos, chunkSize));
            }
        }

        return result;
    }

    // Simulate a timeout (readPacket returns 0 = timeout)
    void setSimulateTimeout(bool timeout) { m_simulateTimeout = timeout; }

    // Simulate a read error (readPacket returns -1)
    void setSimulateReadError(bool error) { m_simulateReadError = error; }

  protected:
    int writePacket(const uint8_t* data, int size) override {
        if (!m_open) {
            return -1;
        }
        m_writtenPackets.append(QByteArray(reinterpret_cast<const char*>(data), size));
        return size;
    }

    int readPacket(uint8_t* buf, int bufSize, int) override {
        if (m_simulateReadError) {
            m_lastError = QStringLiteral("Simulated read error");
            return -1;
        }
        if (m_simulateTimeout) {
            return 0; // timeout
        }
        if (m_readQueue.isEmpty()) {
            m_lastError = QStringLiteral("No more canned responses");
            return -1;
        }
        QByteArray pkt = m_readQueue.takeFirst();
        int copySize = qMin(static_cast<int>(pkt.size()), bufSize);
        std::memcpy(buf, pkt.constData(), copySize);
        return copySize;
    }

  private:
    bool m_open = true;
    bool m_simulateTimeout = false;
    bool m_simulateReadError = false;
    QList<QByteArray> m_writtenPackets;
    QList<QByteArray> m_readQueue;
};

#endif // MOCKLEDGERTRANSPORT_H
