#ifndef TREZORPLUGIN_H
#define TREZORPLUGIN_H

#include "crypto/HardwareWalletPlugin.h"

#include <QList>
#include <QMap>
#include <QTimer>

class TrezorPlugin : public HardwareWalletPlugin {
    Q_OBJECT
  public:
    explicit TrezorPlugin(QObject* parent = nullptr);
    ~TrezorPlugin() override;

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

  private:
    // Trezor One (HID)
    static constexpr uint16_t kTrezorOneVID = 0x534C;
    static constexpr uint16_t kTrezorOnePID = 0x0001;
    // Trezor Model T / Safe 3 / Safe 5 (WebUSB/libusb)
    static constexpr uint16_t kTrezorVID = 0x1209;
    static constexpr uint16_t kTrezorPID = 0x53C1;

    enum class TransportType { HID, USB };

    QList<HWDeviceInfo> m_devices;
    QMap<QString, TransportType> m_transportTypes; // deviceId -> transport type
    QTimer* m_pollTimer = nullptr;
    bool m_hidInitialized = false;
};

#endif // TREZORPLUGIN_H
