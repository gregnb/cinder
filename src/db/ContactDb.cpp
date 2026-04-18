#include "ContactDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QDebug>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

    ContactRecord recordFromQuery(const QSqlQuery& q) {
        ContactRecord r;
        r.id = q.value("id").toInt();
        r.name = q.value("name").toString();
        r.address = q.value("address").toString();
        r.avatarPath = q.value("avatar_path").toString();
        r.createdAt = q.value("created_at").toLongLong();
        return r;
    }

} // namespace

QSqlDatabase ContactDb::db() { return Database::connection(); }

bool ContactDb::insertContact(const QString& name, const QString& address) {
    return DbUtil::exec(db(), "INSERT INTO contacts (name, address) VALUES (:name, :addr)",
                        {{":name", name}, {":addr", address}});
}

bool ContactDb::updateContact(int id, const QString& newName, const QString& newAddress) {
    return DbUtil::exec(db(), "UPDATE contacts SET name = :name, address = :addr WHERE id = :id",
                        {{":name", newName}, {":addr", newAddress}, {":id", id}},
                        DbUtil::RequireRows::Yes);
}

bool ContactDb::deleteContact(int id) {
    return DbUtil::exec(db(), "DELETE FROM contacts WHERE id = :id", {{":id", id}},
                        DbUtil::RequireRows::Yes);
}

QList<ContactRecord> ContactDb::getAllRecords(const QString& search) {
    static const QString kSqlNoFilter = R"(
        SELECT * FROM contacts ORDER BY name COLLATE NOCASE ASC
    )";
    static const QString kSqlWithFilter = R"(
        SELECT * FROM contacts
        WHERE name LIKE :search OR address LIKE :search
        ORDER BY name COLLATE NOCASE ASC
    )";
    if (search.isEmpty()) {
        return DbUtil::many<ContactRecord>(db(), kSqlNoFilter, {}, recordFromQuery);
    }
    return DbUtil::many<ContactRecord>(db(), kSqlWithFilter, {{":search", "%" + search + "%"}},
                                       recordFromQuery);
}

std::optional<ContactRecord> ContactDb::getByIdRecord(int id) {
    return DbUtil::one<ContactRecord>(db(), "SELECT * FROM contacts WHERE id = :id LIMIT 1",
                                      {{":id", id}}, recordFromQuery);
}

int ContactDb::countAll() {
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM contacts").value_or(0);
}

bool ContactDb::hasAddress(const QString& address) {
    return DbUtil::exists(db(), "SELECT 1 FROM contacts WHERE address = :addr LIMIT 1",
                          {{":addr", address}});
}

QString ContactDb::getNameByAddress(const QString& address) {
    return DbUtil::scalarString(db(), "SELECT name FROM contacts WHERE address = :addr LIMIT 1",
                                {{":addr", address}})
        .value_or(QString{});
}

std::optional<ContactRecord> ContactDb::getByAddressRecord(const QString& address) {
    return DbUtil::one<ContactRecord>(db(), "SELECT * FROM contacts WHERE address = :addr LIMIT 1",
                                      {{":addr", address}}, recordFromQuery);
}

// ── Avatar ────────────────────────────────────────────────────────

bool ContactDb::setAvatarPath(int contactId, const QString& path) {
    return DbUtil::exec(db(), "UPDATE contacts SET avatar_path = :p WHERE id = :id",
                        {{":p", path.isEmpty() ? QVariant() : path}, {":id", contactId}});
}

QString ContactDb::getAvatarPath(int contactId) {
    return DbUtil::scalarString(db(), "SELECT avatar_path FROM contacts WHERE id = :id LIMIT 1",
                                {{":id", contactId}})
        .value_or(QString{});
}

QString ContactDb::avatarDir() {
    QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/contact-avatars";
    QDir().mkpath(dir);
    return dir;
}

QString ContactDb::avatarFullPath(const QString& relativePath) {
    if (relativePath.isEmpty()) {
        return {};
    }
    return avatarDir() + "/" + relativePath;
}
