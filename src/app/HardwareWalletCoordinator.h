#ifndef HARDWAREWALLETCOORDINATOR_H
#define HARDWAREWALLETCOORDINATOR_H

#include "models/WalletTypes.h"

#include <QObject>
#include <QString>
#include <functional>

class QWidget;
class QLabel;
class Signer;
class SignerManager;
class HardwareWalletPlugin;

class HardwareWalletCoordinator : public QObject {
    Q_OBJECT

  public:
    HardwareWalletCoordinator(QWidget* parentWindow, SignerManager* signerManager,
                              std::function<void(Signer*)> onSignerReady,
                              QObject* parent = nullptr);

    void setReconnectBanner(QWidget* banner, QLabel* bannerText);
    void setPendingSigner(Signer* signer);
    Signer* takePendingSignerIfMatches(const QString& address);

    void requestSetupConnection(HardwareWalletPlugin* plugin, const QString& deviceId,
                                const QString& derivPath, const QString& failureMessage,
                                const std::function<void(Signer*)>& onSuccess,
                                const std::function<void(const QString&)>& onFailure);

    void connectHardwareSigner(HardwarePluginId pluginId, const QString& derivPath);
    void showReconnectBanner(const QString& activeAddress, const QString& deviceName = QString());
    void hideReconnectBanner();

  private:
    HardwareWalletPlugin* findPlugin(HardwarePluginId pluginId) const;
    QWidget* parentWindow() const;

    QWidget* m_parentWindow = nullptr;
    SignerManager* m_signerManager = nullptr;
    std::function<void(Signer*)> m_onSignerReady;
    QWidget* m_reconnectBanner = nullptr;
    QLabel* m_reconnectBannerText = nullptr;
    Signer* m_pendingSigner = nullptr;
};

#endif // HARDWAREWALLETCOORDINATOR_H
