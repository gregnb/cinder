#include "features/lockscreen/LockScreenHandler.h"

#include "crypto/Keypair.h"
#include "crypto/WalletCrypto.h"
#include "db/WalletDb.h"
#ifdef Q_OS_MAC
#include "util/MacUtils.h"
#endif

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <sodium.h>

namespace {
    void wipeQString(QString& s) {
        if (!s.isEmpty()) {
            sodium_memzero(s.data(), static_cast<size_t>(s.size() * sizeof(QChar)));
            s.clear();
        }
    }
} // namespace

LockScreenHandler::LockScreenHandler(QObject* parent) : QObject(parent) {}

void LockScreenHandler::refreshBiometricState() {
    const auto wallets = WalletDb::getAllRecords();
    if (wallets.isEmpty()) {
        m_walletAddress.clear();
        m_biometricAvailable = false;
        emit biometricStateChanged(false);
        return;
    }

    m_walletAddress = wallets.first().address;
#ifdef Q_OS_MAC
    bool biometricEnabled = WalletDb::isBiometricEnabled(m_walletAddress);
    const bool hasStoredPassword =
        biometricEnabled ? hasStoredBiometricPassword(m_walletAddress) : false;
    if (biometricEnabled && !hasStoredPassword) {
        WalletDb::setBiometricEnabled(m_walletAddress, false);
        biometricEnabled = false;
    }
    m_biometricAvailable = isBiometricAvailable() && biometricEnabled && hasStoredPassword;
#else
    m_biometricAvailable = false;
#endif

    emit biometricStateChanged(m_biometricAvailable);
}

void LockScreenHandler::attemptUnlock(const QString& passwordInput) {
    QString password = passwordInput;
    if (password.isEmpty()) {
        emit unlockFailed(UnlockError::EmptyPassword);
        wipeQString(password);
        return;
    }

    const auto wallets = WalletDb::getAllRecords();
    if (wallets.isEmpty()) {
        emit unlockFailed(UnlockError::NoWallet);
        wipeQString(password);
        return;
    }

    const QString address = wallets.first().address;
    const auto wallet = WalletDb::getByAddressRecord(address);
    if (!wallet.has_value()) {
        emit unlockFailed(UnlockError::WalletNotFound);
        wipeQString(password);
        return;
    }

    WalletCrypto::EncryptedBlob blob;
    blob.salt = wallet->salt;
    blob.nonce = wallet->nonce;
    blob.ciphertext = wallet->ciphertext;

    const WalletKeyType walletType = wallet->walletKeyType();
    const HardwarePluginId hardwarePlugin = wallet->hardwarePluginId();
    const QString derivPath = wallet->derivationPath;

    emit unlockStarted();

    auto* watcher = new QFutureWatcher<QByteArray>(this);
    QString passwordCopy = password;

    connect(
        watcher, &QFutureWatcher<QByteArray>::finished, this,
        [this, watcher, address, walletType, hardwarePlugin, derivPath, passwordCopy]() mutable {
            QByteArray decrypted = watcher->result();
            watcher->deleteLater();

            if (decrypted.isEmpty()) {
                emit unlockFailed(UnlockError::IncorrectPassword);
                emit unlockFlowFinished();
                wipeQString(passwordCopy);
                return;
            }

            UnlockResult result;
            result.address = address;
            result.walletType = walletType;
            result.password = passwordCopy;
            wipeQString(passwordCopy);

            if (hardwarePlugin != HardwarePluginId::None) {
                result.publicKey = decrypted;
                result.hardwarePlugin = hardwarePlugin;
                result.derivationPath = derivPath;
            } else {
                const Keypair kp = Keypair::fromSecretKey(decrypted);
                result.publicKey = kp.publicKey();
                result.keypair = kp;
            }

            sodium_memzero(decrypted.data(), decrypted.size());
            emit unlockSucceeded(result);
            emit unlockFlowFinished();
        });

    watcher->setFuture(QtConcurrent::run([blob, password]() mutable {
        const QByteArray decrypted = WalletCrypto::decrypt(blob, password);
        wipeQString(password);
        return decrypted;
    }));

    wipeQString(password);
}

void LockScreenHandler::attemptBiometricUnlock() {
#ifdef Q_OS_MAC
    if (!m_biometricAvailable || m_walletAddress.isEmpty()) {
        return;
    }

    emit unlockStarted();

    auto* watcher = new QFutureWatcher<QPair<bool, UnlockResult>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, UnlockResult>>::finished, this, [this, watcher]() {
        auto [success, result] = watcher->result();
        watcher->deleteLater();

        if (success) {
            emit unlockSucceeded(result);
        } else {
            emit unlockFailed(UnlockError::IncorrectPassword);
        }

        emit unlockFlowFinished();
    });

    // Read wallet record on main thread (QSqlDatabase is not thread-safe)
    const QString address = m_walletAddress;
    const auto wallet = WalletDb::getByAddressRecord(address);
    if (!wallet.has_value()) {
        emit unlockFailed(UnlockError::WalletNotFound);
        emit unlockFlowFinished();
        return;
    }

    WalletCrypto::EncryptedBlob blob;
    blob.salt = wallet->salt;
    blob.nonce = wallet->nonce;
    blob.ciphertext = wallet->ciphertext;
    const WalletKeyType walletType = wallet->walletKeyType();
    const HardwarePluginId hardwarePlugin = wallet->hardwarePluginId();
    const QString derivPath = wallet->derivationPath;

    watcher->setFuture(QtConcurrent::run(
        [address, blob, walletType, hardwarePlugin, derivPath]() -> QPair<bool, UnlockResult> {
            QString password;
            if (!retrieveBiometricPassword(address, password)) {
                if (!hasStoredBiometricPassword(address)) {
                    WalletDb::setBiometricEnabled(address, false);
                }
                return {false, {}};
            }

            QByteArray decrypted = WalletCrypto::decrypt(blob, password);

            if (decrypted.isEmpty()) {
                wipeQString(password);
                deleteBiometricPassword(address);
                WalletDb::setBiometricEnabled(address, false);
                return {false, {}};
            }

            UnlockResult result;
            result.address = address;
            result.walletType = walletType;
            result.password = password;
            wipeQString(password);

            if (hardwarePlugin != HardwarePluginId::None) {
                result.publicKey = decrypted;
                result.hardwarePlugin = hardwarePlugin;
                result.derivationPath = derivPath;
            } else {
                const Keypair kp = Keypair::fromSecretKey(decrypted);
                result.publicKey = kp.publicKey();
                result.keypair = kp;
            }

            sodium_memzero(decrypted.data(), decrypted.size());
            return {true, result};
        }));
#endif
}
