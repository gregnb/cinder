#ifndef STAKEACCOUNTREWARDDB_H
#define STAKEACCOUNTREWARDDB_H

#include "services/model/StakeRewardInfo.h"
#include <QList>
#include <QString>
#include <optional>

class QSqlDatabase;

class StakeAccountRewardDb {
  public:
    static QList<StakeRewardInfo> load(const QString& stakeAddress);
    static void upsert(const QString& stakeAddress, const StakeRewardInfo& reward);
    static std::optional<quint64> maxEpoch(const QString& stakeAddress);
    static quint64 totalLamports(const QString& stakeAddress);

  private:
    static QSqlDatabase db();
};

#endif // STAKEACCOUNTREWARDDB_H
