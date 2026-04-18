#include "crypto/plugin/trezor/TrezorTransport.h"
#include "crypto/plugin/trezor/TrezorProtobuf.h"

#include <QDebug>
#include <cstring>

TrezorResponse TrezorTransport::call(uint16_t msgType, const QByteArray& protobuf, int timeoutMs) {
    m_lastError.clear();

    if (!isOpen()) {
        m_lastError = QStringLiteral("Device not open");
        return {};
    }

    if (!sendMessage(msgType, protobuf)) {
        return {};
    }

    return readMessage(timeoutMs);
}

bool TrezorTransport::sendMessage(uint16_t msgType, const QByteArray& data) {
    // Wire protocol V1 header: "##" + msgType(2 BE) + dataLen(4 BE)
    QByteArray header;
    header.reserve(TrezorWire::kHeaderSize);
    header.append(TrezorWire::kHeaderMagic0);
    header.append(TrezorWire::kHeaderMagic1);
    header.append(static_cast<char>((msgType >> 8) & 0xFF));
    header.append(static_cast<char>(msgType & 0xFF));
    uint32_t dataLen = static_cast<uint32_t>(data.size());
    header.append(static_cast<char>((dataLen >> 24) & 0xFF));
    header.append(static_cast<char>((dataLen >> 16) & 0xFF));
    header.append(static_cast<char>((dataLen >> 8) & 0xFF));
    header.append(static_cast<char>(dataLen & 0xFF));

    QByteArray payload = header + data;
    int offset = 0;
    const int pktSize = writePacketSize();
    const int dataOff = writeDataOffset();
    // Payload capacity per packet: pktSize - dataOff
    const int payloadCapacity = pktSize - dataOff;

    while (offset < payload.size() || offset == 0) {
        auto packet = std::make_unique<uint8_t[]>(pktSize);
        std::memset(packet.get(), 0, pktSize);

        // Write marker (and report ID for HID)
        if (dataOff == 2) {
            // HID: report ID + marker
            packet[0] = TrezorWire::kHidReportId;
            packet[1] = TrezorWire::kMarker;
        } else {
            // USB: just marker
            packet[0] = TrezorWire::kMarker;
        }

        int chunkSize = qMin(payloadCapacity, payload.size() - offset);
        if (chunkSize > 0) {
            std::memcpy(packet.get() + dataOff, payload.constData() + offset, chunkSize);
        }

        if (!writePacket(packet.get(), pktSize)) {
            return false;
        }

        offset += chunkSize;
    }

    return true;
}

TrezorResponse TrezorTransport::readMessage(int timeoutMs) {
    TrezorResponse response;
    const int pktSize = readPacketSize();

    auto packet = std::make_unique<uint8_t[]>(pktSize);
    int bytesRead = readPacket(packet.get(), pktSize, timeoutMs);

    if (bytesRead <= 0) {
        if (m_lastError.isEmpty()) {
            m_lastError = bytesRead < 0 ? QStringLiteral("USB read failed")
                                        : QStringLiteral("Device read timed out");
        }
        return {};
    }

    // Parse header: '?' + "##" + type(2) + length(4)
    int pos = 0;
    if (packet[pos] != TrezorWire::kMarker) {
        m_lastError = QStringLiteral("Invalid response: missing '?' marker (got 0x%1)")
                          .arg(packet[pos], 2, 16, QChar('0'));
        return {};
    }
    pos++;

    if (pos + 1 >= bytesRead || packet[pos] != static_cast<uint8_t>(TrezorWire::kHeaderMagic0) ||
        packet[pos + 1] != static_cast<uint8_t>(TrezorWire::kHeaderMagic1)) {
        m_lastError = QStringLiteral("Invalid response: missing '##' header");
        return {};
    }
    pos += 2;

    if (pos + 6 > bytesRead) {
        m_lastError = QStringLiteral("First packet too short for header");
        return {};
    }

    response.msgType = static_cast<uint16_t>((packet[pos] << 8) | packet[pos + 1]);
    pos += 2;

    uint32_t dataLen = (static_cast<uint32_t>(packet[pos]) << 24) |
                       (static_cast<uint32_t>(packet[pos + 1]) << 16) |
                       (static_cast<uint32_t>(packet[pos + 2]) << 8) |
                       static_cast<uint32_t>(packet[pos + 3]);
    pos += 4;

    // Read payload from first packet
    int firstChunkSize = qMin(bytesRead - pos, static_cast<int>(dataLen));
    if (firstChunkSize > 0) {
        response.data.append(reinterpret_cast<const char*>(packet.get() + pos), firstChunkSize);
    }

    // Read continuation packets
    while (static_cast<uint32_t>(response.data.size()) < dataLen) {
        bytesRead = readPacket(packet.get(), pktSize, timeoutMs);
        if (bytesRead <= 0) {
            m_lastError = bytesRead < 0 ? QStringLiteral("Continuation read failed")
                                        : QStringLiteral("Continuation read timed out");
            return {};
        }

        pos = 0;
        if (packet[pos] == TrezorWire::kMarker) {
            pos++;
        }

        int remaining = static_cast<int>(dataLen) - response.data.size();
        int chunkSize = qMin(bytesRead - pos, remaining);
        if (chunkSize > 0) {
            response.data.append(reinterpret_cast<const char*>(packet.get() + pos), chunkSize);
        }
    }

    return response;
}
