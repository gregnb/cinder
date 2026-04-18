#ifndef STAKEACCOUNTDB_H
#define STAKEACCOUNTDB_H

#include "services/model/StakeAccountInfo.h"
#include <QList>
#include <QString>

class QSqlDatabase;

class StakeAccountDb {
  public:
    static void save(const QString& walletAddress, const QList<StakeAccountInfo>& accounts);
    static QList<StakeAccountInfo> load(const QString& walletAddress);
    static void remove(const QString& walletAddress);
    static void setTotalRewardsLamports(const QString& stakeAddress, quint64 lamports);

  private:
    static QSqlDatabase db();
};

#endif // STAKEACCOUNTDB_H
