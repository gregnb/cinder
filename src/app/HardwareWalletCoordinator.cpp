#include "app/HardwareWalletCoordinator.h"

#include "crypto/HardwareWalletPlugin.h"
#include "crypto/Signer.h"
#include "crypto/SignerManager.h"
#include "db/WalletDb.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {

    void repolishWidget(QWidget* widget) {
        if (!widget) {
            return;
        }
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

} // namespace

HardwareWalletCoordinator::HardwareWalletCoordinator(QWidget* parentWindow,
                                                     SignerManager* signerManager,
                                                     std::function<void(Signer*)> onSignerReady,
                                                     QObject* parent)
    : QObject(parent), m_parentWindow(parentWindow), m_signerManager(signerManager),
      m_onSignerReady(std::move(onSignerReady)) {}

void HardwareWalletCoordinator::setReconnectBanner(QWidget* banner, QLabel* bannerText) {
    m_reconnectBanner = banner;
    m_reconnectBannerText = bannerText;
}

void HardwareWalletCoordinator::setPendingSigner(Signer* signer) { m_pendingSigner = signer; }

Signer* HardwareWalletCoordinator::takePendingSignerIfMatches(const QString& address) {
    if (!m_pendingSigner || m_pendingSigner->address() != address) {
        return nullptr;
    }

    Signer* signer = m_pendingSigner;
    m_pendingSigner = nullptr;
    return signer;
}

void HardwareWalletCoordinator::requestSetupConnection(
    HardwareWalletPlugin* plugin, const QString& deviceId, const QString& derivPath,
    const QString& failureMessage, const std::function<void(Signer*)>& onSuccess,
    const std::function<void(const QString&)>& onFailure) {
    if (!plugin) {
        if (onFailure) {
            onFailure(failureMessage);
        }
        return;
    }

    Signer* signer = plugin->createSigner(deviceId, derivPath, parentWindow());
    if (!signer) {
        if (onFailure) {
            onFailure(failureMessage);
        }
        return;
    }

    m_pendingSigner = signer;
    if (onSuccess) {
        onSuccess(signer);
    }
}

void HardwareWalletCoordinator::connectHardwareSigner(HardwarePluginId pluginId,
                                                      const QString& derivPath) {
    HardwareWalletPlugin* plugin = findPlugin(pluginId);
    if (!plugin) {
        qWarning() << "Hardware wallet plugin not found:" << toStorageString(pluginId);
        return;
    }

    const auto devices = plugin->connectedDevices();
    if (devices.isEmpty()) {
        connect(
            plugin, &HardwareWalletPlugin::deviceConnected, this,
            [this, pluginId, derivPath](const HWDeviceInfo&) {
                connectHardwareSigner(pluginId, derivPath);
            },
            static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
        return;
    }

    QString deviceName = plugin->displayName();
    auto* dialog = new QDialog(parentWindow());
    dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dialog->setObjectName("hardwareConnectDialog");
    dialog->setFixedSize(420, 200);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(32, 28, 32, 24);

    auto* titleLabel = new QLabel(tr("Connecting to your %1...").arg(deviceName));
    titleLabel->setProperty("uiClass", "hardwareConnectTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto* subtitleLabel =
        new QLabel(tr("Unlock your device and confirm the connection when prompted."));
    subtitleLabel->setProperty("uiClass", "hardwareConnectSubtitle");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    layout->addSpacing(8);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* actionBtn = new QPushButton(tr("Cancel"));
    actionBtn->setCursor(Qt::PointingHandCursor);
    actionBtn->setFixedSize(120, 40);
    actionBtn->setProperty("uiClass", "hardwareConnectCancelBtn");
    btnLayout->addWidget(actionBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    QString deviceId = devices.first().deviceId;
    auto future = QtConcurrent::run([plugin, deviceId, derivPath]() -> Signer* {
        return plugin->createSigner(deviceId, derivPath, nullptr);
    });

    auto* watcher = new QFutureWatcher<Signer*>(dialog);
    auto stateDone = std::make_shared<bool>(false);

    auto finish = [this, dialog, titleLabel, subtitleLabel, actionBtn, deviceName,
                   stateDone](Signer* signer) {
        if (*stateDone) {
            delete signer;
            return;
        }
        *stateDone = true;

        if (signer) {
            signer->setParent(parentWindow());
            if (m_onSignerReady) {
                m_onSignerReady(signer);
            }
            hideReconnectBanner();
            dialog->close();
            dialog->deleteLater();
        } else {
            titleLabel->setText(tr("Connection Failed"));
            titleLabel->setProperty("tone", "error");
            subtitleLabel->setText(
                tr("Could not connect to your %1.\nMake sure it is unlocked and ready.")
                    .arg(deviceName));
            subtitleLabel->setProperty("tone", "error");
            repolishWidget(titleLabel);
            repolishWidget(subtitleLabel);
            actionBtn->setText(tr("OK"));
            actionBtn->disconnect();
            QObject::connect(actionBtn, &QPushButton::clicked, dialog, [dialog]() {
                dialog->close();
                dialog->deleteLater();
            });
        }
    };

    connect(watcher, &QFutureWatcher<Signer*>::finished, dialog,
            [watcher, finish]() { finish(watcher->result()); });

    connect(actionBtn, &QPushButton::clicked, dialog, [dialog]() { dialog->reject(); });
    connect(dialog, &QDialog::rejected, dialog, [stateDone, dialog]() {
        if (!*stateDone) {
            *stateDone = true;
        }
        dialog->deleteLater();
    });

    watcher->setFuture(future);
    dialog->open();
}

void HardwareWalletCoordinator::showReconnectBanner(const QString& activeAddress,
                                                    const QString& deviceName) {
    if (!m_reconnectBanner) {
        return;
    }

    if (m_reconnectBannerText) {
        QString name = deviceName;
        if (name.isEmpty()) {
            auto wallet = WalletDb::getByAddressRecord(activeAddress);
            if (wallet) {
                name = hardwarePluginDisplayName(wallet->hardwarePluginId());
            }
        }
        if (!name.isEmpty()) {
            m_reconnectBannerText->setText(tr("Connect your %1 to sign transactions").arg(name));
        } else {
            m_reconnectBannerText->setText(tr("Connect your hardware wallet to sign transactions"));
        }
    }

    m_reconnectBanner->show();
}

void HardwareWalletCoordinator::hideReconnectBanner() {
    if (m_reconnectBanner) {
        m_reconnectBanner->hide();
    }
}

HardwareWalletPlugin* HardwareWalletCoordinator::findPlugin(HardwarePluginId pluginId) const {
    if (!m_signerManager) {
        return nullptr;
    }

    for (auto* plugin : m_signerManager->plugins()) {
        if (parseHardwarePluginId(plugin->pluginId()) == pluginId) {
            return plugin;
        }
    }
    return nullptr;
}

QWidget* HardwareWalletCoordinator::parentWindow() const { return m_parentWindow; }
