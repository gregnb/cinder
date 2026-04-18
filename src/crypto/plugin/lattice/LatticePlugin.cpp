#include "crypto/plugin/lattice/LatticePlugin.h"
#include "crypto/plugin/lattice/LatticeSigner.h"
#include "crypto/plugin/lattice/LatticeTransport.h"
#include "tx/Base58.h"

#include <QDebug>
#include <QSettings>
#include <QtEndian>

namespace {
    const QString kSettingsGroup = QStringLiteral("lattice");
    const QString kKeyPrivKey = QStringLiteral("appPrivateKey");
    const QString kKeyPubKey = QStringLiteral("appPublicKey");
    const QString kKeySharedSecret = QStringLiteral("sharedSecret");
    const QString kKeyEphemeralPub = QStringLiteral("ephemeralPub");

    constexpr int kGetAddressPayloadSize = 54;
    constexpr int kEd25519BlockSize = 113;
} // namespace

LatticePlugin::LatticePlugin(QObject* parent)
    : HardwareWalletPlugin(parent), m_appName(QStringLiteral("Cinder Wallet")) {}

LatticePlugin::~LatticePlugin() { stopPolling(); }

QString LatticePlugin::pluginId() const { return QStringLiteral("gridplus"); }
QString LatticePlugin::displayName() const { return QStringLiteral("GridPlus"); }

void LatticePlugin::scanDevices() {
    // Lattice1 is network-based — no USB enumeration.
    // If a device ID is set, treat it as "connected" (reachability is checked on connect).
    if (m_deviceId.isEmpty()) {
        if (!m_devices.isEmpty()) {
            m_devices.clear();
            emit devicesChanged();
        }
        return;
    }

    bool alreadyListed = false;
    for (const auto& d : m_devices) {
        if (d.deviceId == m_deviceId) {
            alreadyListed = true;
            break;
        }
    }

    if (!alreadyListed) {
        HWDeviceInfo info;
        info.deviceId = m_deviceId;
        info.displayName = QStringLiteral("Lattice1");
        info.model = QStringLiteral("lattice1");
        info.manufacturer = QStringLiteral("GridPlus");
        m_devices = {info};
        emit devicesChanged();
        emit deviceConnected(info);
    }
}

QList<HWDeviceInfo> LatticePlugin::connectedDevices() const { return m_devices; }

void LatticePlugin::setDeviceId(const QString& deviceId) {
    m_deviceId = deviceId;
    scanDevices();
}

bool LatticePlugin::isPaired() const { return m_transport && m_transport->isPaired(); }

bool LatticePlugin::pair(const QString& pairingCode) {
    if (!m_transport) {
        emit pairingFailed(QStringLiteral("Not connected to device"));
        return false;
    }

    bool ok = m_transport->pair(pairingCode, m_appName);
    if (ok) {
        // Persist pairing state
        QSettings settings;
        settings.beginGroup(kSettingsGroup + "/" + m_deviceId);
        settings.setValue(kKeySharedSecret, m_transport->storedKeyPair().privateKey.toHex());
        settings.endGroup();

        emit pairingSucceeded();
    } else {
        emit pairingFailed(m_transport->lastError());
    }
    return ok;
}

Signer* LatticePlugin::createSigner(const QString& deviceId, const QString& derivationPath,
                                    QObject* parent) {
    qDebug() << "[LatticePlugin::createSigner] deviceId=" << deviceId << "path=" << derivationPath;

    QList<uint32_t> addressN = parseBip44Path(derivationPath);
    if (addressN.isEmpty()) {
        qWarning() << "[LatticePlugin] Invalid derivation path:" << derivationPath;
        return nullptr;
    }

    // Create transport
    auto transport = std::make_unique<LatticeTransport>();

    // Restore persisted keypair if available
    QSettings settings;
    settings.beginGroup(kSettingsGroup + "/" + deviceId);
    QByteArray storedPriv = QByteArray::fromHex(settings.value(kKeyPrivKey).toByteArray());
    QByteArray storedPub = QByteArray::fromHex(settings.value(kKeyPubKey).toByteArray());
    settings.endGroup();

    if (!storedPriv.isEmpty() && !storedPub.isEmpty()) {
        LatticeCrypto::EcKeyPair keypair;
        keypair.privateKey = storedPriv;
        keypair.publicKey = storedPub;
        transport->setStoredKeyPair(keypair);
    }

    if (!transport->open(deviceId)) {
        qWarning() << "[LatticePlugin] Open failed:" << transport->lastError();
        return nullptr;
    }

    // Connect (ECDH handshake)
    if (!transport->connect(m_appName)) {
        qWarning() << "[LatticePlugin] Connect failed:" << transport->lastError();
        return nullptr;
    }

    // Persist the keypair for future sessions
    LatticeCrypto::EcKeyPair keypair = transport->storedKeyPair();
    settings.beginGroup(kSettingsGroup + "/" + deviceId);
    settings.setValue(kKeyPrivKey, keypair.privateKey.toHex());
    settings.setValue(kKeyPubKey, keypair.publicKey.toHex());
    settings.endGroup();

    // Check if pairing is needed
    if (!transport->isPaired()) {
        m_transport = std::move(transport);
        emit pairingRequired();
        return nullptr;
    }

    // Get ED25519 public key via getAddresses request
    QByteArray getAddrPayload(kGetAddressPayloadSize, 0);
    char* p = getAddrPayload.data();

    // walletUID (32 bytes)
    QByteArray walletUid = transport->walletUID();
    if (walletUid.size() >= 32) {
        memcpy(p, walletUid.constData(), 32);
    }
    p += 32;

    // pathDepth_IterIdx: lower 4 bits = path length, upper 4 bits = iterIdx(0)
    *p = static_cast<char>(addressN.size() & 0x0F);
    p++;

    // 5 path indices as u32 BE (20 bytes)
    for (int i = 0; i < 5; ++i) {
        uint32_t idx = (i < addressN.size()) ? addressN[i] : 0;
        qToBigEndian(idx, p);
        p += 4;
    }

    // count(lower 4) | flag(upper 4): count=1, flag=ed25519Pubkey(4)
    *p = static_cast<char>((LatticeAddressFlag::kEd25519Pubkey << 4) | 1);

    LatticeResponse resp = transport->request(LatticeRequestType::kGetAddresses, getAddrPayload,
                                              LatticeTimeout::kGetAddress);
    if (!resp.isValid()) {
        qWarning() << "[LatticePlugin] GetAddresses failed:" << transport->lastError();
        return nullptr;
    }

    // Parse response: skip 1 byte (type marker), then 113-byte blocks, first 32 bytes = ed25519 key
    if (resp.payload.size() < 1 + 32) {
        qWarning() << "[LatticePlugin] GetAddresses response too short:" << resp.payload.size();
        return nullptr;
    }

    QByteArray pubkey;
    int offset = 1; // skip type marker
    while (offset + kEd25519BlockSize <= resp.payload.size()) {
        QByteArray key = resp.payload.mid(offset, LatticeCrypto::kEd25519KeySize);
        if (!key.isEmpty() && key != QByteArray(32, 0)) {
            pubkey = key;
            break;
        }
        offset += kEd25519BlockSize;
    }

    // Fallback: try reading 32 bytes directly after type marker
    if (pubkey.isEmpty() && resp.payload.size() >= 33) {
        pubkey = resp.payload.mid(1, 32);
    }

    if (pubkey.size() != LatticeCrypto::kEd25519KeySize) {
        qWarning() << "[LatticePlugin] Invalid pubkey size:" << pubkey.size();
        return nullptr;
    }

    QString address = Base58::encode(pubkey);
    qDebug() << "[LatticePlugin] Derived address:" << address;

    return new LatticeSigner(std::move(transport), addressN, pubkey, address, parent);
}

QList<uint32_t> LatticePlugin::parseBip44Path(const QString& path) const {
    // Parse "m/44'/501'/0'/0'" format
    QList<uint32_t> result;
    QString clean = path.trimmed();
    if (clean.startsWith("m/")) {
        clean = clean.mid(2);
    }

    const QStringList parts = clean.split('/');
    for (const QString& part : parts) {
        bool ok = false;
        QString numStr = part;
        bool hardened = numStr.endsWith('\'') || numStr.endsWith('h');
        if (hardened) {
            numStr.chop(1);
        }
        uint32_t val = numStr.toUInt(&ok);
        if (!ok) {
            return {};
        }
        if (hardened) {
            val |= kHardened;
        }
        result.append(val);
    }

    if (result.size() < 2 || result.size() > 5) {
        return {};
    }
    return result;
}

void LatticePlugin::startPolling(int intervalMs) {
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &LatticePlugin::scanDevices);
    }
    m_pollTimer->start(intervalMs);
}

void LatticePlugin::stopPolling() {
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}
