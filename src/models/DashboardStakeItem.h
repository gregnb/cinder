#ifndef DASHBOARDSTAKEITEM_H
#define DASHBOARDSTAKEITEM_H

#include <QMetaType>
#include <QString>

struct DashboardStakeItem {
    QString stakeAddress;
    QString validatorName;
    QString avatarUrl;
    QString stateString; // "Active", "Activating", etc.
    double solAmount = 0;
};

Q_DECLARE_METATYPE(DashboardStakeItem)

#endif // DASHBOARDSTAKEITEM_H
