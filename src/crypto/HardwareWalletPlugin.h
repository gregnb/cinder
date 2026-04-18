#ifndef HARDWAREWALLETPLUGIN_H
#define HARDWAREWALLETPLUGIN_H

#include <QList>
#include <QObject>
#include <QString>

class Signer;

struct HWDeviceInfo {
    QString deviceId;    // USB path or serial number
    QString displayName; // e.g. "Ledger Nano S"
    QString model;       // e.g. "nano_s", "nano_x", "trezor_one"

    // Tier 1 — HID/USB enumeration metadata
    QString serialNumber;
    QString manufacturer;
    QString productString;
    QString firmwareVersion; // HID release_number (Ledger) or Features (Trezor)

    // Tier 2 — Protocol-level device features (only valid when featuresQueried == true)
    bool featuresQueried = false;   // Set true after protocol-level query (e.g. Trezor Initialize)
    QString label;                  // User-set device name (Trezor only)
    bool initialized = false;       // Device has been set up
    bool pinProtected = false;      // PIN is enabled
    bool passphraseEnabled = false; // Passphrase is enabled
};

class HardwareWalletPlugin : public QObject {
    Q_OBJECT
  public:
    explicit HardwareWalletPlugin(QObject* parent = nullptr) : QObject(parent) {}
    ~HardwareWalletPlugin() override = default;

    // ── Plugin identity ───────────────────────────────────
    virtual QString pluginId() const = 0;    // "ledger", "trezor", "gridplus"
    virtual QString displayName() const = 0; // "Ledger", "Trezor", "GridPlus"

    // ── Device discovery ──────────────────────────────────
    virtual void scanDevices() = 0;
    virtual QList<HWDeviceInfo> connectedDevices() const = 0;

    // ── Device info query ─────────────────────────────────
    // Opens the device briefly to query protocol-level features (firmware,
    // PIN, label, etc.) and returns an enriched HWDeviceInfo. Updates the
    // stored device list. Default returns scan-time info only.
    virtual HWDeviceInfo queryDeviceInfo(const QString& deviceId) {
        for (const auto& d : connectedDevices()) {
            if (d.deviceId == deviceId) {
                return d;
            }
        }
        return {};
    }

    // ── Signer factory ────────────────────────────────────
    // Creates a Signer bound to a specific device + derivation path.
    // The caller takes ownership of the returned pointer.
    virtual Signer* createSigner(const QString& deviceId, const QString& derivationPath,
                                 QObject* parent = nullptr) = 0;

  signals:
    void devicesChanged();
    void deviceConnected(const HWDeviceInfo& device);
    void deviceDisconnected(const QString& deviceId);
};

#endif // HARDWAREWALLETPLUGIN_H
