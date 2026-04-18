#ifndef DBUTIL_H
#define DBUTIL_H

#include <QList>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <optional>
#include <source_location>

namespace DbUtil {

    using SqlParam = QPair<QString, QVariant>;
    using SqlParams = QList<SqlParam>;

    enum class RequireRows {
        No,
        Yes,
    };

    bool prepareBindExec(QSqlQuery& q, const QString& sql, const SqlParams& params,
                         std::source_location loc = std::source_location::current());

    bool exec(const QSqlDatabase& db, const QString& sql, const SqlParams& params = {},
              RequireRows requireRows = RequireRows::No,
              std::source_location loc = std::source_location::current());

    template <typename T, typename Mapper>
    std::optional<T> one(QSqlDatabase db, const QString& sql, const SqlParams& params,
                         Mapper mapper,
                         std::source_location loc = std::source_location::current()) {
        QSqlQuery q(db);
        if (!prepareBindExec(q, sql, params, loc)) {
            return std::nullopt;
        }
        if (!q.next()) {
            return std::nullopt;
        }
        return mapper(q);
    }

    template <typename T, typename Mapper>
    QList<T> many(QSqlDatabase db, const QString& sql, const SqlParams& params, Mapper mapper,
                  std::source_location loc = std::source_location::current()) {
        QSqlQuery q(db);
        if (!prepareBindExec(q, sql, params, loc)) {
            return {};
        }
        QList<T> out;
        while (q.next()) {
            out.append(mapper(q));
        }
        return out;
    }

    std::optional<int> scalarInt(const QSqlDatabase& db, const QString& sql,
                                 const SqlParams& params = {},
                                 std::source_location loc = std::source_location::current());

    std::optional<QString> scalarString(const QSqlDatabase& db, const QString& sql,
                                        const SqlParams& params = {},
                                        std::source_location loc = std::source_location::current());

    bool exists(const QSqlDatabase& db, const QString& sql, const SqlParams& params = {},
                std::source_location loc = std::source_location::current());

} // namespace DbUtil

#endif // DBUTIL_H
