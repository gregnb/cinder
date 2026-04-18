#ifndef TREZORSIGNER_H
#define TREZORSIGNER_H

#include "crypto/Signer.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <memory>

class TrezorTransport;

class TrezorSigner : public Signer {
    Q_OBJECT
  public:
    TrezorSigner(std::unique_ptr<TrezorTransport> transport, const QList<uint32_t>& addressN,
                 const QByteArray& pubkey, const QString& address, const QString& model,
                 QObject* parent = nullptr);
    ~TrezorSigner() override;

    // ── Signer interface ──────────────────────────────────
    QString address() const override;
    QByteArray publicKey() const override;
    QByteArray sign(const QByteArray& message) override;
    void signAsync(const QByteArray& message, QObject* context, SignCallback onDone) override;
    QString lastError() const override;
    QString type() const override;
    bool isConnected() const override;
    bool canExportSecret() const override;

    QString model() const { return m_model; }

  private:
    std::unique_ptr<TrezorTransport> m_transport;
    QList<uint32_t> m_addressN;
    QByteArray m_cachedPubkey;
    QString m_cachedAddress;
    QString m_model;
    QString m_lastError;
    bool m_connected = true;
};

#endif // TREZORSIGNER_H
