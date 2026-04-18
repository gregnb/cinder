#ifndef FEATURES_SETUP_SETUPHANDLER_H
#define FEATURES_SETUP_SETUPHANDLER_H

#include "crypto/Keypair.h"
#include "models/WalletTypes.h"
#include <QByteArray>
#include <QObject>
#include <QString>

class SetupHandler : public QObject {
    Q_OBJECT

  public:
    struct RecoveryChallenge {
        QStringList words;
        QList<int> indices;
    };

    struct CreateWalletRequest {
        QString mnemonic;
        QString passphrase;
        QString password;
    };

    struct ImportRecoveryRequest {
        QStringList words;
        QString passphrase;
        QString password;
    };

    struct ImportPrivateKeyRequest {
        QString privateKey;
        QString password;
    };

    struct HardwareWalletRequest {
        QByteArray publicKey;
        QString address;
        QString password;
        QString derivationPath;
        WalletKeyType type = WalletKeyType::Unknown;
    };

    struct WalletCompletion {
        QString address;
        Keypair keypair;
        WalletKeyType type = WalletKeyType::Mnemonic;
    };

    explicit SetupHandler(QObject* parent = nullptr);

    QString generateMnemonic(int wordCount) const;
    RecoveryChallenge buildRecoveryChallenge(const QString& mnemonic, int challengeCount) const;
    bool validateRecoveryChallenge(const RecoveryChallenge& challenge,
                                   const QStringList& enteredWords, QString* error) const;

    bool completeCreateWallet(const CreateWalletRequest& request, WalletCompletion* completion,
                              QString* error) const;
    bool completeImportRecovery(const ImportRecoveryRequest& request, WalletCompletion* completion,
                                QString* error) const;
    bool completeImportPrivateKey(const ImportPrivateKeyRequest& request,
                                  WalletCompletion* completion, QString* error) const;
    bool completeHardwareWallet(const HardwareWalletRequest& request, WalletCompletion* completion,
                                QString* error) const;

  private:
    static void wipeQString(QString& text);
};

#endif // FEATURES_SETUP_SETUPHANDLER_H
