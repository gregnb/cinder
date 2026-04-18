#ifndef SOFTWARESIGNER_H
#define SOFTWARESIGNER_H

#include "Keypair.h"
#include "Signer.h"

class SoftwareSigner : public Signer {
    Q_OBJECT
  public:
    explicit SoftwareSigner(const Keypair& keypair, QObject* parent = nullptr);

    // ── Signer interface ──────────────────────────────────
    QString address() const override;
    QByteArray publicKey() const override;
    QByteArray sign(const QByteArray& message) override;
    QString lastError() const override;
    QString type() const override;
    bool isConnected() const override;
    bool canExportSecret() const override;

    // ── Software-only ─────────────────────────────────────
    // Access the underlying Keypair for secret key export
    // (e.g. TerminalPage "export-key" command).
    const Keypair& keypair() const;

  private:
    Keypair m_keypair;
    QString m_lastError;
};

#endif // SOFTWARESIGNER_H
