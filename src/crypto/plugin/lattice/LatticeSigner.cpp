#include "crypto/plugin/lattice/LatticeSigner.h"
#include "crypto/plugin/lattice/LatticeCrypto.h"
#include "crypto/plugin/lattice/LatticeTransport.h"

#include <QDebug>
#include <QDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QtEndian>

namespace {
    // Lattice generic signing constants
    constexpr uint8_t kCurveEd25519 = 0x01;
    constexpr uint8_t kHashNone = 0x00;
    constexpr uint32_t kEncodingSolana = 0x02;
} // namespace

LatticeSigner::LatticeSigner(std::unique_ptr<LatticeTransport> transport,
                             const QList<uint32_t>& addressN, const QByteArray& pubkey,
                             const QString& address, QObject* parent)
    : Signer(parent), m_transport(std::move(transport)), m_addressN(addressN),
      m_cachedPubkey(pubkey), m_cachedAddress(address) {}

LatticeSigner::~LatticeSigner() {
    qDebug() << "[LatticeSigner] destructor, address=" << m_cachedAddress;
}

QString LatticeSigner::address() const { return m_cachedAddress; }
QByteArray LatticeSigner::publicKey() const { return m_cachedPubkey; }
QString LatticeSigner::lastError() const { return m_lastError; }
QString LatticeSigner::type() const { return QStringLiteral("gridplus"); }
bool LatticeSigner::isConnected() const { return m_connected; }
bool LatticeSigner::canExportSecret() const { return false; }

QByteArray LatticeSigner::buildSignPayload(const QByteArray& message) const {
    // Generic signing request format:
    // encoding(4 LE) + hash(1) + curve(1) + path indices(Nx4 BE) + omitPubkey(1) + dataLen(2 LE) +
    // data

    QByteArray payload;

    // Encoding type: SOLANA (4 bytes LE)
    char encBuf[4];
    qToLittleEndian(kEncodingSolana, encBuf);
    payload.append(encBuf, 4);

    // Hash type: NONE (1 byte)
    payload.append(static_cast<char>(kHashNone));

    // Curve type: ED25519 (1 byte)
    payload.append(static_cast<char>(kCurveEd25519));

    // Derivation path: each index as 4 bytes BE
    for (uint32_t idx : m_addressN) {
        char idxBuf[4];
        qToBigEndian(idx, idxBuf);
        payload.append(idxBuf, 4);
    }

    // Omit pubkey flag (1 byte): 0 = include pubkey in response
    payload.append(static_cast<char>(0x00));

    // Data length (2 bytes LE)
    uint16_t dataLen = static_cast<uint16_t>(message.size());
    char lenBuf[2];
    qToLittleEndian(dataLen, lenBuf);
    payload.append(lenBuf, 2);

    // Serialized transaction data
    payload.append(message);

    return payload;
}

QByteArray LatticeSigner::sign(const QByteArray& message) {
    m_lastError.clear();

    if (!m_transport || !m_transport->isOpen()) {
        m_lastError = QStringLiteral("Lattice1 not connected");
        m_connected = false;
        emit connectionChanged(false);
        return {};
    }

    // Build modal confirmation dialog
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("Lattice1 Confirmation"));
    dialog.setFixedSize(400, 160);
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* label = new QLabel(QStringLiteral("Confirm the transaction on your Lattice1..."));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // Run signing in background thread
    LatticeTransport* transport = m_transport.get();
    QByteArray signPayload = buildSignPayload(message);
    QString* errorOut = &m_lastError;

    auto future = QtConcurrent::run([transport, signPayload, errorOut]() -> QByteArray {
        LatticeResponse resp =
            transport->request(LatticeRequestType::kSign, signPayload, LatticeTimeout::kSign);

        if (!resp.isValid()) {
            *errorOut = QStringLiteral("Sign request failed: %1").arg(transport->lastError());
            return {};
        }

        // Parse response: skip 32 bytes (pubkey echo), next 64 bytes = ed25519 signature
        if (resp.payload.size() <
            LatticeCrypto::kEd25519KeySize + LatticeCrypto::kEd25519SignatureSize) {
            *errorOut = QStringLiteral("Sign response too short: %1").arg(resp.payload.size());
            return {};
        }

        QByteArray sig =
            resp.payload.mid(LatticeCrypto::kEd25519KeySize, LatticeCrypto::kEd25519SignatureSize);
        return sig;
    });

    auto* watcher = new QFutureWatcher<QByteArray>(&dialog);
    watcher->setFuture(future);

    QObject::connect(watcher, &QFutureWatcher<QByteArray>::finished, &dialog,
                     [&dialog]() { dialog.accept(); });

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, [this, &dialog]() {
        m_transport->close();
        dialog.reject();
    });

    QObject::connect(&dialog, &QDialog::rejected, this, [this]() {
        if (m_transport && m_transport->isOpen()) {
            m_transport->close();
        }
    });

    int result = dialog.exec();

    if (!future.isFinished()) {
        future.waitForFinished();
    }

    if (result != QDialog::Accepted) {
        m_lastError = QStringLiteral("Signing cancelled");
        return {};
    }

    QByteArray sig = future.result();

    if (sig.isEmpty()) {
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("No signature returned from device");
        }
        m_connected = false;
        emit connectionChanged(false);
        return {};
    }

    if (sig.size() != LatticeCrypto::kEd25519SignatureSize) {
        m_lastError = QStringLiteral("Invalid signature length: %1").arg(sig.size());
        return {};
    }

    return sig;
}

void LatticeSigner::signAsync(const QByteArray& message, QObject* context, SignCallback onDone) {
    m_lastError.clear();

    if (!m_transport || !m_transport->isOpen()) {
        m_lastError = QStringLiteral("Lattice1 not connected");
        m_connected = false;
        emit connectionChanged(false);
        if (onDone) {
            if (context) {
                QMetaObject::invokeMethod(
                    context, [onDone, err = m_lastError]() { onDone(QByteArray(), err); },
                    Qt::QueuedConnection);
            } else {
                onDone(QByteArray(), m_lastError);
            }
        }
        return;
    }

    QWidget* parentWidget = qobject_cast<QWidget*>(context);
    auto* dialog = new QDialog(parentWidget);
    dialog->setWindowTitle(QStringLiteral("Lattice1 Confirmation"));
    dialog->setFixedSize(400, 160);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* label = new QLabel(QStringLiteral("Confirm the transaction on your Lattice1..."));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    auto stateDone = std::make_shared<bool>(false);
    QPointer<QObject> safeContext = context;

    auto finish = [this, onDone, safeContext, stateDone, dialog](const QByteArray& sig,
                                                                 const QString& err) {
        if (*stateDone) {
            return;
        }
        *stateDone = true;
        m_lastError = err;
        if (!sig.isEmpty()) {
            m_connected = true;
        }
        if (dialog) {
            dialog->close();
            dialog->deleteLater();
        }
        if (!onDone) {
            return;
        }
        if (safeContext) {
            QMetaObject::invokeMethod(
                safeContext, [onDone, sig, err]() { onDone(sig, err); }, Qt::QueuedConnection);
            return;
        }
        onDone(sig, err);
    };

    LatticeTransport* transport = m_transport.get();
    QByteArray signPayload = buildSignPayload(message);

    auto future = QtConcurrent::run([transport, signPayload]() -> QPair<QByteArray, QString> {
        LatticeResponse resp =
            transport->request(LatticeRequestType::kSign, signPayload, LatticeTimeout::kSign);

        if (!resp.isValid()) {
            return qMakePair(QByteArray(),
                             QStringLiteral("Sign failed: %1").arg(transport->lastError()));
        }

        if (resp.payload.size() <
            LatticeCrypto::kEd25519KeySize + LatticeCrypto::kEd25519SignatureSize) {
            return qMakePair(QByteArray(),
                             QStringLiteral("Response too short: %1").arg(resp.payload.size()));
        }

        QByteArray sig =
            resp.payload.mid(LatticeCrypto::kEd25519KeySize, LatticeCrypto::kEd25519SignatureSize);
        if (sig.size() != LatticeCrypto::kEd25519SignatureSize) {
            return qMakePair(QByteArray(),
                             QStringLiteral("Invalid sig length: %1").arg(sig.size()));
        }
        return qMakePair(sig, QString());
    });

    auto* watcher = new QFutureWatcher<QPair<QByteArray, QString>>(dialog);
    QObject::connect(watcher, &QFutureWatcher<QPair<QByteArray, QString>>::finished, dialog,
                     [watcher, finish]() {
                         const auto result = watcher->result();
                         finish(result.first, result.second);
                     });

    QObject::connect(cancelBtn, &QPushButton::clicked, dialog, [this, finish]() {
        if (m_transport && m_transport->isOpen()) {
            m_transport->close();
        }
        finish(QByteArray(), QStringLiteral("Signing cancelled"));
    });
    QObject::connect(dialog, &QDialog::rejected, dialog, [this, finish]() {
        if (m_transport && m_transport->isOpen()) {
            m_transport->close();
        }
        finish(QByteArray(), QStringLiteral("Signing cancelled"));
    });

    watcher->setFuture(future);
    dialog->open();
}
