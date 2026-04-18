#include "WalletDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

    WalletSummaryRecord summaryFromQuery(const QSqlQuery& q) {
        WalletSummaryRecord r;
        r.id = q.value("id").toInt();
        r.label = q.value("label").toString();
        r.address = q.value("address").toString();
        r.keyType = q.value("key_type").toString();
        r.accountIndex = q.value("account_index").isNull() ? -1 : q.value("account_index").toInt();
        r.parentWalletId =
            q.value("parent_wallet_id").isNull() ? -1 : q.value("parent_wallet_id").toInt();
        r.createdAt = q.value("created_at").toLongLong();
        r.avatarPath = q.value("avatar_path").toString();
        return r;
    }

    WalletRecord walletFromQuery(const QSqlQuery& q) {
        WalletRecord r;
        r.id = q.value("id").toInt();
        r.label = q.value("label").toString();
        r.address = q.value("address").toString();
        r.keyType = q.value("key_type").toString();
        r.salt = q.value("salt").toByteArray();
        r.nonce = q.value("nonce").toByteArray();
        r.ciphertext = q.value("ciphertext").toByteArray();
        r.createdAt = q.value("created_at").toLongLong();
        r.biometricEnabled = q.value("biometric_enabled").toInt() == 1;
        r.derivationPath = q.value("derivation_path").toString();
        r.hwPlugin = q.value("hw_plugin").toString();
        r.accountIndex = q.value("account_index").isNull() ? -1 : q.value("account_index").toInt();
        r.parentWalletId =
            q.value("parent_wallet_id").isNull() ? -1 : q.value("parent_wallet_id").toInt();
        r.avatarPath = q.value("avatar_path").toString();
        return r;
    }

} // namespace

QSqlDatabase WalletDb::db() { return Database::connection(); }

bool WalletDb::insertWallet(const QString& label, const QString& address, const QString& keyType,
                            const QByteArray& salt, const QByteArray& nonce,
                            const QByteArray& ciphertext) {
    static const QString kSql = R"(
        INSERT INTO wallets (label, address, key_type, salt, nonce, ciphertext)
        VALUES (:label, :addr, :type, :salt, :nonce, :ct)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":label", label},
                         {":addr", address},
                         {":type", keyType},
                         {":salt", salt},
                         {":nonce", nonce},
                         {":ct", ciphertext}});
}

bool WalletDb::insertDerivedWallet(const QString& label, const QString& address,
                                   const QString& keyType, const QByteArray& salt,
                                   const QByteArray& nonce, const QByteArray& ciphertext,
                                   int accountIndex, int parentWalletId) {
    static const QString kSql = R"(
        INSERT INTO wallets (
            label,
            address,
            key_type,
            salt,
            nonce,
            ciphertext,
            account_index,
            parent_wallet_id
        )
        VALUES (:label, :addr, :type, :salt, :nonce, :ct, :idx, :parent)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":label", label},
                         {":addr", address},
                         {":type", keyType},
                         {":salt", salt},
                         {":nonce", nonce},
                         {":ct", ciphertext},
                         {":idx", accountIndex},
                         {":parent", parentWalletId}});
}

bool WalletDb::insertHardwareWallet(const QString& label, const QString& address,
                                    const QString& keyType, const QByteArray& salt,
                                    const QByteArray& nonce, const QByteArray& ciphertext,
                                    const QString& derivationPath, const QString& hwPlugin) {
    static const QString kSql = R"(
        INSERT INTO wallets (
            label,
            address,
            key_type,
            salt,
            nonce,
            ciphertext,
            derivation_path,
            hw_plugin
        )
        VALUES (:label, :addr, :type, :salt, :nonce, :ct, :deriv, :plugin)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":label", label},
                         {":addr", address},
                         {":type", keyType},
                         {":salt", salt},
                         {":nonce", nonce},
                         {":ct", ciphertext},
                         {":deriv", derivationPath},
                         {":plugin", hwPlugin}});
}

QList<WalletSummaryRecord> WalletDb::getAllRecords() {
    static const QString kSql = R"(
        SELECT
            id,
            label,
            address,
            key_type,
            account_index,
            parent_wallet_id,
            created_at,
            avatar_path
        FROM wallets
        ORDER BY created_at ASC
    )";

    return DbUtil::many<WalletSummaryRecord>(db(), kSql, {}, summaryFromQuery);
}

std::optional<WalletRecord> WalletDb::getByAddressRecord(const QString& address) {
    return DbUtil::one<WalletRecord>(db(), "SELECT * FROM wallets WHERE address = :addr LIMIT 1",
                                     {{":addr", address}}, walletFromQuery);
}

bool WalletDb::hasAnyWallet() { return DbUtil::exists(db(), "SELECT 1 FROM wallets LIMIT 1"); }

int WalletDb::countAll() {
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM wallets").value_or(0);
}

bool WalletDb::deleteWallet(int id) {
    return DbUtil::exec(db(), "DELETE FROM wallets WHERE id = :id", {{":id", id}},
                        DbUtil::RequireRows::Yes);
}

bool WalletDb::updateLabel(int id, const QString& label) {
    return DbUtil::exec(db(), "UPDATE wallets SET label = :label WHERE id = :id",
                        {{":label", label}, {":id", id}}, DbUtil::RequireRows::Yes);
}

bool WalletDb::isBiometricEnabled(const QString& address) {
    return DbUtil::scalarInt(db(),
                             "SELECT biometric_enabled FROM wallets WHERE address = :addr LIMIT 1",
                             {{":addr", address}})
               .value_or(0) == 1;
}

bool WalletDb::setBiometricEnabled(const QString& address, bool enabled) {
    return DbUtil::exec(db(), "UPDATE wallets SET biometric_enabled = :val WHERE address = :addr",
                        {{":val", enabled ? 1 : 0}, {":addr", address}}, DbUtil::RequireRows::Yes);
}

// ── Seed storage ────────────────────────────────────────────────

bool WalletDb::storeSeed(int walletId, const QByteArray& salt, const QByteArray& nonce,
                         const QByteArray& ciphertext) {
    static const QString kSql = R"(
        UPDATE wallets
        SET
            seed_salt = :salt,
            seed_nonce = :nonce,
            seed_ciphertext = :ct
        WHERE id = :id
    )";

    return DbUtil::exec(
        db(), kSql, {{":salt", salt}, {":nonce", nonce}, {":ct", ciphertext}, {":id", walletId}},
        DbUtil::RequireRows::Yes);
}

std::optional<WalletEncryptedBlobRecord> WalletDb::getSeedBlobRecord(int walletId) {
    QSqlQuery q(db());
    if (!DbUtil::prepareBindExec(
            q, "SELECT seed_salt, seed_nonce, seed_ciphertext FROM wallets WHERE id = :id LIMIT 1",
            {{":id", walletId}})) {
        return std::nullopt;
    }

    if (q.next() && !q.value("seed_ciphertext").isNull()) {
        return WalletEncryptedBlobRecord{q.value("seed_salt").toByteArray(),
                                         q.value("seed_nonce").toByteArray(),
                                         q.value("seed_ciphertext").toByteArray()};
    }
    return std::nullopt;
}

// ── Mnemonic storage ───────────────────────────────────────────

bool WalletDb::storeMnemonic(int walletId, const QByteArray& salt, const QByteArray& nonce,
                             const QByteArray& ciphertext) {
    static const QString kSql = R"(
        UPDATE wallets
        SET
            mnemonic_salt = :salt,
            mnemonic_nonce = :nonce,
            mnemonic_ciphertext = :ct
        WHERE id = :id
    )";

    return DbUtil::exec(
        db(), kSql, {{":salt", salt}, {":nonce", nonce}, {":ct", ciphertext}, {":id", walletId}},
        DbUtil::RequireRows::Yes);
}

std::optional<WalletEncryptedBlobRecord> WalletDb::getMnemonicBlobRecord(int walletId) {
    QSqlQuery q(db());
    if (!DbUtil::prepareBindExec(q,
                                 "SELECT w.mnemonic_salt, w.mnemonic_nonce, w.mnemonic_ciphertext "
                                 "FROM wallets w "
                                 "LEFT JOIN wallets p ON w.parent_wallet_id = p.id "
                                 "WHERE w.id = :id LIMIT 1",
                                 {{":id", walletId}})) {
        return std::nullopt;
    }

    if (q.next() && !q.value("mnemonic_ciphertext").isNull()) {
        return WalletEncryptedBlobRecord{q.value("mnemonic_salt").toByteArray(),
                                         q.value("mnemonic_nonce").toByteArray(),
                                         q.value("mnemonic_ciphertext").toByteArray()};
    }

    QSqlQuery parentLookup(db());
    if (!DbUtil::prepareBindExec(
            parentLookup,
            "SELECT parent_wallet_id FROM wallets WHERE id = :id AND parent_wallet_id IS NOT NULL",
            {{":id", walletId}})) {
        return std::nullopt;
    }

    if (!parentLookup.next()) {
        return std::nullopt;
    }

    const int parentId = parentLookup.value(0).toInt();
    QSqlQuery parentQ(db());
    if (!DbUtil::prepareBindExec(parentQ,
                                 "SELECT mnemonic_salt, mnemonic_nonce, mnemonic_ciphertext "
                                 "FROM wallets WHERE id = :id LIMIT 1",
                                 {{":id", parentId}})) {
        return std::nullopt;
    }

    if (parentQ.next() && !parentQ.value("mnemonic_ciphertext").isNull()) {
        return WalletEncryptedBlobRecord{parentQ.value("mnemonic_salt").toByteArray(),
                                         parentQ.value("mnemonic_nonce").toByteArray(),
                                         parentQ.value("mnemonic_ciphertext").toByteArray()};
    }

    return std::nullopt;
}

// ── Multi-account helpers ───────────────────────────────────────

QList<WalletSummaryRecord> WalletDb::getSiblingsRecords(int parentWalletId) {
    static const QString kSql = R"(
        SELECT
            id,
            label,
            address,
            key_type,
            account_index,
            created_at,
            avatar_path
        FROM wallets
        WHERE parent_wallet_id = :parent OR id = :parent
        ORDER BY account_index ASC
    )";

    return DbUtil::many<WalletSummaryRecord>(db(), kSql, {{":parent", parentWalletId}},
                                             summaryFromQuery);
}

int WalletDb::nextAccountIndex(int parentWalletId) {
    auto maxIndex = DbUtil::scalarInt(
        db(),
        "SELECT MAX(account_index) FROM wallets WHERE parent_wallet_id = :parent OR id = :parent",
        {{":parent", parentWalletId}});

    if (maxIndex.has_value()) {
        return maxIndex.value() + 1;
    }
    return 1;
}

int WalletDb::getWalletId(const QString& address) {
    return DbUtil::scalarInt(db(), "SELECT id FROM wallets WHERE address = :addr LIMIT 1",
                             {{":addr", address}})
        .value_or(-1);
}

int WalletDb::getParentWalletId(const QString& address) {
    QSqlQuery q(db());
    if (!DbUtil::prepareBindExec(
            q, "SELECT parent_wallet_id, id FROM wallets WHERE address = :addr LIMIT 1",
            {{":addr", address}})) {
        return -1;
    }

    if (!q.next()) {
        return -1;
    }

    const QVariant parentId = q.value("parent_wallet_id");
    if (!parentId.isNull() && parentId.toInt() > 0) {
        return parentId.toInt();
    }
    return q.value("id").toInt();
}

// ── Avatar ────────────────────────────────────────────────────────

bool WalletDb::setAvatarPath(int walletId, const QString& path) {
    return DbUtil::exec(db(), "UPDATE wallets SET avatar_path = :p WHERE id = :id",
                        {{":p", path.isEmpty() ? QVariant() : path}, {":id", walletId}});
}

QString WalletDb::getAvatarPath(int walletId) {
    return DbUtil::scalarString(db(), "SELECT avatar_path FROM wallets WHERE id = :id LIMIT 1",
                                {{":id", walletId}})
        .value_or(QString{});
}

QString WalletDb::avatarDir() {
    QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wallet-avatars";
    QDir().mkpath(dir);
    return dir;
}

QString WalletDb::avatarFullPath(const QString& relativePath) {
    if (relativePath.isEmpty()) {
        return {};
    }
    return avatarDir() + "/" + relativePath;
}
