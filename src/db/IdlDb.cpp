#include "IdlDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QSqlDatabase>
#include <QSqlQuery>

QSqlDatabase IdlDb::db() { return Database::connection(); }

bool IdlDb::upsertIdl(const QString& programId, const QString& programName,
                      const QString& idlJson) {
    static const QString kSql = R"(
        INSERT INTO anchor_idl_cache (program_id, program_name, idl_json)
        VALUES (:pid, :pname, :json)
        ON CONFLICT(program_id) DO UPDATE SET
            program_name = :pname2,
            idl_json = :json2,
            fetched_at = strftime('%s', 'now')
    )";

    return DbUtil::exec(db(), kSql,
                        {{":pid", programId},
                         {":pname", programName},
                         {":json", idlJson},
                         {":pname2", programName},
                         {":json2", idlJson}});
}

QString IdlDb::getIdlJson(const QString& programId) {
    static const QString kSql = R"(
        SELECT idl_json
        FROM anchor_idl_cache
        WHERE program_id = :pid
        LIMIT 1
    )";

    return DbUtil::scalarString(db(), kSql, {{":pid", programId}}).value_or(QString{});
}

bool IdlDb::markNoIdl(const QString& programId) { return upsertIdl(programId, "", ""); }

bool IdlDb::isMarkedNoIdl(const QString& programId) {
    auto idl = DbUtil::scalarString(
        db(), "SELECT idl_json FROM anchor_idl_cache WHERE program_id = :pid LIMIT 1",
        {{":pid", programId}});
    if (!idl.has_value()) {
        return false;
    }
    return idl->isEmpty();
}

int IdlDb::countAll() {
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM anchor_idl_cache").value_or(0);
}

QList<QPair<QString, QString>> IdlDb::getAll() {
    return DbUtil::many<QPair<QString, QString>>(
        db(), "SELECT program_id, idl_json FROM anchor_idl_cache", {},
        [](const QSqlQuery& q) { return qMakePair(q.value(0).toString(), q.value(1).toString()); });
}
