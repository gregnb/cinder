#ifndef NETWORKSTATSDB_H
#define NETWORKSTATSDB_H

#include "services/model/NetworkStats.h"

class QSqlDatabase;

class NetworkStatsDb {
  public:
    static bool save(const NetworkStats& stats);
    static bool load(NetworkStats& stats);

  private:
    static QSqlDatabase db();
};

#endif // NETWORKSTATSDB_H
