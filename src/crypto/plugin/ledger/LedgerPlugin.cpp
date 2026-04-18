#include "crypto/plugin/ledger/LedgerPlugin.h"
#include "crypto/plugin/ledger/LedgerSigner.h"
#include "crypto/plugin/ledger/LedgerTransport.h"
#include "tx/Base58.h"

#include <hidapi.h>

#include <QDebug>

LedgerPlugin::LedgerPlugin(QObject* parent) : HardwareWalletPlugin(parent) {
    if (hid_init() == 0) {
        m_hidInitialized = true;
    } else {
        qWarning() << "LedgerPlugin: hid_init() failed";
    }
}

LedgerPlugin::~LedgerPlugin() {
    stopPolling();
    if (m_hidInitialized) {
        hid_exit();
    }
}

QString LedgerPlugin::pluginId() const { return QStringLiteral("ledger"); }
QString LedgerPlugin::displayName() const { return QStringLiteral("Ledger"); }

QList<HWDeviceInfo> LedgerPlugin::connectedDevices() const { return m_devices; }

void LedgerPlugin::scanDevices() {
    if (!m_hidInitialized) {
        return;
    }

    QList<HWDeviceInfo> newDevices;

    hid_device_info* devs = hid_enumerate(kLedgerVID, 0);
    for (hid_device_info* cur = devs; cur; cur = cur->next) {
        // On macOS, Ledger exposes multiple HID interfaces — use interface 0 only
        if (cur->interface_number != 0 && cur->interface_number != -1) {
            continue;
        }

        HWDeviceInfo info;
        info.deviceId = QString::fromUtf8(cur->path);
        info.model = modelFromPID(cur->product_id);
        info.displayName = QStringLiteral("Ledger %1").arg(info.model);

        // Tier 1 — HID enumeration metadata
        if (cur->serial_number) {
            info.serialNumber = QString::fromWCharArray(cur->serial_number);
        }
        if (cur->manufacturer_string) {
            info.manufacturer = QString::fromWCharArray(cur->manufacturer_string);
        }
        if (cur->product_string) {
            info.productString = QString::fromWCharArray(cur->product_string);
        }
        if (cur->release_number > 0) {
            int major = (cur->release_number >> 8) & 0xFF;
            int minor = cur->release_number & 0xFF;
            info.firmwareVersion = QStringLiteral("%1.%2").arg(major).arg(minor);
        }

        newDevices.append(info);
    }
    hid_free_enumeration(devs);

    // Detect changes
    QStringList oldIds;
    for (const auto& d : m_devices) {
        oldIds.append(d.deviceId);
    }
    QStringList newIds;
    for (const auto& d : newDevices) {
        newIds.append(d.deviceId);
    }

    bool changed = false;

    // Check for disconnected devices
    for (const auto& d : m_devices) {
        if (!newIds.contains(d.deviceId)) {
            changed = true;
            emit deviceDisconnected(d.deviceId);
            qDebug() << "Ledger disconnected:" << d.displayName;
        }
    }

    // Check for newly connected devices
    for (const auto& d : newDevices) {
        if (!oldIds.contains(d.deviceId)) {
            changed = true;
            emit deviceConnected(d);
            qDebug() << "Ledger connected:" << d.displayName;
        }
    }

    m_devices = newDevices;

    if (changed) {
        emit devicesChanged();
    }
}

HWDeviceInfo LedgerPlugin::queryDeviceInfo(const QString& deviceId) {
    // Find base device info from scan
    HWDeviceInfo info;
    for (const auto& d : m_devices) {
        if (d.deviceId == deviceId) {
            info = d;
            break;
        }
    }
    if (info.deviceId.isEmpty()) {
        return info;
    }

    // All initialized Ledger devices have PIN enabled (mandatory).
    // We don't open the device here — on macOS, opening HID for a query
    // can leave the handle in a bad state and break subsequent operations
    // (signing, public key retrieval). Just set known facts.
    info.pinProtected = true;
    info.initialized = true;
    info.featuresQueried = true;

    // Update stored device list
    for (auto& d : m_devices) {
        if (d.deviceId == deviceId) {
            d = info;
            break;
        }
    }

    return info;
}

Signer* LedgerPlugin::createSigner(const QString& deviceId, const QString& derivationPath,
                                   QObject* parent) {
    QByteArray encodedPath = encodeDerivationPath(derivationPath);
    if (encodedPath.isEmpty()) {
        qWarning() << "LedgerPlugin: invalid derivation path:" << derivationPath;
        return nullptr;
    }

    auto transport = std::make_unique<LedgerTransport>();
    if (!transport->open(deviceId)) {
        qWarning() << "LedgerPlugin: failed to open device:" << transport->lastError();
        return nullptr;
    }

    // Send GET_PUBKEY APDU
    QByteArray apdu;
    apdu.append(static_cast<char>(LedgerApdu::kCLA));
    apdu.append(static_cast<char>(LedgerApdu::kINS_GET_PUBKEY));
    apdu.append(static_cast<char>(0x00));               // P1
    apdu.append(static_cast<char>(0x00));               // P2
    apdu.append(static_cast<char>(encodedPath.size())); // Lc
    apdu.append(encodedPath);

    QByteArray pubkey = transport->exchange(apdu, LedgerTimeout::kGetPubkey);
    if (pubkey.size() != LedgerCrypto::kEd25519KeySize) {
        qWarning() << "LedgerPlugin: GET_PUBKEY failed:" << transport->lastError() << "got"
                   << pubkey.size() << "bytes";
        return nullptr;
    }

    QString address = Base58::encode(pubkey);

    // Determine model from connected devices list
    QString model = QStringLiteral("Nano");
    for (const auto& d : m_devices) {
        if (d.deviceId == deviceId) {
            model = d.model;
            break;
        }
    }

    return new LedgerSigner(std::move(transport), encodedPath, pubkey, address, model, parent);
}

void LedgerPlugin::startPolling(int intervalMs) {
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &LedgerPlugin::scanDevices);
    }
    m_pollTimer->start(intervalMs);

    // Do an initial scan immediately
    scanDevices();
}

void LedgerPlugin::stopPolling() {
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}

QByteArray LedgerPlugin::encodeDerivationPath(const QString& path) {
    // Parse "m/44'/501'/0'/0'" into segments
    QString p = path;
    if (p.startsWith(QLatin1String("m/"))) {
        p = p.mid(2);
    }

    QStringList parts = p.split(QLatin1Char('/'));
    if (parts.isEmpty() || parts.size() > 10) {
        return {};
    }

    QByteArray result;
    result.append(static_cast<char>(parts.size()));

    for (const QString& part : parts) {
        QString seg = part.trimmed();
        bool hardened = seg.endsWith(QLatin1Char('\'')) || seg.endsWith(QLatin1Char('h'));
        if (hardened) {
            seg.chop(1);
        }

        bool ok = false;
        uint32_t val = seg.toUInt(&ok);
        if (!ok) {
            return {};
        }

        if (hardened) {
            val |= LedgerCrypto::kHardenedBit;
        }

        // Big-endian uint32
        char buf[4];
        buf[0] = static_cast<char>((val >> 24) & 0xFF);
        buf[1] = static_cast<char>((val >> 16) & 0xFF);
        buf[2] = static_cast<char>((val >> 8) & 0xFF);
        buf[3] = static_cast<char>(val & 0xFF);
        result.append(buf, 4);
    }

    return result;
}

QString LedgerPlugin::modelFromPID(uint16_t pid) {
    // Bootloader-mode PIDs (exact match)
    switch (pid) {
    case 0x0001:
        return QStringLiteral("Nano S");
    case 0x0004:
        return QStringLiteral("Nano X");
    case 0x0005:
        return QStringLiteral("Nano S Plus");
    case 0x0006:
        return QStringLiteral("Stax");
    case 0x0007:
        return QStringLiteral("Flex");
    case 0x0008:
        return QStringLiteral("Nano Gen5");
    default:
        break;
    }

    // App-mode PIDs: high byte encodes device family
    // e.g. 0x1011 = Nano S, 0x4011 = Nano X, 0x5011 = Nano S Plus, etc.
    uint8_t family = static_cast<uint8_t>(pid >> 8);
    switch (family) {
    case 0x10:
        return QStringLiteral("Nano S");
    case 0x40:
        return QStringLiteral("Nano X");
    case 0x50:
        return QStringLiteral("Nano S Plus");
    case 0x60:
        return QStringLiteral("Stax");
    case 0x70:
        return QStringLiteral("Flex");
    case 0x80:
        return QStringLiteral("Nano Gen5");
    default:
        return QStringLiteral("Unknown (0x%1)").arg(pid, 4, 16, QLatin1Char('0'));
    }
}
