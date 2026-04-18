#include "crypto/plugin/trezor/TrezorHidTransport.h"
#include "crypto/plugin/trezor/TrezorProtobuf.h"

#include <hidapi.h>

#include <QDebug>

TrezorHidTransport::TrezorHidTransport() = default;

TrezorHidTransport::~TrezorHidTransport() { close(); }

bool TrezorHidTransport::open(const QString& devicePath) {
    close();
    m_device = hid_open_path(devicePath.toUtf8().constData());
    if (!m_device) {
        const wchar_t* err = hid_error(nullptr);
        m_lastError = QStringLiteral("Failed to open Trezor HID device: %1")
                          .arg(err ? QString::fromWCharArray(err) : QStringLiteral("unknown"));
        qWarning() << "[TrezorHidTransport::open]" << m_lastError;
        return false;
    }
    m_lastError.clear();
    qDebug() << "[TrezorHidTransport::open] Device opened";
    return true;
}

void TrezorHidTransport::close() {
    if (m_device) {
        hid_close(m_device);
        m_device = nullptr;
    }
}

bool TrezorHidTransport::isOpen() const { return m_device != nullptr; }

int TrezorHidTransport::writePacketSize() const { return TrezorWire::kHidWriteSize; }
int TrezorHidTransport::readPacketSize() const { return TrezorWire::kUsbPacketSize; }
int TrezorHidTransport::writeDataOffset() const { return 2; } // report ID + marker

bool TrezorHidTransport::writePacket(const uint8_t* data, int size) {
    int written = hid_write(m_device, data, size);
    if (written < 0) {
        const wchar_t* err = hid_error(m_device);
        m_lastError = QStringLiteral("HID write failed: %1")
                          .arg(err ? QString::fromWCharArray(err) : QStringLiteral("unknown"));
        return false;
    }
    return true;
}

int TrezorHidTransport::readPacket(uint8_t* buf, int bufSize, int timeoutMs) {
    int bytesRead = hid_read_timeout(m_device, buf, bufSize, timeoutMs);
    if (bytesRead < 0) {
        const wchar_t* err = hid_error(m_device);
        m_lastError = QStringLiteral("HID read failed: %1")
                          .arg(err ? QString::fromWCharArray(err) : QStringLiteral("unknown"));
    } else if (bytesRead == 0) {
        m_lastError = QStringLiteral("Device read timed out");
    }
    return bytesRead;
}
