#ifndef ACTIVITY_H
#define ACTIVITY_H

#include <QSet>
#include <QString>

struct ActivityFilters {
    QString signature;
    qint64 timeFrom = 0;
    qint64 timeTo = 0;
    QSet<QString> actionTypes;
    QString fromAddress;
    QString toAddress;
    double amountMin = -1;
    double amountMax = -1;
    QString token;
};

struct ActivityRowView {
    QString signature;
    qint64 blockTime = 0;
    QString activityType;
    QString fromAddress;
    QString toAddress;
    double amount = 0.0;
    QString tokenSymbol;
    QString iconPath;
    QString logoUrl;
    bool err = false;

    QString signatureDisplay;
    QString badgeText;
    QString fromDisplay;
    QString toDisplay;
    QString amountText;
    QString amountColor;
};

#endif // ACTIVITY_H
