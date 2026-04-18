#include "WalletsHandler.h"
#include "crypto/Keypair.h"
#include "crypto/WalletCrypto.h"
#include <QCoreApplication>
#include <QPixmap>
#include <sodium.h>

namespace {
    const QColor kAvatarColors[] = {
        QColor("#14F195"), QColor("#9945FF"), QColor("#4DA1FF"), QColor("#FF6B6B"),
        QColor("#FFB347"), QColor("#B8E986"), QColor("#FF85C0"), QColor("#7AFBFF"),
    };
    constexpr int kNumAvatarColors = sizeof(kAvatarColors) / sizeof(kAvatarColors[0]);
} // namespace

QList<WalletSummaryRecord> WalletsHandler::listWallets() const { return WalletDb::getAllRecords(); }

WalletRecord WalletsHandler::loadWalletDetail(const WalletSummaryRecord& wallet) const {
    const auto full = WalletDb::getByAddressRecord(wallet.address);
    if (full.has_value()) {
        return *full;
    }

    WalletRecord fallback;
    fallback.id = wallet.id;
    fallback.label = wallet.label;
    fallback.address = wallet.address;
    fallback.keyType = wallet.keyType;
    fallback.accountIndex = wallet.accountIndex;
    fallback.parentWalletId = wallet.parentWalletId;
    fallback.createdAt = wallet.createdAt;
    fallback.avatarPath = wallet.avatarPath;
    return fallback;
}

bool WalletsHandler::renameWallet(int walletId, const QString& label) const {
    return WalletDb::updateLabel(walletId, label);
}

bool WalletsHandler::removeWallet(int walletId) const { return WalletDb::deleteWallet(walletId); }

bool WalletsHandler::saveAvatar(int walletId, const QString& address, const QString& sourcePath,
                                QString& relativePathOut) const {
    if (walletId <= 0 || address.isEmpty()) {
        return false;
    }

    QPixmap pm(sourcePath);
    if (pm.isNull()) {
        return false;
    }
    if (pm.width() > 256 || pm.height() > 256) {
        pm = pm.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const QString relName = address + ".png";
    const QString destPath = WalletDb::avatarDir() + "/" + relName;
    if (!pm.save(destPath, "PNG")) {
        return false;
    }
    if (!WalletDb::setAvatarPath(walletId, relName)) {
        return false;
    }

    relativePathOut = relName;
    return true;
}

QString WalletsHandler::typeBadge(const WalletSummaryRecord& account) const {
    switch (account.walletKeyType()) {
        case WalletKeyType::Ledger:
            return QCoreApplication::translate("WalletsHandler", "Ledger");
        case WalletKeyType::Trezor:
            return QCoreApplication::translate("WalletsHandler", "Trezor");
        case WalletKeyType::Lattice:
            return QCoreApplication::translate("WalletsHandler", "Lattice");
        case WalletKeyType::PrivateKey:
            return QCoreApplication::translate("WalletsHandler", "Imported");
        case WalletKeyType::Mnemonic:
        case WalletKeyType::Unknown:
            return QCoreApplication::translate("WalletsHandler", "HD Wallet");
    }
}

QString WalletsHandler::derivationPath(const WalletRecord& account) const {
    switch (account.walletKeyType()) {
        case WalletKeyType::Ledger:
        case WalletKeyType::Trezor:
        case WalletKeyType::Lattice:
            return account.derivationPath;
        case WalletKeyType::Mnemonic:
            if (account.accountIndex >= 0) {
                return QString("m/44'/501'/%1'/0'").arg(account.accountIndex);
            }
            return {};
        case WalletKeyType::PrivateKey:
        case WalletKeyType::Unknown:
            return {};
    }
}

bool WalletsHandler::showDerivationPath(const WalletRecord& account) const {
    return !derivationPath(account).isEmpty();
}

bool WalletsHandler::isSoftwareWallet(const WalletRecord& account) const {
    const WalletKeyType keyType = account.walletKeyType();
    return keyType == WalletKeyType::Mnemonic || keyType == WalletKeyType::PrivateKey;
}

bool WalletsHandler::hasRecoveryPhrase(int walletId) const {
    return WalletDb::getMnemonicBlobRecord(walletId).has_value();
}

std::optional<QString> WalletsHandler::revealPrivateKey(const QString& address,
                                                        const QString& password,
                                                        QString& errorOut) const {
    if (password.isEmpty()) {
        errorOut = "Please enter your password.";
        return std::nullopt;
    }

    const auto wallet = WalletDb::getByAddressRecord(address);
    if (!wallet.has_value()) {
        errorOut = "Wallet record not found.";
        return std::nullopt;
    }

    WalletCrypto::EncryptedBlob blob;
    blob.salt = wallet->salt;
    blob.nonce = wallet->nonce;
    blob.ciphertext = wallet->ciphertext;

    QByteArray secretKey = WalletCrypto::decrypt(blob, password);
    if (secretKey.isEmpty()) {
        errorOut = "Incorrect password.";
        return std::nullopt;
    }

    const Keypair kp = Keypair::fromSecretKey(secretKey);
    sodium_memzero(secretKey.data(), static_cast<size_t>(secretKey.size()));
    return kp.toBase58();
}

std::optional<QStringList> WalletsHandler::revealRecoveryPhrase(int walletId,
                                                                const QString& password,
                                                                QString& errorOut) const {
    if (password.isEmpty()) {
        errorOut = "Please enter your password.";
        return std::nullopt;
    }

    const auto mnBlob = WalletDb::getMnemonicBlobRecord(walletId);
    if (!mnBlob.has_value()) {
        errorOut = "No recovery phrase stored for this wallet.";
        return std::nullopt;
    }

    WalletCrypto::EncryptedBlob blob;
    blob.salt = mnBlob->salt;
    blob.nonce = mnBlob->nonce;
    blob.ciphertext = mnBlob->ciphertext;

    QByteArray mnemonicUtf8 = WalletCrypto::decrypt(blob, password);
    if (mnemonicUtf8.isEmpty()) {
        errorOut = "Incorrect password.";
        return std::nullopt;
    }

    QString mnemonic = QString::fromUtf8(mnemonicUtf8);
    sodium_memzero(mnemonicUtf8.data(), static_cast<size_t>(mnemonicUtf8.size()));
    QStringList words = mnemonic.split(' ');
    sodium_memzero(mnemonic.data(), static_cast<size_t>(mnemonic.size() * sizeof(QChar)));
    return words;
}

QColor WalletsHandler::avatarColor(const QString& address) const {
    if (address.isEmpty()) {
        return kAvatarColors[0];
    }

    uint hash = 0;
    for (const QChar& c : address) {
        hash = hash * 31 + c.unicode();
    }
    return kAvatarColors[hash % kNumAvatarColors];
}

QString WalletsHandler::avatarText(const WalletSummaryRecord& account) const {
    if (account.walletKeyType() == WalletKeyType::PrivateKey) {
        return "i";
    }
    return QString::number(account.accountIndex + 1);
}
