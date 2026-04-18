#include "crypto/plugin/trezor/TrezorPlugin.h"
#include "crypto/plugin/trezor/TrezorHidTransport.h"
#include "crypto/plugin/trezor/TrezorProtobuf.h"
#include "crypto/plugin/trezor/TrezorSigner.h"
#include "crypto/plugin/trezor/TrezorUsbTransport.h"
#include "tx/Base58.h"

#include <hidapi.h>

#include <QDebug>

TrezorPlugin::TrezorPlugin(QObject* parent) : HardwareWalletPlugin(parent) {
    int rc = hid_init();
    if (rc == 0) {
        m_hidInitialized = true;
    } else {
        qWarning() << "[TrezorPlugin] hid_init() failed rc=" << rc;
    }
}

TrezorPlugin::~TrezorPlugin() {
    stopPolling();
    if (m_hidInitialized) {
        hid_exit();
    }
}

QString TrezorPlugin::pluginId() const { return QStringLiteral("trezor"); }
QString TrezorPlugin::displayName() const { return QStringLiteral("Trezor"); }

QList<HWDeviceInfo> TrezorPlugin::connectedDevices() const { return m_devices; }

void TrezorPlugin::scanDevices() {
    QList<HWDeviceInfo> newDevices;

    // 1. Scan for Trezor One via HIDAPI (VID 0x534C)
    if (m_hidInitialized) {
        hid_device_info* devs = hid_enumerate(kTrezorOneVID, kTrezorOnePID);
        for (hid_device_info* cur = devs; cur; cur = cur->next) {
            if (cur->interface_number != 0 && cur->interface_number != -1) {
                continue;
            }

            HWDeviceInfo info;
            info.deviceId = QString::fromUtf8(cur->path);
            info.model = QStringLiteral("Model One");
            info.displayName = QStringLiteral("Trezor Model One");

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

            newDevices.append(info);
            m_transportTypes[info.deviceId] = TransportType::HID;
        }
        hid_free_enumeration(devs);
    }

    // 2. Scan for Trezor Model T / Safe 3 / Safe 5 via libusb
    auto usbDevices = TrezorUsbTransport::enumerate(kTrezorVID, kTrezorPID);
    for (const auto& dev : usbDevices) {
        HWDeviceInfo info;
        info.deviceId = dev.path;
        info.model = QStringLiteral("Safe / Model T");
        info.displayName = QStringLiteral("Trezor (USB)");

        // Tier 1 — USB enumeration metadata
        info.manufacturer = dev.manufacturer;
        info.productString = dev.product;

        newDevices.append(info);
        m_transportTypes[info.deviceId] = TransportType::USB;
    }

    // Detect changes
    QStringList oldIds, newIds;
    for (const auto& d : m_devices) {
        oldIds.append(d.deviceId);
    }
    for (const auto& d : newDevices) {
        newIds.append(d.deviceId);
    }

    bool changed = false;
    for (const auto& d : m_devices) {
        if (!newIds.contains(d.deviceId)) {
            changed = true;
            m_transportTypes.remove(d.deviceId);
            emit deviceDisconnected(d.deviceId);
        }
    }
    for (const auto& d : newDevices) {
        if (!oldIds.contains(d.deviceId)) {
            changed = true;
            emit deviceConnected(d);
        }
    }

    m_devices = newDevices;
    if (changed) {
        emit devicesChanged();
    }
}

HWDeviceInfo TrezorPlugin::queryDeviceInfo(const QString& deviceId) {
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

    // Open device and get Features via Initialize
    TransportType ttype = m_transportTypes.value(deviceId, TransportType::USB);
    std::unique_ptr<TrezorTransport> transport;
    if (ttype == TransportType::HID) {
        transport = std::make_unique<TrezorHidTransport>();
    } else {
        transport = std::make_unique<TrezorUsbTransport>();
    }

    if (!transport->open(deviceId)) {
        qWarning() << "[TrezorPlugin::queryDeviceInfo] Open failed:" << transport->lastError();
        return info;
    }

    TrezorResponse resp = transport->call(TrezorMsg::Initialize, TrezorProtobuf::encodeInitialize(),
                                          TrezorTimeout::kInit);
    if (resp.msgType != TrezorMsg::Features) {
        qWarning() << "[TrezorPlugin::queryDeviceInfo] Expected Features, got" << resp.msgType;
        return info;
    }

    TrezorFeatures features = TrezorProtobuf::decodeFeatures(resp.data);

    // Enrich with protocol-level data
    info.featuresQueried = true;
    info.firmwareVersion = features.firmwareVersionString();
    info.label = features.label;
    info.initialized = features.initialized;
    info.pinProtected = features.pinProtection;
    info.passphraseEnabled = features.passphraseProtection;
    if (!features.deviceId.isEmpty()) {
        info.serialNumber = features.deviceId;
    }
    if (!features.internalModel.isEmpty()) {
        info.model = features.internalModel;
        info.displayName = QStringLiteral("Trezor %1").arg(features.internalModel);
    }

    // Update stored device list too
    for (auto& d : m_devices) {
        if (d.deviceId == deviceId) {
            d = info;
            break;
        }
    }

    transport->close();
    return info;
}

Signer* TrezorPlugin::createSigner(const QString& deviceId, const QString& derivationPath,
                                   QObject* parent) {
    qDebug() << "[TrezorPlugin::createSigner] deviceId=" << deviceId << "path=" << derivationPath;

    QList<uint32_t> addressN = TrezorProtobuf::parseBip44Path(derivationPath);
    if (addressN.isEmpty()) {
        qWarning() << "[TrezorPlugin::createSigner] Invalid derivation path:" << derivationPath;
        return nullptr;
    }

    // Create transport based on device type
    TransportType ttype = m_transportTypes.value(deviceId, TransportType::USB);
    std::unique_ptr<TrezorTransport> transport;

    if (ttype == TransportType::HID) {
        transport = std::make_unique<TrezorHidTransport>();
    } else {
        transport = std::make_unique<TrezorUsbTransport>();
    }

    if (!transport->open(deviceId)) {
        qWarning() << "[TrezorPlugin::createSigner] Open failed:" << transport->lastError();
        return nullptr;
    }

    // Step 1: Initialize session
    TrezorResponse resp = transport->call(TrezorMsg::Initialize, TrezorProtobuf::encodeInitialize(),
                                          TrezorTimeout::kInit);
    if (resp.msgType != TrezorMsg::Features) {
        qWarning() << "[TrezorPlugin::createSigner] Expected Features, got" << resp.msgType;
        return nullptr;
    }

    TrezorFeatures features = TrezorProtobuf::decodeFeatures(resp.data);
    qDebug() << "[TrezorPlugin::createSigner] model=" << features.model
             << "internalModel=" << features.internalModel
             << "firmware=" << features.firmwareVersionString() << "label=" << features.label
             << "deviceId=" << features.deviceId << "pin=" << features.pinProtection
             << "passphrase=" << features.passphraseProtection
             << "initialized=" << features.initialized << "unlocked=" << features.unlocked;

    // Enrich stored device info with protocol-level Features data
    for (auto& d : m_devices) {
        if (d.deviceId == deviceId) {
            d.featuresQueried = true;
            d.firmwareVersion = features.firmwareVersionString();
            d.label = features.label;
            d.initialized = features.initialized;
            d.pinProtected = features.pinProtection;
            d.passphraseEnabled = features.passphraseProtection;
            if (!features.deviceId.isEmpty()) {
                d.serialNumber = features.deviceId;
            }
            if (!features.internalModel.isEmpty()) {
                d.model = features.internalModel;
                d.displayName = QStringLiteral("Trezor %1").arg(features.internalModel);
            }
            break;
        }
    }

    // Step 2: Get public key via SolanaGetPublicKey(900)
    QByteArray getPubMsg = TrezorProtobuf::encodeSolanaGetPublicKey(addressN, false);
    resp = transport->call(TrezorMsg::SolanaGetPublicKey, getPubMsg, TrezorTimeout::kDefault);
    resp = TrezorProtobuf::driveInteraction(transport.get(), std::move(resp));

    // If Solana messages fail, fallback to GetPublicKey(11) + ed25519
    if (resp.msgType == TrezorMsg::Failure) {
        TrezorFailure fail = TrezorProtobuf::decodeFailure(resp.data);
        qDebug() << "[TrezorPlugin::createSigner] SolanaGetPublicKey failed:" << fail.message
                 << "— falling back to GetPublicKey+ed25519";

        // Re-initialize before fallback
        resp = transport->call(TrezorMsg::Initialize, TrezorProtobuf::encodeInitialize(),
                               TrezorTimeout::kInit);
        if (resp.msgType != TrezorMsg::Features) {
            return nullptr;
        }

        QByteArray genPubMsg =
            TrezorProtobuf::encodeGetPublicKey(addressN, QStringLiteral("ed25519"));
        resp = transport->call(TrezorMsg::GetPublicKey, genPubMsg, TrezorTimeout::kDefault);
        resp = TrezorProtobuf::driveInteraction(transport.get(), std::move(resp));

        if (resp.msgType == TrezorMsg::Failure) {
            TrezorFailure fail2 = TrezorProtobuf::decodeFailure(resp.data);
            qWarning() << "[TrezorPlugin::createSigner] GetPublicKey(11) also failed:"
                       << fail2.message;
            return nullptr;
        }
        if (resp.msgType != TrezorMsg::PublicKey) {
            qWarning() << "[TrezorPlugin::createSigner] Unexpected response:" << resp.msgType;
            return nullptr;
        }
    }

    // Decode public key from whichever path succeeded
    QByteArray pubkey;
    if (resp.msgType == TrezorMsg::SolanaPublicKey) {
        pubkey = TrezorProtobuf::decodeSolanaPublicKey(resp.data);
    } else if (resp.msgType == TrezorMsg::PublicKey) {
        pubkey = TrezorProtobuf::decodePublicKeyNode(resp.data);
    } else {
        qWarning() << "[TrezorPlugin::createSigner] Unexpected msgType:" << resp.msgType;
        return nullptr;
    }

    if (pubkey.size() != TrezorCrypto::kEd25519KeySize) {
        qWarning() << "[TrezorPlugin::createSigner] Invalid pubkey size:" << pubkey.size();
        return nullptr;
    }

    QString address = Base58::encode(pubkey);
    qDebug() << "[TrezorPlugin::createSigner] Address:" << address;

    QString model = features.model.isEmpty() ? QStringLiteral("Trezor") : features.model;
    for (const auto& d : m_devices) {
        if (d.deviceId == deviceId) {
            model = d.model;
            break;
        }
    }

    return new TrezorSigner(std::move(transport), addressN, pubkey, address, model, parent);
}

void TrezorPlugin::startPolling(int intervalMs) {
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &TrezorPlugin::scanDevices);
    }
    m_pollTimer->start(intervalMs);
    scanDevices();
}

void TrezorPlugin::stopPolling() {
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}
