#ifndef LEDGERSIGNER_H
#define LEDGERSIGNER_H

#include "crypto/Signer.h"

#include <QByteArray>
#include <QString>
#include <memory>

class LedgerTransport;

class LedgerSigner : public Signer {
    Q_OBJECT
  public:
    // Takes ownership of transport. pubkey/address/model pre-cached by
    // LedgerPlugin::createSigner().
    LedgerSigner(std::unique_ptr<LedgerTransport> transport, const QByteArray& derivationPath,
                 const QByteArray& pubkey, const QString& address, const QString& model,
                 QObject* parent = nullptr);
    ~LedgerSigner() override;

    // ── Signer interface ──────────────────────────────────
    QString address() const override;
    QByteArray publicKey() const override;
    QByteArray sign(const QByteArray& message) override;
    QString lastError() const override;
    QString type() const override;
    bool isConnected() const override;
    bool canExportSecret() const override;

    QString model() const { return m_model; }

  private:
    std::unique_ptr<LedgerTransport> m_transport;
    QByteArray m_derivationPath; // Pre-encoded BIP44 path (17 bytes)
    QByteArray m_cachedPubkey;   // 32 bytes
    QString m_cachedAddress;
    QString m_model;
    QString m_lastError;
    bool m_connected = true;
};

#endif // LEDGERSIGNER_H
