#ifndef DASHBOARDHANDLER_H
#define DASHBOARDHANDLER_H

#include "models/Dashboard.h"

class QString;
struct PortfolioSnapshotRecord;
struct TransactionRecord;

class DashboardHandler {
  public:
    DashboardViewData buildViewData(const QString& ownerAddress) const;

  private:
    QList<QPointF> buildChartPoints(const QList<PortfolioSnapshotRecord>& snapshots,
                                    double totalUsd) const;
    DashboardActivityView buildActivityView(const TransactionRecord& tx) const;
};

#endif // DASHBOARDHANDLER_H
