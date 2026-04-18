#ifndef ASSETSHANDLER_H
#define ASSETSHANDLER_H

#include "models/Assets.h"
#include <QObject>
#include <QString>

class AssetsHandler : public QObject {
    Q_OBJECT

  public:
    explicit AssetsHandler(QObject* parent = nullptr);

    AssetsViewData loadAssets(const QString& ownerAddress) const;
    void sortAssets(QList<AssetInfo>& assets, AssetsSortOption option) const;

    AssetsSortOption sortOptionFromLabel(const QString& label) const;
    QString labelForSortOption(AssetsSortOption option) const;
    QList<AssetsSortOption> sortOptions() const;

  private:
    static QString iconForMint(const QString& mint);
    static QList<QPointF> buildChartPoints(const QString& ownerAddress);
};

#endif // ASSETSHANDLER_H
