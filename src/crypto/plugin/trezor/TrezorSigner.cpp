#include "crypto/plugin/trezor/TrezorSigner.h"
#include "crypto/plugin/trezor/TrezorProtobuf.h"
#include "crypto/plugin/trezor/TrezorTransport.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>

TrezorSigner::TrezorSigner(std::unique_ptr<TrezorTransport> transport,
                           const QList<uint32_t>& addressN, const QByteArray& pubkey,
                           const QString& address, const QString& model, QObject* parent)
    : Signer(parent), m_transport(std::move(transport)), m_addressN(addressN),
      m_cachedPubkey(pubkey), m_cachedAddress(address), m_model(model) {}

TrezorSigner::~TrezorSigner() {}

QString TrezorSigner::address() const { return m_cachedAddress; }
QByteArray TrezorSigner::publicKey() const { return m_cachedPubkey; }
QString TrezorSigner::lastError() const { return m_lastError; }
QString TrezorSigner::type() const { return QStringLiteral("trezor"); }
bool TrezorSigner::isConnected() const { return m_connected; }
bool TrezorSigner::canExportSecret() const { return false; }

QByteArray TrezorSigner::sign(const QByteArray& message) {
    m_lastError.clear();

    if (!m_transport || !m_transport->isOpen()) {
        m_lastError = QStringLiteral("Trezor not connected");
        qWarning() << "[TrezorSigner::sign] Transport not open";
        m_connected = false;
        emit connectionChanged(false);
        return {};
    }

    // Build modal confirmation dialog
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("Trezor Confirmation"));
    dialog.setFixedSize(400, 160);
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* label =
        new QLabel(QStringLiteral("Confirm the transaction on your Trezor %1...").arg(m_model));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // Run USB exchange in background thread.
    // m_lastError is written from the lambda to propagate device errors to the caller.
    // This is safe because m_lastError is only read after the future completes.
    TrezorTransport* transport = m_transport.get();
    QList<uint32_t> addressN = m_addressN;
    QString* errorOut = &m_lastError;

    auto future = QtConcurrent::run([transport, addressN, message, errorOut]() -> QByteArray {
        // Re-initialize session before signing
        TrezorResponse initResp = transport->call(
            TrezorMsg::Initialize, TrezorProtobuf::encodeInitialize(), TrezorTimeout::kInit);
        if (initResp.msgType != TrezorMsg::Features) {
            QString err = initResp.isValid()
                              ? QStringLiteral("Initialize: unexpected response type %1")
                                    .arg(initResp.msgType)
                              : QStringLiteral("Initialize failed: %1").arg(transport->lastError());
            qWarning() << "[TrezorSigner::sign]" << err;
            *errorOut = err;
            return {};
        }

        TrezorProtobuf::decodeFeatures(initResp.data);

        // Send SolanaSignTx
        QByteArray signMsg = TrezorProtobuf::encodeSolanaSignTx(addressN, message);

        TrezorResponse resp =
            transport->call(TrezorMsg::SolanaSignTx, signMsg, TrezorTimeout::kSign);

        if (!resp.isValid()) {
            QString err = QStringLiteral("SolanaSignTx: %1").arg(transport->lastError());
            qWarning() << "[TrezorSigner::sign]" << err;
            *errorOut = err;
            return {};
        }

        // Drive interaction loop
        resp = TrezorProtobuf::driveInteraction(transport, std::move(resp), TrezorTimeout::kSign);

        if (resp.msgType == TrezorMsg::Failure) {
            TrezorFailure fail = TrezorProtobuf::decodeFailure(resp.data);
            QString err = QStringLiteral("Device rejected: %1").arg(fail.message);
            qWarning() << "[TrezorSigner::sign]" << err << "code=" << fail.code;
            *errorOut = err;
            return {};
        }

        if (resp.msgType != TrezorMsg::SolanaTxSignature) {
            QString err = QStringLiteral("Unexpected response type %1 (expected %2)")
                              .arg(resp.msgType)
                              .arg(TrezorMsg::SolanaTxSignature);
            qWarning() << "[TrezorSigner::sign]" << err;
            *errorOut = err;
            return {};
        }

        return TrezorProtobuf::decodeSolanaTxSignature(resp.data);
    });

    // Wire up completion / cancellation
    auto* watcher = new QFutureWatcher<QByteArray>(&dialog);
    watcher->setFuture(future);

    QObject::connect(watcher, &QFutureWatcher<QByteArray>::finished, &dialog,
                     [&dialog]() { dialog.accept(); });

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, [this, &dialog]() {
        m_transport->close();
        dialog.reject();
    });

    // Handle ANY dialog rejection (Escape key, window close, etc.) —
    // must close transport so the background thread's blocking read unblocks.
    QObject::connect(&dialog, &QDialog::rejected, this, [this]() {
        if (m_transport && m_transport->isOpen()) {
            m_transport->close();
        }
    });

    int result = dialog.exec();

    // ── CRITICAL: Always wait for the background thread to finish ──
    // Without this, the caller might destroy the signer (and transport)
    // while the background thread is still running → use-after-free crash.
    if (!future.isFinished()) {
        future.waitForFinished();
    }

    if (result != QDialog::Accepted) {
        m_lastError = QStringLiteral("Signing cancelled");
        return {};
    }

    QByteArray sig = future.result();

    if (sig.isEmpty()) {
        // m_lastError may already be set by the lambda with the actual device error.
        // Only fall back to transport error or generic message if nothing was set.
        if (m_lastError.isEmpty()) {
            m_lastError = transport->lastError();
        }
        qWarning() << "[TrezorSigner::sign] Empty signature. Error:" << m_lastError;
        if (m_lastError.isEmpty()) {
            m_lastError = QStringLiteral("No signature returned from device");
        }
        m_connected = false;
        emit connectionChanged(false);
        return {};
    }

    if (sig.size() != TrezorCrypto::kEd25519SignatureSize) {
        qWarning() << "[TrezorSigner::sign] Invalid signature length:" << sig.size() << "(expected"
                   << TrezorCrypto::kEd25519SignatureSize << ")";
        m_lastError = QStringLiteral("Invalid signature length: %1").arg(sig.size());
        return {};
    }
    return sig;
}

void TrezorSigner::signAsync(const QByteArray& message, QObject* context, SignCallback onDone) {
    m_lastError.clear();

    if (!m_transport || !m_transport->isOpen()) {
        m_lastError = QStringLiteral("Trezor not connected");
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
    dialog->setWindowTitle(QStringLiteral("Trezor Confirmation"));
    dialog->setFixedSize(400, 160);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* label =
        new QLabel(QStringLiteral("Confirm the transaction on your Trezor %1...").arg(m_model));
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

    TrezorTransport* transport = m_transport.get();
    QList<uint32_t> addressN = m_addressN;
    auto future = QtConcurrent::run([transport, addressN, message]() -> QPair<QByteArray, QString> {
        TrezorResponse initResp = transport->call(
            TrezorMsg::Initialize, TrezorProtobuf::encodeInitialize(), TrezorTimeout::kInit);
        if (initResp.msgType != TrezorMsg::Features) {
            QString err = initResp.isValid()
                              ? QStringLiteral("Initialize: unexpected response type %1")
                                    .arg(initResp.msgType)
                              : QStringLiteral("Initialize failed: %1").arg(transport->lastError());
            return qMakePair(QByteArray(), err);
        }

        QByteArray signMsg = TrezorProtobuf::encodeSolanaSignTx(addressN, message);
        TrezorResponse resp =
            transport->call(TrezorMsg::SolanaSignTx, signMsg, TrezorTimeout::kSign);
        if (!resp.isValid()) {
            return qMakePair(QByteArray(),
                             QStringLiteral("SolanaSignTx: %1").arg(transport->lastError()));
        }

        resp = TrezorProtobuf::driveInteraction(transport, std::move(resp), TrezorTimeout::kSign);
        if (resp.msgType == TrezorMsg::Failure) {
            TrezorFailure fail = TrezorProtobuf::decodeFailure(resp.data);
            return qMakePair(QByteArray(), QStringLiteral("Device rejected: %1").arg(fail.message));
        }
        if (resp.msgType != TrezorMsg::SolanaTxSignature) {
            return qMakePair(QByteArray(),
                             QStringLiteral("Unexpected response type %1 (expected %2)")
                                 .arg(resp.msgType)
                                 .arg(TrezorMsg::SolanaTxSignature));
        }

        QByteArray sig = TrezorProtobuf::decodeSolanaTxSignature(resp.data);
        if (sig.isEmpty()) {
            return qMakePair(QByteArray(), QStringLiteral("No signature returned from device"));
        }
        if (sig.size() != TrezorCrypto::kEd25519SignatureSize) {
            return qMakePair(QByteArray(),
                             QStringLiteral("Invalid signature length: %1").arg(sig.size()));
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
