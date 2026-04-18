#ifndef WALLETSHANDLER_H
#define WALLETSHANDLER_H

#include "db/WalletDb.h"
#include <QColor>
#include <QString>
#include <QStringList>
#include <optional>

class WalletsHandler {
  public:
    QList<WalletSummaryRecord> listWallets() const;
    WalletRecord loadWalletDetail(const WalletSummaryRecord& wallet) const;

    bool renameWallet(int walletId, const QString& label) const;
    bool removeWallet(int walletId) const;
    bool saveAvatar(int walletId, const QString& address, const QString& sourcePath,
                    QString& relativePathOut) const;

    QString typeBadge(const WalletSummaryRecord& account) const;
    QString derivationPath(const WalletRecord& account) const;
    bool showDerivationPath(const WalletRecord& account) const;
    bool isSoftwareWallet(const WalletRecord& account) const;
    bool hasRecoveryPhrase(int walletId) const;

    std::optional<QString> revealPrivateKey(const QString& address, const QString& password,
                                            QString& errorOut) const;
    std::optional<QStringList> revealRecoveryPhrase(int walletId, const QString& password,
                                                    QString& errorOut) const;

    QColor avatarColor(const QString& address) const;
    QString avatarText(const WalletSummaryRecord& account) const;
};

#endif // WALLETSHANDLER_H
