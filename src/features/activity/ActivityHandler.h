#ifndef ACTIVITYHANDLER_H
#define ACTIVITYHANDLER_H

#include "db/TransactionDb.h"
#include "models/Activity.h"

class ActivityHandler {
  public:
    QStringList allActionTypes() const;
    QString badgeText(const QString& type) const;
    QString formatNumber(double amount) const;
    QString truncateAddr(const QString& addr) const;

    TransactionFilter buildFilter(const ActivityFilters& filters) const;
    bool hasActiveFilter(const ActivityFilters& filters) const;

    int totalRows(const QString& ownerAddress) const;
    int filteredRows(const QString& ownerAddress, const ActivityFilters& filters) const;
    QList<TransactionRecord> loadPage(const QString& ownerAddress, const ActivityFilters& filters,
                                      int pageSize, int currentPage) const;
    QList<ActivityRowView> buildRows(const QList<TransactionRecord>& txns) const;
};

#endif // ACTIVITYHANDLER_H
