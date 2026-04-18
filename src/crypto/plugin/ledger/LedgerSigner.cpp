#include "crypto/plugin/ledger/LedgerSigner.h"
#include "crypto/plugin/ledger/LedgerTransport.h"

#include <QDebug>
#include <QDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>

LedgerSigner::LedgerSigner(std::unique_ptr<LedgerTransport> transport,
                           const QByteArray& derivationPath, const QByteArray& pubkey,
                           const QString& address, const QString& model, QObject* parent)
    : Signer(parent), m_transport(std::move(transport)), m_derivationPath(derivationPath),
      m_cachedPubkey(pubkey), m_cachedAddress(address), m_model(model) {}

LedgerSigner::~LedgerSigner() {
    qDebug() << "[LedgerSigner] destructor called, address=" << m_cachedAddress;
}

QString LedgerSigner::address() const { return m_cachedAddress; }
QByteArray LedgerSigner::publicKey() const { return m_cachedPubkey; }
QString LedgerSigner::lastError() const { return m_lastError; }
QString LedgerSigner::type() const { return QStringLiteral("ledger"); }
bool LedgerSigner::isConnected() const { return m_connected; }
bool LedgerSigner::canExportSecret() const { return false; }

QByteArray LedgerSigner::sign(const QByteArray& message) {
    m_lastError.clear();

    qDebug() << "[LedgerSigner::sign] ──────────────────────────────────────";
    qDebug() << "[LedgerSigner::sign] message size=" << message.size()
             << "hex=" << message.toHex().left(64) << "...";

    if (!m_transport || !m_transport->isOpen()) {
        m_lastError = QStringLiteral("Ledger not connected");
        qWarning() << "[LedgerSigner::sign] Transport not open";
        m_connected = false;
        emit connectionChanged(false);
        return {};
    }
    qDebug() << "[LedgerSigner::sign] Transport is open, building dialog...";

    // ── Build modal confirmation dialog ──────────────────
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("Ledger Confirmation"));
    dialog.setFixedSize(400, 160);
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* label =
        new QLabel(QStringLiteral("Confirm the transaction on your Ledger %1...").arg(m_model));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // ── Run USB exchange in background thread ────────────
    // m_lastError is written from the lambda to propagate device errors to the caller.
    // This is safe because we always waitForFinished() before reading m_lastError.
    LedgerTransport* transport = m_transport.get();
    QByteArray derivPath = m_derivationPath;
    QString* errorOut = &m_lastError;

    qDebug() << "[LedgerSigner::sign] Launching background thread for USB exchange...";

    auto future = QtConcurrent::run([transport, derivPath, message, errorOut]() -> QByteArray {
        qDebug() << "[LedgerSigner::sign:bg] Background thread started";
        const int msgLen = message.size();

        if (msgLen <= LedgerApdu::kFirstChunkMax) {
            // Single APDU — fits in one packet
            qDebug() << "[LedgerSigner::sign:bg] Single-chunk sign, msgLen=" << msgLen;
            QByteArray data;
            data.reserve(1 + derivPath.size() + msgLen);
            data.append(static_cast<char>(0x01)); // signer count = 1
            data.append(derivPath);
            data.append(message);

            QByteArray apdu;
            apdu.append(static_cast<char>(LedgerApdu::kCLA));
            apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
            apdu.append(static_cast<char>(0x01));                 // P1 = single signer
            apdu.append(static_cast<char>(LedgerApdu::kP2_LAST)); // P2 = LAST
            apdu.append(static_cast<char>(data.size()));
            apdu.append(data);

            qDebug() << "[LedgerSigner::sign:bg] Sending sign APDU, size=" << apdu.size()
                     << "data_size=" << data.size();
            QByteArray resp = transport->exchange(apdu, LedgerTimeout::kSign);

            if (resp.isEmpty()) {
                QString err = transport->lastError();
                uint16_t sw = transport->lastStatusWord();
                qWarning() << "[LedgerSigner::sign:bg] Exchange failed:" << err << "SW=0x"
                           << QString::number(sw, 16);
                *errorOut = err;
                return {};
            }

            qDebug() << "[LedgerSigner::sign:bg] Got response, size=" << resp.size()
                     << "hex=" << resp.toHex();
            return resp;
        }

        // ── Multi-chunk signing ──────────────────────────
        qDebug() << "[LedgerSigner::sign:bg] Multi-chunk sign, msgLen=" << msgLen;
        int offset = 0;
        int firstDataSize = LedgerApdu::kFirstChunkMax;
        QByteArray response;

        // First APDU: signer count + path + first chunk
        {
            QByteArray data;
            data.reserve(1 + derivPath.size() + firstDataSize);
            data.append(static_cast<char>(0x01));
            data.append(derivPath);
            data.append(message.mid(0, firstDataSize));

            bool isLast = (firstDataSize >= msgLen);
            QByteArray apdu;
            apdu.append(static_cast<char>(LedgerApdu::kCLA));
            apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
            apdu.append(static_cast<char>(0x01));
            apdu.append(static_cast<char>(isLast ? LedgerApdu::kP2_LAST : LedgerApdu::kP2_MORE));
            apdu.append(static_cast<char>(data.size()));
            apdu.append(data);

            qDebug() << "[LedgerSigner::sign:bg] First chunk APDU, size=" << apdu.size()
                     << "isLast=" << isLast;
            response = transport->exchange(apdu, LedgerTimeout::kSign);
            if (response.isEmpty() && !isLast) {
                QString err = transport->lastError();
                qWarning() << "[LedgerSigner::sign:bg] First chunk failed:" << err;
                *errorOut = err;
                return {};
            }
            offset = firstDataSize;
        }

        // Continuation APDUs
        int chunkNum = 1;
        while (offset < msgLen) {
            int chunkSize = qMin(LedgerApdu::kContChunkMax, msgLen - offset);
            bool isLast = (offset + chunkSize >= msgLen);
            QByteArray chunk = message.mid(offset, chunkSize);

            QByteArray apdu;
            apdu.append(static_cast<char>(LedgerApdu::kCLA));
            apdu.append(static_cast<char>(LedgerApdu::kINS_SIGN));
            apdu.append(static_cast<char>(0x01));
            apdu.append(static_cast<char>(isLast ? LedgerApdu::kP2_LAST : LedgerApdu::kP2_MORE));
            apdu.append(static_cast<char>(chunk.size()));
            apdu.append(chunk);

            qDebug() << "[LedgerSigner::sign:bg] Continuation chunk" << chunkNum
                     << "size=" << chunk.size() << "isLast=" << isLast;
            response = transport->exchange(apdu, LedgerTimeout::kSign);
            if (response.isEmpty() && !isLast) {
                QString err = transport->lastError();
                qWarning() << "[LedgerSigner::sign:bg] Chunk" << chunkNum << "failed:" << err;
                *errorOut = err;
                return {};
            }
            offset += chunkSize;
            chunkNum++;
        }

        qDebug() << "[LedgerSigner::sign:bg] All chunks sent, response size=" << response.size();
        return response;
    });

    // ── Wire up completion / cancellation ────────────────
    auto* watcher = new QFutureWatcher<QByteArray>(&dialog);
    watcher->setFuture(future);

    QObject::connect(watcher, &QFutureWatcher<QByteArray>::finished, &dialog, [&dialog]() {
        qDebug() << "[LedgerSigner::sign] Watcher finished → accepting dialog";
        dialog.accept();
    });

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, [this, &dialog]() {
        qDebug() << "[LedgerSigner::sign] User cancelled via button";
        m_transport->close();
        dialog.reject();
    });

    // Handle ANY dialog rejection (Escape key, window close, etc.) —
    // must close transport so the background thread's hid_read_timeout unblocks.
    QObject::connect(&dialog, &QDialog::rejected, this, [this]() {
        qDebug() << "[LedgerSigner::sign] Dialog rejected — closing transport to unblock bg thread";
        if (m_transport && m_transport->isOpen()) {
            m_transport->close();
        }
    });

    // ── Block caller (event loop still runs) ─────────────
    qDebug() << "[LedgerSigner::sign] Showing modal dialog...";
    int result = dialog.exec();
    qDebug() << "[LedgerSigner::sign] Dialog returned:"
             << (result == QDialog::Accepted ? "Accepted" : "Rejected");

    // ── CRITICAL: Always wait for the background thread to finish ──
    // Without this, the caller might destroy the signer (and transport)
    // while the background thread is still running → use-after-free crash.
    if (!future.isFinished()) {
        qDebug() << "[LedgerSigner::sign] Waiting for background thread to finish...";
        future.waitForFinished();
        qDebug() << "[LedgerSigner::sign] Background thread finished";
    }

    if (result != QDialog::Accepted) {
        m_lastError = QStringLiteral("Signing cancelled");
        qDebug() << "[LedgerSigner::sign] Returning empty (cancelled)";
        return {};
    }

    QByteArray sig = future.result();
    qDebug() << "[LedgerSigner::sign] Signature result: size=" << sig.size()
             << "empty=" << sig.isEmpty();

    if (sig.isEmpty()) {
        // m_lastError may already be set by the lambda with the actual device error.
        // Only fall back to transport error or generic message if nothing was set.
        if (m_lastError.isEmpty()) {
            m_lastError = transport->lastError();
        }
        qWarning() << "[LedgerSigner::sign] Empty signature. Error:" << m_lastError;
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("No signature returned from device");
        }
        // Check for disconnect-related errors
        uint16_t sw = transport->lastStatusWord();
        if (sw == 0x6FAA || sw == 0x5515 || m_lastError.contains(QStringLiteral("read failed"))) {
            m_connected = false;
            emit connectionChanged(false);
        }
        return {};
    }

    if (sig.size() != LedgerCrypto::kEd25519SignatureSize) {
        qWarning() << "[LedgerSigner::sign] Invalid signature length:" << sig.size() << "(expected"
                   << LedgerCrypto::kEd25519SignatureSize << ")";
        m_lastError = QStringLiteral("Invalid signature length: %1").arg(sig.size());
        return {};
    }

    qDebug() << "[LedgerSigner::sign] SUCCESS — 64-byte signature obtained";
    qDebug() << "[LedgerSigner::sign] sig hex=" << sig.toHex();
    qDebug() << "[LedgerSigner::sign] ──── done ─────────────────────────────";
    return sig;
}
