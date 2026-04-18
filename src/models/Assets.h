#ifndef ASSETS_H
#define ASSETS_H

#include <QList>
#include <QPointF>
#include <QString>

enum class AssetsSortOption {
    ValueHighToLow,
    ValueLowToHigh,
    NameAToZ,
    NameZToA,
    HoldingHighToLow,
    HoldingLowToHigh,
    PriceHighToLow,
    PriceLowToHigh,
};

struct AssetInfo {
    QString mint;
    QString symbol;
    QString name;
    QString logoUrl;
    QString iconPath;
    double balance = 0.0;
    double priceUsd = 0.0;
    double valueUsd = 0.0;
};

struct AssetsViewData {
    QList<AssetInfo> assets;
    QList<QPointF> chartPoints;
    double totalPortfolioValue = 0.0;
};

#endif // ASSETS_H
