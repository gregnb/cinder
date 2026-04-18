#ifndef LATTICEPLUGIN_H
#define LATTICEPLUGIN_H

#include "crypto/HardwareWalletPlugin.h"

#include <QList>
#include <QTimer>
#include <memory>

class LatticeTransport;

class LatticePlugin : public HardwareWalletPlugin {
    Q_OBJECT
  public:
    explicit LatticePlugin(QObject* parent = nullptr);
    ~LatticePlugin() override;

    // ── HardwareWalletPlugin interface ────────────────────
    QString pluginId() const override;
    QString displayName() const override;
    void scanDevices() override;
    QList<HWDeviceInfo> connectedDevices() const override;
    Signer* createSigner(const QString& deviceId, const QString& derivationPath,
                         QObject* parent = nullptr) override;

    // ── Lattice-specific ─────────────────────────────────
    void setDeviceId(const QString& deviceId);
    bool pair(const QString& pairingCode);
    bool isPaired() const;

    void startPolling(int intervalMs = 10000);
    void stopPolling();

  signals:
    void pairingRequired();
    void pairingSucceeded();
    void pairingFailed(const QString& error);

  private:
    static constexpr uint32_t kHardened = 0x80000000;

    QList<uint32_t> parseBip44Path(const QString& path) const;

    QList<HWDeviceInfo> m_devices;
    QString m_deviceId;
    QString m_appName;
    std::unique_ptr<LatticeTransport> m_transport;
    QTimer* m_pollTimer = nullptr;
};

#endif // LATTICEPLUGIN_H
