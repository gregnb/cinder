#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QtGlobal>

struct DashboardActivityView {
    QString signature;
    QString title;
    QString amountText;
    QString iconName;
    QString iconObjectName;
    QString iconBackground;
    QString amountObjectName;
    qint64 blockTime = 0;
    bool hasAmount = false;
};

struct DashboardViewData {
    QString balanceAmountText;
    QString balanceUsdText;
    bool showChart = false;
    QList<QPointF> chartPoints;
    QList<DashboardActivityView> activities;
};

#endif // DASHBOARD_H
