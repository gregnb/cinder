#include "NonceDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

    NonceAccountRecord recordFromQuery(const QSqlQuery& q) {
        NonceAccountRecord r;
        r.id = q.value("id").toInt();
        r.address = q.value("address").toString();
        r.authority = q.value("authority").toString();
        r.nonceValue = q.value("nonce_value").toString();
        r.createdAt = q.value("created_at").toLongLong();
        return r;
    }

} // namespace

QSqlDatabase NonceDb::db() { return Database::connection(); }

bool NonceDb::insertNonceAccount(const QString& address, const QString& authority,
                                 const QString& nonceValue) {
    return DbUtil::exec(db(),
                        "INSERT INTO nonce_accounts (address, authority, nonce_value) "
                        "VALUES (:addr, :auth, :nonce)",
                        {{":addr", address}, {":auth", authority}, {":nonce", nonceValue}});
}

std::optional<NonceAccountRecord> NonceDb::getByAuthorityRecord(const QString& authority) {
    return DbUtil::one<NonceAccountRecord>(
        db(),
        "SELECT id, address, authority, nonce_value, created_at FROM nonce_accounts "
        "WHERE authority = :auth LIMIT 1",
        {{":auth", authority}}, recordFromQuery);
}

bool NonceDb::updateNonceValue(const QString& address, const QString& newNonceValue) {
    return DbUtil::exec(db(),
                        "UPDATE nonce_accounts SET nonce_value = :nonce WHERE address = :addr",
                        {{":nonce", newNonceValue}, {":addr", address}}, DbUtil::RequireRows::Yes);
}

bool NonceDb::deleteNonceAccount(const QString& address) {
    return DbUtil::exec(db(), "DELETE FROM nonce_accounts WHERE address = :addr",
                        {{":addr", address}}, DbUtil::RequireRows::Yes);
}

bool NonceDb::hasNonceAccount(const QString& authority) {
    return DbUtil::exists(db(), "SELECT 1 FROM nonce_accounts WHERE authority = :auth LIMIT 1",
                          {{":auth", authority}});
}
