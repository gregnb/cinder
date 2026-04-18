#include "DbUtil.h"
#include <QDebug>

namespace DbUtil {

    bool prepareBindExec(QSqlQuery& q, const QString& sql, const SqlParams& params,
                         std::source_location loc) {
        if (!q.prepare(sql)) {
            qWarning() << "[DB]" << loc.function_name()
                       << "prepare failed:" << q.lastError().text();
            return false;
        }

        for (const auto& [key, value] : params) {
            q.bindValue(key, value);
        }

        if (!q.exec()) {
            qWarning() << "[DB]" << loc.function_name() << "exec failed:" << q.lastError().text();
            return false;
        }
        return true;
    }

    bool exec(const QSqlDatabase& db, const QString& sql, const SqlParams& params,
              RequireRows requireRows, std::source_location loc) {
        QSqlQuery q(db);
        if (!prepareBindExec(q, sql, params, loc)) {
            return false;
        }
        if (requireRows == RequireRows::Yes && q.numRowsAffected() <= 0) {
            return false;
        }
        return true;
    }

    std::optional<int> scalarInt(const QSqlDatabase& db, const QString& sql,
                                 const SqlParams& params, std::source_location loc) {
        QSqlQuery q(db);
        if (!prepareBindExec(q, sql, params, loc)) {
            return std::nullopt;
        }
        if (!q.next()) {
            return std::nullopt;
        }
        return q.value(0).toInt();
    }

    std::optional<QString> scalarString(const QSqlDatabase& db, const QString& sql,
                                        const SqlParams& params, std::source_location loc) {
        QSqlQuery q(db);
        if (!prepareBindExec(q, sql, params, loc)) {
            return std::nullopt;
        }
        if (!q.next()) {
            return std::nullopt;
        }
        return q.value(0).toString();
    }

    bool exists(const QSqlDatabase& db, const QString& sql, const SqlParams& params,
                std::source_location loc) {
        QSqlQuery q(db);
        if (!prepareBindExec(q, sql, params, loc)) {
            return false;
        }
        return q.next();
    }

} // namespace DbUtil
