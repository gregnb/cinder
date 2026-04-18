#ifndef SIGNERMANAGER_H
#define SIGNERMANAGER_H

#include <QList>
#include <QObject>

class Signer;
class HardwareWalletPlugin;

class SignerManager : public QObject {
    Q_OBJECT
  public:
    explicit SignerManager(QObject* parent = nullptr);

    // ── Plugin registry ───────────────────────────────────
    void registerPlugin(HardwareWalletPlugin* plugin);
    QList<HardwareWalletPlugin*> plugins() const;

    // ── Active signer ─────────────────────────────────────
    Signer* activeSigner() const;
    void setActiveSigner(Signer* signer);

    // ── Convenience ───────────────────────────────────────
    QString activeAddress() const;
    bool isHardwareWallet() const;

  signals:
    void activeSignerChanged(Signer* signer);
    void signerDisconnected();

  private:
    Signer* m_activeSigner = nullptr;
    QList<HardwareWalletPlugin*> m_plugins;
};

#endif // SIGNERMANAGER_H
