#ifndef LEDGERPLUGIN_H
#define LEDGERPLUGIN_H

#include "crypto/HardwareWalletPlugin.h"

#include <QList>
#include <QTimer>

class LedgerPlugin : public HardwareWalletPlugin {
    Q_OBJECT
  public:
    explicit LedgerPlugin(QObject* parent = nullptr);
    ~LedgerPlugin() override;

    // ── HardwareWalletPlugin interface ────────────────────
    QString pluginId() const override;
    QString displayName() const override;
    void scanDevices() override;
    QList<HWDeviceInfo> connectedDevices() const override;
    HWDeviceInfo queryDeviceInfo(const QString& deviceId) override;
    Signer* createSigner(const QString& deviceId, const QString& derivationPath,
                         QObject* parent = nullptr) override;

    void startPolling(int intervalMs = 2500);
    void stopPolling();

    // Encode a BIP44 derivation path string to binary format.
    // e.g. "m/44'/501'/0'/0'" → [0x04][0x8000002C][0x800001F5][0x80000000][0x80000000]
    static QByteArray encodeDerivationPath(const QString& path);

  private:
    static constexpr uint16_t kLedgerVID = 0x2C97;

    static QString modelFromPID(uint16_t pid);

    QList<HWDeviceInfo> m_devices;
    QTimer* m_pollTimer = nullptr;
    bool m_hidInitialized = false;
};

#endif // LEDGERPLUGIN_H
