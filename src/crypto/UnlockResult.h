#ifndef UNLOCKRESULT_H
#define UNLOCKRESULT_H

#include "crypto/Keypair.h"
#include "models/WalletTypes.h"

#include <QByteArray>
#include <QString>

struct UnlockResult {
    QString address;
    QByteArray publicKey; // 32 bytes — always available
    Keypair keypair;      // populated only for software wallets
    WalletKeyType walletType = WalletKeyType::Unknown;
    HardwarePluginId hardwarePlugin = HardwarePluginId::None;
    QString derivationPath; // "m/44'/501'/0'/0'" or empty
    QString password;       // session password for account switching (wiped on lock)

    bool isHardwareWallet() const { return hardwarePlugin != HardwarePluginId::None; }
};

#endif // UNLOCKRESULT_H
