#include "NotificationDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

    NotificationRecord recordFromQuery(const QSqlQuery& q) {
        NotificationRecord r;
        r.id = q.value("id").toInt();
        r.type = q.value("type").toString();
        r.title = q.value("title").toString();
        r.body = q.value("body").toString();
        r.signature = q.value("signature").toString();
        r.token = q.value("token").toString();
        r.amount = q.value("amount").toString();
        r.fromAddr = q.value("from_addr").toString();
        r.isRead = q.value("is_read").toInt() != 0;
        r.createdAt = q.value("created_at").toLongLong();
        return r;
    }

} // namespace

QSqlDatabase NotificationDb::db() { return Database::connection(); }

bool NotificationDb::insertNotification(const QString& type, const QString& title,
                                        const QString& body, const QString& signature,
                                        const QString& token, const QString& amount,
                                        const QString& fromAddr) {
    return DbUtil::exec(
        db(),
        "INSERT INTO notifications (type, title, body, signature, token, amount, from_addr, "
        "created_at) VALUES (:type, :title, :body, :sig, :token, :amount, :from, :ts)",
        {{":type", type},
         {":title", title},
         {":body", body},
         {":sig", signature.isEmpty() ? QVariant() : signature},
         {":token", token.isEmpty() ? QVariant() : token},
         {":amount", amount.isEmpty() ? QVariant() : amount},
         {":from", fromAddr.isEmpty() ? QVariant() : fromAddr},
         {":ts", QDateTime::currentSecsSinceEpoch()}});
}

QList<NotificationRecord> NotificationDb::getRecentRecords(int limit) {
    return DbUtil::many<NotificationRecord>(
        db(),
        "SELECT id, type, title, body, signature, token, amount, from_addr, is_read, created_at "
        "FROM notifications ORDER BY created_at DESC LIMIT :lim",
        {{":lim", limit}}, recordFromQuery);
}

int NotificationDb::countUnread() {
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM notifications WHERE is_read = 0")
        .value_or(0);
}

bool NotificationDb::markRead(int id) {
    return DbUtil::exec(db(), "UPDATE notifications SET is_read = 1 WHERE id = :id", {{":id", id}},
                        DbUtil::RequireRows::Yes);
}

bool NotificationDb::markAllRead() {
    return DbUtil::exec(db(), "UPDATE notifications SET is_read = 1 WHERE is_read = 0");
}

bool NotificationDb::existsForSignature(const QString& signature) {
    return DbUtil::exists(db(), "SELECT 1 FROM notifications WHERE signature = :sig LIMIT 1",
                          {{":sig", signature}});
}
