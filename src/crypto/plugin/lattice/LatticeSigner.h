#ifndef LATTICESIGNER_H
#define LATTICESIGNER_H

#include "crypto/Signer.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <memory>

class LatticeTransport;

class LatticeSigner : public Signer {
    Q_OBJECT
  public:
    LatticeSigner(std::unique_ptr<LatticeTransport> transport, const QList<uint32_t>& addressN,
                  const QByteArray& pubkey, const QString& address, QObject* parent = nullptr);
    ~LatticeSigner() override;

    // ── Signer interface ──────────────────────────────────
    QString address() const override;
    QByteArray publicKey() const override;
    QByteArray sign(const QByteArray& message) override;
    void signAsync(const QByteArray& message, QObject* context, SignCallback onDone) override;
    QString lastError() const override;
    QString type() const override;
    bool isConnected() const override;
    bool canExportSecret() const override;

  private:
    QByteArray buildSignPayload(const QByteArray& message) const;

    std::unique_ptr<LatticeTransport> m_transport;
    QList<uint32_t> m_addressN;
    QByteArray m_cachedPubkey;
    QString m_cachedAddress;
    QString m_lastError;
    bool m_connected = true;
};

#endif // LATTICESIGNER_H
