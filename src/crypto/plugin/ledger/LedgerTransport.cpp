#include "crypto/plugin/ledger/LedgerTransport.h"

#include <hidapi.h>

#include <QByteArray>
#include <QDebug>
#include <QtEndian>
#include <cstring>

LedgerTransport::LedgerTransport() = default;

LedgerTransport::~LedgerTransport() { close(); }

bool LedgerTransport::open(const QString& devicePath) {
    close();
    qDebug() << "[LedgerTransport::open] Opening device:" << devicePath;
    m_device = hid_open_path(devicePath.toUtf8().constData());
    if (!m_device) {
        m_lastError = QStringLiteral("Failed to open HID device");
        qWarning() << "[LedgerTransport::open] FAILED:" << m_lastError;
        return false;
    }
    m_lastError.clear();
    qDebug() << "[LedgerTransport::open] Device opened successfully";
    return true;
}

void LedgerTransport::close() {
    if (m_device) {
        qDebug() << "[LedgerTransport::close] Closing HID device";
        hid_close(m_device);
        m_device = nullptr;
    }
}

bool LedgerTransport::isOpen() const { return m_device != nullptr; }

// ── Raw I/O (overridable for testing) ───────────────────────────

int LedgerTransport::writePacket(const uint8_t* data, int size) {
    if (!m_device) {
        return -1;
    }
    return hid_write(m_device, data, size);
}

int LedgerTransport::readPacket(uint8_t* buf, int bufSize, int timeoutMs) {
    if (!m_device) {
        return -1;
    }
    return hid_read_timeout(m_device, buf, bufSize, timeoutMs);
}

// ── APDU exchange ───────────────────────────────────────────────

QByteArray LedgerTransport::exchange(const QByteArray& apdu, int timeoutMs) {
    m_statusWord = 0;
    m_lastError.clear();

    qDebug() << "[LedgerTransport::exchange] ────────────────────────────────";
    qDebug() << "[LedgerTransport::exchange] APDU size=" << apdu.size() << "timeout=" << timeoutMs
             << "ms";
    qDebug() << "[LedgerTransport::exchange] APDU hex=" << apdu.toHex();

    if (!sendFrames(apdu)) {
        qWarning() << "[LedgerTransport::exchange] sendFrames FAILED:" << m_lastError;
        return {};
    }

    qDebug() << "[LedgerTransport::exchange] Frames sent, reading response...";
    QByteArray response = readFrames(timeoutMs);
    if (response.isEmpty()) {
        qWarning() << "[LedgerTransport::exchange] readFrames returned empty:" << m_lastError;
        return {};
    }

    // Last 2 bytes are the status word
    if (response.size() < 2) {
        m_lastError = QStringLiteral("Response too short");
        qWarning() << "[LedgerTransport::exchange] Response too short:" << response.size();
        return {};
    }

    auto* raw = reinterpret_cast<const uint8_t*>(response.constData());
    m_statusWord =
        static_cast<uint16_t>((raw[response.size() - 2] << 8) | raw[response.size() - 1]);

    qDebug() << "[LedgerTransport::exchange] Response size=" << response.size() << "SW=0x"
             << QString::number(m_statusWord, 16).rightJustified(4, '0')
             << "data hex=" << response.toHex().left(128);

    if (m_statusWord != 0x9000) {
        m_lastError = statusWordMessage(m_statusWord);
        qWarning() << "[LedgerTransport::exchange] Non-success SW:" << m_lastError;
        return {};
    }

    QByteArray data = response.left(response.size() - 2);
    qDebug() << "[LedgerTransport::exchange] SUCCESS — data size=" << data.size();
    return data;
}

// ── Frame sending ───────────────────────────────────────────────

bool LedgerTransport::sendFrames(const QByteArray& apdu) {
    int offset = 0;
    uint16_t seq = 0;
    const int apduLen = apdu.size();

    qDebug() << "[LedgerTransport::sendFrames] Sending" << apduLen << "bytes";

    while (offset < apduLen) {
        uint8_t packet[LedgerWire::kPacketSize];
        std::memset(packet, 0, LedgerWire::kPacketSize);

        int pos = 0;
        // Channel (big-endian)
        packet[pos++] = static_cast<uint8_t>(LedgerWire::kChannel >> 8);
        packet[pos++] = static_cast<uint8_t>(LedgerWire::kChannel & 0xFF);
        // Tag
        packet[pos++] = LedgerWire::kTagApdu;
        // Sequence (big-endian)
        packet[pos++] = static_cast<uint8_t>(seq >> 8);
        packet[pos++] = static_cast<uint8_t>(seq & 0xFF);

        if (seq == 0) {
            // First packet includes APDU length (big-endian)
            packet[pos++] = static_cast<uint8_t>(apduLen >> 8);
            packet[pos++] = static_cast<uint8_t>(apduLen & 0xFF);
        }

        int payloadCapacity = (seq == 0) ? LedgerWire::kFirstPayload : LedgerWire::kContPayload;
        int chunkSize = qMin(payloadCapacity, apduLen - offset);
        std::memcpy(packet + pos, apdu.constData() + offset, chunkSize);

        qDebug() << "[LedgerTransport::sendFrames] Packet seq=" << seq << "payload=" << chunkSize
                 << "bytes, offset=" << offset << "/" << apduLen;

        int written = writePacket(packet, LedgerWire::kPacketSize);
        if (written < 0) {
            m_lastError = QStringLiteral("USB write failed");
            qWarning() << "[LedgerTransport::sendFrames] writePacket FAILED at seq=" << seq;
            return false;
        }

        offset += chunkSize;
        seq++;
    }

    qDebug() << "[LedgerTransport::sendFrames] All" << seq << "packets sent";
    return true;
}

// ── Frame reading ───────────────────────────────────────────────

QByteArray LedgerTransport::readFrames(int timeoutMs) {
    QByteArray result;
    uint16_t seq = 0;
    int totalLen = 0;

    qDebug() << "[LedgerTransport::readFrames] Waiting for response, timeout=" << timeoutMs << "ms";

    while (true) {
        uint8_t packet[LedgerWire::kPacketSize];
        int bytesRead = readPacket(packet, LedgerWire::kPacketSize, timeoutMs);

        if (bytesRead < 0) {
            m_lastError = QStringLiteral("USB read failed");
            qWarning() << "[LedgerTransport::readFrames] readPacket returned" << bytesRead;
            return {};
        }
        if (bytesRead == 0) {
            m_lastError = QStringLiteral("Device read timed out");
            qWarning() << "[LedgerTransport::readFrames] Timed out waiting for seq=" << seq;
            return {};
        }

        int pos = 0;

        // Validate channel
        uint16_t chan = static_cast<uint16_t>((packet[pos] << 8) | packet[pos + 1]);
        pos += 2;
        if (chan != LedgerWire::kChannel) {
            qDebug() << "[LedgerTransport::readFrames] Skipping non-matching channel: 0x"
                     << QString::number(chan, 16);
            continue;
        }

        // Validate tag
        if (packet[pos++] != LedgerWire::kTagApdu) {
            qDebug() << "[LedgerTransport::readFrames] Skipping non-APDU tag";
            continue;
        }

        // Validate sequence
        uint16_t rSeq = static_cast<uint16_t>((packet[pos] << 8) | packet[pos + 1]);
        pos += 2;
        if (rSeq != seq) {
            qDebug() << "[LedgerTransport::readFrames] Sequence mismatch: got" << rSeq << "expected"
                     << seq;
            continue;
        }

        if (seq == 0) {
            // First response includes total length
            totalLen = (packet[pos] << 8) | packet[pos + 1];
            pos += 2;
            qDebug() << "[LedgerTransport::readFrames] First packet, totalLen=" << totalLen;
        }

        int payloadCapacity = (seq == 0) ? LedgerWire::kFirstPayload : LedgerWire::kContPayload;
        int remaining = totalLen - result.size();
        int chunkSize = qMin(payloadCapacity, remaining);
        result.append(reinterpret_cast<const char*>(packet + pos), chunkSize);

        qDebug() << "[LedgerTransport::readFrames] Packet seq=" << seq << "chunk=" << chunkSize
                 << "bytes, total so far=" << result.size() << "/" << totalLen;

        if (result.size() >= totalLen) {
            break;
        }
        seq++;
    }

    qDebug() << "[LedgerTransport::readFrames] Complete, received" << result.size() << "bytes";
    return result;
}

// ── Status word messages ────────────────────────────────────────

QString LedgerTransport::statusWordMessage(uint16_t sw) {
    switch (sw) {
    case 0x9000:
        return QStringLiteral("Success");
    case 0x6985:
        return QStringLiteral("Transaction rejected on device");
    case 0x6A82:
        return QStringLiteral("Please open the Solana app on your Ledger");
    case 0x6FAA: // fall through
    case 0x5515:
        return QStringLiteral("Ledger is locked — please unlock it");
    case 0x6D00:
        return QStringLiteral("Instruction not supported — update Solana app");
    case 0x6E00:
        return QStringLiteral("CLA not supported");
    case 0x6700:
        return QStringLiteral("Wrong data length");
    case 0x6982:
        return QStringLiteral("Security condition not satisfied");
    default:
        return QStringLiteral("Ledger error 0x%1").arg(sw, 4, 16, QLatin1Char('0'));
    }
}
