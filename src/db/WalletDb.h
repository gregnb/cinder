#ifndef WALLETDB_H
#define WALLETDB_H

#include "models/WalletTypes.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <optional>

class QSqlDatabase;

struct WalletSummaryRecord {
    int id = 0;
    QString label;
    QString address;
    QString keyType;
    int accountIndex = -1;
    int parentWalletId = -1;
    qint64 createdAt = 0;
    QString avatarPath;

    WalletKeyType walletKeyType() const { return parseWalletKeyType(keyType); }
    bool isImportedPrivateKey() const { return walletKeyType() == WalletKeyType::PrivateKey; }
};

struct WalletRecord {
    int id = 0;
    QString label;
    QString address;
    QString keyType;
    QByteArray salt;
    QByteArray nonce;
    QByteArray ciphertext;
    qint64 createdAt = 0;
    bool biometricEnabled = false;
    QString derivationPath;
    QString hwPlugin;
    int accountIndex = -1;
    int parentWalletId = -1;
    QString avatarPath;

    WalletKeyType walletKeyType() const { return parseWalletKeyType(keyType); }
    HardwarePluginId hardwarePluginId() const {
        if (!hwPlugin.isEmpty()) {
            return parseHardwarePluginId(hwPlugin);
        }
        return pluginForWalletType(walletKeyType());
    }
    bool isHardwareWallet() const { return hardwarePluginId() != HardwarePluginId::None; }
};

struct WalletEncryptedBlobRecord {
    QByteArray salt;
    QByteArray nonce;
    QByteArray ciphertext;
};

class WalletDb {
  public:
    // Insert a new encrypted wallet. Returns false if address already exists.
    static bool insertWallet(const QString& label, const QString& address, const QString& keyType,
                             const QByteArray& salt, const QByteArray& nonce,
                             const QByteArray& ciphertext);

    // Insert a derived wallet with account index and parent reference.
    static bool insertDerivedWallet(const QString& label, const QString& address,
                                    const QString& keyType, const QByteArray& salt,
                                    const QByteArray& nonce, const QByteArray& ciphertext,
                                    int accountIndex, int parentWalletId);

    // Insert a hardware wallet with derivation path and plugin metadata.
    static bool insertHardwareWallet(const QString& label, const QString& address,
                                     const QString& keyType, const QByteArray& salt,
                                     const QByteArray& nonce, const QByteArray& ciphertext,
                                     const QString& derivationPath, const QString& hwPlugin);

    // Get all wallets (id, label, address, key_type, account_index, parent_wallet_id, created_at).
    static QList<WalletSummaryRecord> getAllRecords();

    // Get full wallet record including encrypted key material.
    static std::optional<WalletRecord> getByAddressRecord(const QString& address);

    // Check if any wallet exists.
    static bool hasAnyWallet();

    // Count total wallets.
    static int countAll();

    // Delete a wallet by id.
    static bool deleteWallet(int id);

    // Update a wallet's label.
    static bool updateLabel(int id, const QString& label);

    // Biometric (Touch ID) preference per wallet.
    static bool isBiometricEnabled(const QString& address);
    static bool setBiometricEnabled(const QString& address, bool enabled);

    // ── Seed storage (for multi-account derivation) ──────────
    static bool storeSeed(int walletId, const QByteArray& salt, const QByteArray& nonce,
                          const QByteArray& ciphertext);
    static std::optional<WalletEncryptedBlobRecord> getSeedBlobRecord(int walletId);

    // ── Mnemonic storage (for recovery phrase reveal) ─────────
    static bool storeMnemonic(int walletId, const QByteArray& salt, const QByteArray& nonce,
                              const QByteArray& ciphertext);
    static std::optional<WalletEncryptedBlobRecord> getMnemonicBlobRecord(int walletId);

    // ── Avatar ─────────────────────────────────────────────────
    static bool setAvatarPath(int walletId, const QString& path);
    static QString getAvatarPath(int walletId);
    static QString avatarDir();
    static QString avatarFullPath(const QString& relativePath);

    // ── Multi-account helpers ────────────────────────────────
    static QList<WalletSummaryRecord> getSiblingsRecords(int parentWalletId);
    static int nextAccountIndex(int parentWalletId);
    static int getWalletId(const QString& address);
    static int getParentWalletId(const QString& address);

  private:
    static QSqlDatabase db();
};

#endif // WALLETDB_H
