#include "features/setup/SetupHandler.h"
#include "crypto/HDDerivation.h"
#include "crypto/Mnemonic.h"
#include "crypto/WalletCrypto.h"
#include "db/WalletDb.h"
#include <QRandomGenerator>
#include <algorithm>
#include <sodium.h>

SetupHandler::SetupHandler(QObject* parent) : QObject(parent) {}

QString SetupHandler::generateMnemonic(int wordCount) const {
    return Mnemonic::generate(wordCount);
}

SetupHandler::RecoveryChallenge SetupHandler::buildRecoveryChallenge(const QString& mnemonic,
                                                                     int challengeCount) const {
    RecoveryChallenge challenge;
    challenge.words = mnemonic.split(' ', Qt::SkipEmptyParts);

    QList<int> indices;
    for (int i = 0; i < challenge.words.size(); ++i) {
        indices.append(i);
    }

    for (int i = indices.size() - 1; i > 0; --i) {
        const int swapIndex = QRandomGenerator::global()->bounded(i + 1);
        indices.swapItemsAt(i, swapIndex);
    }

    challenge.indices = indices.mid(0, challengeCount);
    std::sort(challenge.indices.begin(), challenge.indices.end());
    return challenge;
}

bool SetupHandler::validateRecoveryChallenge(const RecoveryChallenge& challenge,
                                             const QStringList& enteredWords,
                                             QString* error) const {
    if (enteredWords.size() != challenge.indices.size()) {
        if (error) {
            *error = tr("Please complete all challenge words.");
        }
        return false;
    }

    for (int i = 0; i < challenge.indices.size(); ++i) {
        const int wordIndex = challenge.indices[i];
        if (wordIndex < 0 || wordIndex >= challenge.words.size()) {
            if (error) {
                *error = tr("The recovery phrase challenge is invalid.");
            }
            return false;
        }

        if (enteredWords[i].trimmed().toLower() != challenge.words[wordIndex]) {
            if (error) {
                *error =
                    tr("Word %1 is incorrect. Please check your recovery phrase and try again.")
                        .arg(wordIndex + 1);
            }
            return false;
        }
    }

    if (error) {
        error->clear();
    }
    return true;
}

bool SetupHandler::completeCreateWallet(const CreateWalletRequest& request,
                                        WalletCompletion* completion, QString* error) const {
    if (!completion) {
        if (error) {
            *error = tr("Wallet completion target is missing.");
        }
        return false;
    }

    if (request.mnemonic.isEmpty()) {
        if (error) {
            *error = tr("Recovery phrase is missing.");
        }
        return false;
    }

    QString passphrase = request.passphrase;
    QByteArray seed = Mnemonic::toSeed(request.mnemonic, passphrase);
    wipeQString(passphrase);
    QByteArray derivedKey = HDDerivation::derive(seed, "m/44'/501'/0'/0'");
    Keypair keypair = Keypair::fromSeed(derivedKey);
    const QString address = keypair.address();

    QByteArray secretKey = keypair.secretKey();
    const auto keyBlob = WalletCrypto::encrypt(secretKey, request.password);
    sodium_memzero(secretKey.data(), secretKey.size());

    if (!WalletDb::insertWallet("My Wallet", address, toStorageString(WalletKeyType::Mnemonic),
                                keyBlob.salt, keyBlob.nonce, keyBlob.ciphertext)) {
        if (error) {
            *error = tr("Failed to store wallet.");
        }
        sodium_memzero(seed.data(), seed.size());
        sodium_memzero(derivedKey.data(), derivedKey.size());
        return false;
    }

    const int walletId = WalletDb::getWalletId(address);
    if (walletId > 0) {
        const auto seedBlob = WalletCrypto::encrypt(seed, request.password);
        WalletDb::storeSeed(walletId, seedBlob.salt, seedBlob.nonce, seedBlob.ciphertext);

        const auto mnemonicBlob =
            WalletCrypto::encrypt(request.mnemonic.toUtf8(), request.password);
        WalletDb::storeMnemonic(walletId, mnemonicBlob.salt, mnemonicBlob.nonce,
                                mnemonicBlob.ciphertext);
    }

    sodium_memzero(seed.data(), seed.size());
    sodium_memzero(derivedKey.data(), derivedKey.size());

    completion->address = address;
    completion->keypair = keypair;
    completion->type = WalletKeyType::Mnemonic;
    if (error) {
        error->clear();
    }
    return true;
}

bool SetupHandler::completeImportRecovery(const ImportRecoveryRequest& request,
                                          WalletCompletion* completion, QString* error) const {
    if (!completion) {
        if (error) {
            *error = tr("Wallet completion target is missing.");
        }
        return false;
    }

    if (request.words.size() != 12 && request.words.size() != 24) {
        if (error) {
            *error = tr("Please enter a valid 12 or 24-word recovery phrase.");
        }
        return false;
    }

    QStringList normalizedWords;
    normalizedWords.reserve(request.words.size());
    for (const QString& word : request.words) {
        normalizedWords.append(word.trimmed().toLower());
    }

    QString mnemonic = normalizedWords.join(' ');
    if (!Mnemonic::validate(mnemonic)) {
        if (error) {
            *error = tr("The recovery phrase is not valid. Check your words and try again.");
        }
        wipeQString(mnemonic);
        return false;
    }

    QString passphrase = request.passphrase;
    QByteArray seed = Mnemonic::toSeed(mnemonic, passphrase);
    wipeQString(passphrase);

    const auto mnemonicBlob = WalletCrypto::encrypt(mnemonic.toUtf8(), request.password);
    wipeQString(mnemonic);

    QByteArray derivedKey = HDDerivation::derive(seed, "m/44'/501'/0'/0'");
    Keypair keypair = Keypair::fromSeed(derivedKey);
    const QString address = keypair.address();

    QByteArray secretKey = keypair.secretKey();
    const auto keyBlob = WalletCrypto::encrypt(secretKey, request.password);
    sodium_memzero(secretKey.data(), secretKey.size());

    if (!WalletDb::insertWallet("My Wallet", address, toStorageString(WalletKeyType::Mnemonic),
                                keyBlob.salt, keyBlob.nonce, keyBlob.ciphertext)) {
        if (error) {
            *error = tr("Failed to store wallet.");
        }
        sodium_memzero(seed.data(), seed.size());
        sodium_memzero(derivedKey.data(), derivedKey.size());
        return false;
    }

    const int walletId = WalletDb::getWalletId(address);
    if (walletId > 0) {
        const auto seedBlob = WalletCrypto::encrypt(seed, request.password);
        WalletDb::storeSeed(walletId, seedBlob.salt, seedBlob.nonce, seedBlob.ciphertext);
        WalletDb::storeMnemonic(walletId, mnemonicBlob.salt, mnemonicBlob.nonce,
                                mnemonicBlob.ciphertext);
    }

    sodium_memzero(seed.data(), seed.size());
    sodium_memzero(derivedKey.data(), derivedKey.size());

    completion->address = address;
    completion->keypair = keypair;
    completion->type = WalletKeyType::Mnemonic;
    if (error) {
        error->clear();
    }
    return true;
}

bool SetupHandler::completeImportPrivateKey(const ImportPrivateKeyRequest& request,
                                            WalletCompletion* completion, QString* error) const {
    if (!completion) {
        if (error) {
            *error = tr("Wallet completion target is missing.");
        }
        return false;
    }

    QString privateKey = request.privateKey.trimmed();
    if (privateKey.isEmpty()) {
        if (error) {
            *error = tr("Please enter a private key.");
        }
        return false;
    }

    Keypair keypair = Keypair::fromBase58(privateKey);
    wipeQString(privateKey);
    if (keypair.address().isEmpty()) {
        if (error) {
            *error = tr("The private key is not valid. Check the key and try again.");
        }
        return false;
    }

    const QString address = keypair.address();
    QByteArray secretKey = keypair.secretKey();
    const auto keyBlob = WalletCrypto::encrypt(secretKey, request.password);
    sodium_memzero(secretKey.data(), secretKey.size());

    if (!WalletDb::insertWallet("My Wallet", address, toStorageString(WalletKeyType::PrivateKey),
                                keyBlob.salt, keyBlob.nonce, keyBlob.ciphertext)) {
        if (error) {
            *error = tr("Failed to store wallet.");
        }
        return false;
    }

    completion->address = address;
    completion->keypair = keypair;
    completion->type = WalletKeyType::PrivateKey;
    if (error) {
        error->clear();
    }
    return true;
}

bool SetupHandler::completeHardwareWallet(const HardwareWalletRequest& request,
                                          WalletCompletion* completion, QString* error) const {
    if (!completion) {
        if (error) {
            *error = tr("Wallet completion target is missing.");
        }
        return false;
    }

    if (request.address.isEmpty() || request.publicKey.isEmpty()) {
        if (error) {
            *error = tr("Hardware wallet data is incomplete.");
        }
        return false;
    }

    const auto keyBlob = WalletCrypto::encrypt(request.publicKey, request.password);
    const QString typeId = toStorageString(request.type);
    const QString pluginId = toStorageString(pluginForWalletType(request.type));
    const QString label = QStringLiteral("%1-%2").arg(typeId.left(1).toUpper() + typeId.mid(1),
                                                      request.address.left(6));

    if (!WalletDb::insertHardwareWallet(label, request.address, typeId, keyBlob.salt, keyBlob.nonce,
                                        keyBlob.ciphertext, request.derivationPath, pluginId)) {
        if (error) {
            *error = tr("Failed to store hardware wallet.");
        }
        return false;
    }

    completion->address = request.address;
    completion->type = request.type;
    if (error) {
        error->clear();
    }
    return true;
}

void SetupHandler::wipeQString(QString& text) {
    if (!text.isEmpty()) {
        sodium_memzero(text.data(), static_cast<size_t>(text.size() * sizeof(QChar)));
        text.clear();
    }
}
