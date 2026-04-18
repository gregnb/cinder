#include "SoftwareSigner.h"

SoftwareSigner::SoftwareSigner(const Keypair& keypair, QObject* parent)
    : Signer(parent), m_keypair(keypair) {}

QString SoftwareSigner::address() const { return m_keypair.address(); }

QByteArray SoftwareSigner::publicKey() const { return m_keypair.publicKey(); }

QByteArray SoftwareSigner::sign(const QByteArray& message) {
    if (m_keypair.isNull()) {
        m_lastError = QStringLiteral("No keypair loaded");
        return {};
    }
    QByteArray sig = m_keypair.sign(message);
    if (sig.isEmpty()) {
        m_lastError = QStringLiteral("Signing failed");
        return {};
    }
    m_lastError.clear();
    return sig;
}

QString SoftwareSigner::lastError() const { return m_lastError; }

QString SoftwareSigner::type() const { return QStringLiteral("software"); }

bool SoftwareSigner::isConnected() const { return !m_keypair.isNull(); }

bool SoftwareSigner::canExportSecret() const { return true; }

const Keypair& SoftwareSigner::keypair() const { return m_keypair; }
