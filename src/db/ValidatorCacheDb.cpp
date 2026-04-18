#include "ValidatorCacheDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {

    ValidatorCacheRecord recordFromQuery(const QSqlQuery& q) {
        ValidatorCacheRecord r;
        r.voteAccount = q.value("vote_account").toString();
        r.name = q.value("name").toString();
        r.avatarUrl = q.value("avatar_url").toString();
        r.score = q.value("score").toInt();
        r.version = q.value("version").toString();
        r.city = q.value("city").toString();
        r.country = q.value("country").toString();
        r.updatedAt = q.value("updated_at").toLongLong();
        return r;
    }

} // namespace

QSqlDatabase ValidatorCacheDb::db() { return Database::connection(); }

bool ValidatorCacheDb::upsert(const QString& voteAccount, const QString& name,
                              const QString& avatarUrl, int score, const QString& version,
                              const QString& city, const QString& country) {
    static const QString kSql = R"(
        INSERT INTO validator_cache
            (vote_account, name, avatar_url, score, version, city, country, updated_at)
        VALUES
            (:va, :name, :url, :score, :ver, :city, :country, strftime('%s', 'now'))
        ON CONFLICT(vote_account) DO UPDATE SET
            name = :name2,
            avatar_url = :url2,
            score = :score2,
            version = :ver2,
            city = :city2,
            country = :country2,
            updated_at = strftime('%s', 'now')
    )";

    return DbUtil::exec(db(), kSql,
                        {{":va", voteAccount},
                         {":name", name},
                         {":url", avatarUrl},
                         {":score", score},
                         {":ver", version},
                         {":city", city},
                         {":country", country},
                         {":name2", name},
                         {":url2", avatarUrl},
                         {":score2", score},
                         {":ver2", version},
                         {":city2", city},
                         {":country2", country}});
}

std::optional<ValidatorCacheRecord>
ValidatorCacheDb::getByVoteAccountRecord(const QString& voteAccount) {
    return DbUtil::one<ValidatorCacheRecord>(
        db(), "SELECT * FROM validator_cache WHERE vote_account = :va LIMIT 1",
        {{":va", voteAccount}}, recordFromQuery);
}

QList<ValidatorCacheRecord> ValidatorCacheDb::getAllRecords() {
    return DbUtil::many<ValidatorCacheRecord>(
        db(), "SELECT * FROM validator_cache ORDER BY score DESC", {}, recordFromQuery);
}

QString ValidatorCacheDb::getName(const QString& voteAccount) {
    return DbUtil::scalarString(db(),
                                "SELECT name FROM validator_cache WHERE vote_account = :va LIMIT 1",
                                {{":va", voteAccount}})
        .value_or(QString{});
}

bool ValidatorCacheDb::clearAll() { return DbUtil::exec(db(), "DELETE FROM validator_cache"); }
