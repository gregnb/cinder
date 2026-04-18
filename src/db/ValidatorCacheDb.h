#ifndef VALIDATORCACHEDB_H
#define VALIDATORCACHEDB_H

#include <QList>
#include <QString>
#include <optional>

class QSqlDatabase;

struct ValidatorCacheRecord {
    QString voteAccount;
    QString name;
    QString avatarUrl;
    int score = 0;
    QString version;
    QString city;
    QString country;
    qint64 updatedAt = 0;
};

class ValidatorCacheDb {
  public:
    static bool upsert(const QString& voteAccount, const QString& name, const QString& avatarUrl,
                       int score, const QString& version = {}, const QString& city = {},
                       const QString& country = {});

    static std::optional<ValidatorCacheRecord> getByVoteAccountRecord(const QString& voteAccount);

    static QList<ValidatorCacheRecord> getAllRecords();

    static QString getName(const QString& voteAccount);

    static bool clearAll();

  private:
    static QSqlDatabase db();
};

#endif // VALIDATORCACHEDB_H
