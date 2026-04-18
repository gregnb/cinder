#ifndef SIGNER_H
#define SIGNER_H

#include <QByteArray>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <functional>

class Signer : public QObject {
    Q_OBJECT
  public:
    using SignCallback = std::function<void(const QByteArray& signature, const QString& error)>;

    explicit Signer(QObject* parent = nullptr) : QObject(parent) {}
    ~Signer() override = default;

    // ── Identity ──────────────────────────────────────────
    virtual QString address() const = 0;
    virtual QByteArray publicKey() const = 0;

    // ── Signing (synchronous) ─────────────────────────────
    //
    // Software: instant return via Keypair::sign().
    // Hardware: shows modal "Confirm on device..." dialog,
    //   runs QDialog::exec() which blocks the caller but
    //   processes events (UI stays responsive, USB works).
    //   Returns when user confirms/cancels on device.
    //
    // Returns 64-byte Ed25519 signature, or empty QByteArray on failure.
    // On failure, call lastError() for a human-readable message.
    virtual QByteArray sign(const QByteArray& message) = 0;
    virtual void signAsync(const QByteArray& message, QObject* context, SignCallback onDone) {
        const QByteArray sig = sign(message);
        const QString err = lastError();
        if (!onDone) {
            return;
        }
        if (context) {
            QMetaObject::invokeMethod(
                context, [onDone, sig, err]() { onDone(sig, err); }, Qt::QueuedConnection);
            return;
        }
        onDone(sig, err);
    }
    virtual QString lastError() const = 0;

    // ── Capabilities ──────────────────────────────────────
    virtual QString type() const = 0;         // "software", "ledger", "trezor", ...
    virtual bool isConnected() const = 0;     // always true for software
    virtual bool canExportSecret() const = 0; // true only for software

  signals:
    void connectionChanged(bool connected);
};

#endif // SIGNER_H
