#include "SignerManager.h"
#include "HardwareWalletPlugin.h"
#include "Signer.h"

SignerManager::SignerManager(QObject* parent) : QObject(parent) {}

// ── Plugin registry ──────────────────────────────────────

void SignerManager::registerPlugin(HardwareWalletPlugin* plugin) {
    if (!plugin || m_plugins.contains(plugin)) {
        return;
    }
    plugin->setParent(this);
    m_plugins.append(plugin);
}

QList<HardwareWalletPlugin*> SignerManager::plugins() const { return m_plugins; }

// ── Active signer ────────────────────────────────────────

Signer* SignerManager::activeSigner() const { return m_activeSigner; }

void SignerManager::setActiveSigner(Signer* signer) {
    if (m_activeSigner == signer) {
        return;
    }

    // Disconnect old signer's signals
    if (m_activeSigner) {
        disconnect(m_activeSigner, nullptr, this, nullptr);
    }

    m_activeSigner = signer;

    // Forward disconnection events
    if (m_activeSigner) {
        connect(m_activeSigner, &Signer::connectionChanged, this, [this](bool connected) {
            if (!connected) {
                emit signerDisconnected();
            }
        });
    }

    emit activeSignerChanged(signer);
}

// ── Convenience ──────────────────────────────────────────

QString SignerManager::activeAddress() const {
    return m_activeSigner ? m_activeSigner->address() : QString();
}

bool SignerManager::isHardwareWallet() const {
    return m_activeSigner && m_activeSigner->type() != QStringLiteral("software");
}
