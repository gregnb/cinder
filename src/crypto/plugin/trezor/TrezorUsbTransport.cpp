#include "crypto/plugin/trezor/TrezorUsbTransport.h"
#include "crypto/plugin/trezor/TrezorProtobuf.h"

#include <libusb.h>

#include <QDebug>
#include <cstring>

// ── Shared libusb context (ref-counted) ──────────────────────
// On macOS, destroying a libusb_context (libusb_exit) while the kernel is
// still releasing a claimed interface causes the next context to fail
// libusb_claim_interface with "Access denied". Keeping a single long-lived
// context avoids this race.

libusb_context* TrezorUsbTransport::s_ctx = nullptr;
int TrezorUsbTransport::s_ctxRefCount = 0;
QMutex TrezorUsbTransport::s_ctxMutex;

libusb_context* TrezorUsbTransport::acquireContext() {
    QMutexLocker lock(&s_ctxMutex);
    if (s_ctxRefCount == 0) {
        int rc = libusb_init(&s_ctx);
        if (rc != 0) {
            qWarning() << "[TrezorUsbTransport] libusb_init failed:" << libusb_strerror(rc);
            s_ctx = nullptr;
            return nullptr;
        }
    }
    ++s_ctxRefCount;
    return s_ctx;
}

void TrezorUsbTransport::releaseContext() {
    QMutexLocker lock(&s_ctxMutex);
    if (s_ctxRefCount > 0) {
        --s_ctxRefCount;
        if (s_ctxRefCount == 0 && s_ctx) {
            libusb_exit(s_ctx);
            s_ctx = nullptr;
        }
    }
}

TrezorUsbTransport::TrezorUsbTransport() { acquireContext(); }

TrezorUsbTransport::~TrezorUsbTransport() {
    close();
    releaseContext();
}

// ── Device enumeration (static) ──────────────────────────────

QList<TrezorUsbTransport::UsbDeviceInfo> TrezorUsbTransport::enumerate(uint16_t vid, uint16_t pid) {
    QList<UsbDeviceInfo> result;

    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) {
        return result;
    }

    libusb_device** devList = nullptr;
    ssize_t cnt = libusb_get_device_list(ctx, &devList);

    for (ssize_t i = 0; i < cnt; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devList[i], &desc) != 0) {
            continue;
        }
        if (desc.idVendor != vid || desc.idProduct != pid) {
            continue;
        }

        uint8_t bus = libusb_get_bus_number(devList[i]);
        uint8_t addr = libusb_get_device_address(devList[i]);

        // Verify vendor-specific interface (class 0xFF) on interface 0
        libusb_config_descriptor* config = nullptr;
        int rc = libusb_get_active_config_descriptor(devList[i], &config);
        if (rc != 0) {
            rc = libusb_get_config_descriptor(devList[i], 0, &config);
        }
        if (rc != 0) {
            continue;
        }

        bool hasVendorInterface = false;
        for (int j = 0; j < config->bNumInterfaces; ++j) {
            const libusb_interface& iface = config->interface[j];
            for (int k = 0; k < iface.num_altsetting; ++k) {
                const libusb_interface_descriptor& alt = iface.altsetting[k];
                if (alt.bInterfaceNumber == kInterfaceNum && alt.bInterfaceClass == 0xFF) {
                    hasVendorInterface = true;
                }
            }
        }
        libusb_free_config_descriptor(config);
        if (!hasVendorInterface) {
            continue;
        }

        UsbDeviceInfo info;
        info.path = QStringLiteral("%1:%2").arg(bus).arg(addr);
        info.vid = vid;
        info.pid = pid;

        // Try to get string descriptors
        libusb_device_handle* tmpHandle = nullptr;
        if (libusb_open(devList[i], &tmpHandle) == 0 && tmpHandle) {
            unsigned char buf[256];
            if (desc.iProduct) {
                int len =
                    libusb_get_string_descriptor_ascii(tmpHandle, desc.iProduct, buf, sizeof(buf));
                if (len > 0) {
                    info.product = QString::fromLatin1(reinterpret_cast<const char*>(buf), len);
                }
            }
            if (desc.iManufacturer) {
                int len = libusb_get_string_descriptor_ascii(tmpHandle, desc.iManufacturer, buf,
                                                             sizeof(buf));
                if (len > 0) {
                    info.manufacturer =
                        QString::fromLatin1(reinterpret_cast<const char*>(buf), len);
                }
            }
            libusb_close(tmpHandle);
        }

        result.append(info);
    }

    libusb_free_device_list(devList, 1);
    libusb_exit(ctx);
    return result;
}

// ── Open / close ─────────────────────────────────────────────

bool TrezorUsbTransport::open(const QString& devicePath) {
    close();

    if (!s_ctx) {
        m_lastError = QStringLiteral("libusb not initialized");
        return false;
    }

    QStringList parts = devicePath.split(':');
    if (parts.size() != 2) {
        m_lastError = QStringLiteral("Invalid device path format: %1").arg(devicePath);
        return false;
    }
    uint8_t targetBus = static_cast<uint8_t>(parts[0].toUInt());
    uint8_t targetAddr = static_cast<uint8_t>(parts[1].toUInt());

    libusb_device** devList = nullptr;
    ssize_t cnt = libusb_get_device_list(s_ctx, &devList);

    libusb_device* targetDev = nullptr;
    for (ssize_t i = 0; i < cnt; ++i) {
        if (libusb_get_bus_number(devList[i]) == targetBus &&
            libusb_get_device_address(devList[i]) == targetAddr) {
            targetDev = devList[i];
            break;
        }
    }

    if (!targetDev) {
        m_lastError =
            QStringLiteral("Device not found at bus %1 addr %2").arg(targetBus).arg(targetAddr);
        libusb_free_device_list(devList, 1);
        return false;
    }

    int rc = libusb_open(targetDev, &m_handle);
    libusb_free_device_list(devList, 1);

    if (rc != 0) {
        m_lastError = QStringLiteral("Failed to open USB device: %1").arg(libusb_strerror(rc));
        m_handle = nullptr;
        return false;
    }

    // Set configuration to 1 if needed
    int currentConfig = 0;
    libusb_get_configuration(m_handle, &currentConfig);
    if (currentConfig != 1) {
        rc = libusb_set_configuration(m_handle, 1);
        if (rc != 0) {
            qWarning() << "[TrezorUsbTransport::open] set_configuration failed:"
                       << libusb_strerror(rc);
        }
    }

    // Claim interface 0
    rc = libusb_claim_interface(m_handle, kInterfaceNum);
    if (rc != 0) {
        m_lastError = QStringLiteral("Failed to claim interface 0: %1").arg(libusb_strerror(rc));
        libusb_close(m_handle);
        m_handle = nullptr;
        return false;
    }
    m_interfaceClaimed = true;

    m_lastError.clear();
    qDebug() << "[TrezorUsbTransport::open] Ready at" << devicePath;
    return true;
}

void TrezorUsbTransport::close() {
    if (m_handle) {
        if (m_interfaceClaimed) {
            libusb_release_interface(m_handle, kInterfaceNum);
            m_interfaceClaimed = false;
        }
        libusb_close(m_handle);
        m_handle = nullptr;
    }
}

bool TrezorUsbTransport::isOpen() const { return m_handle != nullptr && m_interfaceClaimed; }

// ── Packet geometry ──────────────────────────────────────────

int TrezorUsbTransport::writePacketSize() const { return TrezorWire::kUsbPacketSize; }
int TrezorUsbTransport::readPacketSize() const { return TrezorWire::kUsbPacketSize; }
int TrezorUsbTransport::writeDataOffset() const { return 1; } // marker only, no report ID

// ── Raw packet I/O ───────────────────────────────────────────

bool TrezorUsbTransport::writePacket(const uint8_t* data, int size) {
    int transferred = 0;
    int rc = libusb_interrupt_transfer(m_handle, kEndpointOut, const_cast<uint8_t*>(data), size,
                                       &transferred, TrezorTimeout::kUsbWrite);
    if (rc != 0) {
        m_lastError = QStringLiteral("USB write failed: %1").arg(libusb_strerror(rc));
        return false;
    }
    return true;
}

int TrezorUsbTransport::readPacket(uint8_t* buf, int bufSize, int timeoutMs) {
    int transferred = 0;
    int rc =
        libusb_interrupt_transfer(m_handle, kEndpointIn, buf, bufSize, &transferred, timeoutMs);
    if (rc != 0) {
        if (rc == LIBUSB_ERROR_TIMEOUT) {
            m_lastError = QStringLiteral("Device read timed out");
        } else {
            m_lastError = QStringLiteral("USB read failed: %1").arg(libusb_strerror(rc));
        }
        return -1;
    }
    if (transferred == 0) {
        m_lastError = QStringLiteral("Zero bytes read");
        return 0;
    }
    return transferred;
}
