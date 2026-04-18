#include "features/assets/AssetsHandler.h"

#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "tx/KnownTokens.h"

#include <QCoreApplication>
#include <QMap>
#include <QVector>
#include <algorithm>
#include <limits>

namespace {
    constexpr int MAX_CHART_SNAPSHOTS = 2000;
    constexpr int MAX_CHART_BUCKETS = 200;
    constexpr int SINGLE_POINT_OFFSET_SECONDS = 3600;
} // namespace

AssetsHandler::AssetsHandler(QObject* parent) : QObject(parent) {}

AssetsViewData AssetsHandler::loadAssets(const QString& ownerAddress) const {
    AssetsViewData data;

    const QList<TokenAccountRecord> accounts =
        TokenAccountDb::getAccountsByOwnerRecords(ownerAddress);

    const auto snapshot = PortfolioDb::getLatestSnapshotRecord(ownerAddress);
    QMap<QString, double> priceMap;
    if (snapshot.has_value()) {
        const QList<TokenSnapshotRecord> tokenSnapshots =
            PortfolioDb::getTokenSnapshotsRecords(snapshot->id);
        for (const auto& tokenSnapshot : tokenSnapshots) {
            priceMap[tokenSnapshot.mint] = tokenSnapshot.priceUsd;
        }
    }

    for (const auto& account : accounts) {
        AssetInfo info;
        info.mint = account.tokenAddress;
        info.symbol = account.symbol;
        info.name = account.name;
        info.logoUrl = account.logoUrl;
        info.iconPath = iconForMint(account.tokenAddress);
        info.balance = account.balance.toDouble();
        info.priceUsd = priceMap.value(info.mint, 0.0);
        info.valueUsd = info.balance * info.priceUsd;

        data.totalPortfolioValue += info.valueUsd;
        if (info.balance <= 0) {
            continue;
        }

        data.assets.append(info);
    }

    if (data.totalPortfolioValue > 0) {
        data.chartPoints = buildChartPoints(ownerAddress);
    }

    return data;
}

void AssetsHandler::sortAssets(QList<AssetInfo>& assets, AssetsSortOption option) const {
    std::sort(assets.begin(), assets.end(), [option](const AssetInfo& a, const AssetInfo& b) {
        switch (option) {
            case AssetsSortOption::ValueHighToLow:
                return a.valueUsd > b.valueUsd;
            case AssetsSortOption::ValueLowToHigh:
                return a.valueUsd < b.valueUsd;
            case AssetsSortOption::NameAToZ:
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            case AssetsSortOption::NameZToA:
                return a.name.compare(b.name, Qt::CaseInsensitive) > 0;
            case AssetsSortOption::HoldingHighToLow:
                return a.balance > b.balance;
            case AssetsSortOption::HoldingLowToHigh:
                return a.balance < b.balance;
            case AssetsSortOption::PriceHighToLow:
                return a.priceUsd > b.priceUsd;
            case AssetsSortOption::PriceLowToHigh:
                return a.priceUsd < b.priceUsd;
        }

        return false;
    });
}

AssetsSortOption AssetsHandler::sortOptionFromLabel(const QString& label) const {
    if (label == labelForSortOption(AssetsSortOption::ValueLowToHigh)) {
        return AssetsSortOption::ValueLowToHigh;
    }
    if (label == labelForSortOption(AssetsSortOption::NameAToZ)) {
        return AssetsSortOption::NameAToZ;
    }
    if (label == labelForSortOption(AssetsSortOption::NameZToA)) {
        return AssetsSortOption::NameZToA;
    }
    if (label == labelForSortOption(AssetsSortOption::HoldingHighToLow)) {
        return AssetsSortOption::HoldingHighToLow;
    }
    if (label == labelForSortOption(AssetsSortOption::HoldingLowToHigh)) {
        return AssetsSortOption::HoldingLowToHigh;
    }
    if (label == labelForSortOption(AssetsSortOption::PriceHighToLow)) {
        return AssetsSortOption::PriceHighToLow;
    }
    if (label == labelForSortOption(AssetsSortOption::PriceLowToHigh)) {
        return AssetsSortOption::PriceLowToHigh;
    }

    return AssetsSortOption::ValueHighToLow;
}

QString AssetsHandler::labelForSortOption(AssetsSortOption option) const {
    switch (option) {
        case AssetsSortOption::ValueHighToLow:
            return QCoreApplication::translate("AssetsPage", "Value: High to Low");
        case AssetsSortOption::ValueLowToHigh:
            return QCoreApplication::translate("AssetsPage", "Value: Low to High");
        case AssetsSortOption::NameAToZ:
            return QCoreApplication::translate("AssetsPage", "Name: A to Z");
        case AssetsSortOption::NameZToA:
            return QCoreApplication::translate("AssetsPage", "Name: Z to A");
        case AssetsSortOption::HoldingHighToLow:
            return QCoreApplication::translate("AssetsPage", "Holding: High to Low");
        case AssetsSortOption::HoldingLowToHigh:
            return QCoreApplication::translate("AssetsPage", "Holding: Low to High");
        case AssetsSortOption::PriceHighToLow:
            return QCoreApplication::translate("AssetsPage", "Price: High to Low");
        case AssetsSortOption::PriceLowToHigh:
            return QCoreApplication::translate("AssetsPage", "Price: Low to High");
    }

    return QString();
}

QList<AssetsSortOption> AssetsHandler::sortOptions() const {
    return {
        AssetsSortOption::ValueHighToLow,   AssetsSortOption::ValueLowToHigh,
        AssetsSortOption::NameAToZ,         AssetsSortOption::NameZToA,
        AssetsSortOption::HoldingHighToLow, AssetsSortOption::HoldingLowToHigh,
        AssetsSortOption::PriceHighToLow,   AssetsSortOption::PriceLowToHigh,
    };
}

QString AssetsHandler::iconForMint(const QString& mint) {
    const KnownToken token = resolveKnownToken(mint);
    return token.iconPath;
}

QList<QPointF> AssetsHandler::buildChartPoints(const QString& ownerAddress) {
    const QList<PortfolioSnapshotRecord> snapshots = PortfolioDb::getSnapshotsRecords(
        ownerAddress, 0, std::numeric_limits<qint64>::max(), MAX_CHART_SNAPSHOTS);

    QList<QPointF> rawPoints;
    for (const auto& snapshot : snapshots) {
        const double timestamp = static_cast<double>(snapshot.timestamp);
        rawPoints.append(QPointF(timestamp, snapshot.totalUsd));
    }

    if (rawPoints.isEmpty()) {
        return {};
    }

    QList<QPointF> chartPoints;
    if (rawPoints.size() > MAX_CHART_BUCKETS) {
        const double tMin = rawPoints.first().x();
        const double tMax = rawPoints.last().x();
        const double bucketWidth = (tMax - tMin) / MAX_CHART_BUCKETS;

        QVector<double> bucketY(MAX_CHART_BUCKETS, 0);
        QVector<bool> bucketFilled(MAX_CHART_BUCKETS, false);
        for (const auto& point : rawPoints) {
            const int bucket =
                qMin(static_cast<int>((point.x() - tMin) / bucketWidth), MAX_CHART_BUCKETS - 1);
            if (!bucketFilled[bucket]) {
                bucketY[bucket] = point.y();
                bucketFilled[bucket] = true;
            } else {
                bucketY[bucket] = (bucketY[bucket] + point.y()) * 0.5;
            }
        }

        int previousFilled = -1;
        for (int bucket = 0; bucket < MAX_CHART_BUCKETS; ++bucket) {
            if (!bucketFilled[bucket]) {
                continue;
            }

            if (previousFilled >= 0 && bucket - previousFilled > 1) {
                for (int gap = previousFilled + 1; gap < bucket; ++gap) {
                    const double t =
                        static_cast<double>(gap - previousFilled) / (bucket - previousFilled);
                    bucketY[gap] = bucketY[previousFilled] * (1.0 - t) + bucketY[bucket] * t;
                    bucketFilled[gap] = true;
                }
            }
            previousFilled = bucket;
        }

        for (int bucket = 0; bucket < MAX_CHART_BUCKETS; ++bucket) {
            if (!bucketFilled[bucket]) {
                continue;
            }

            const double timestamp = tMin + (bucket + 0.5) * bucketWidth;
            chartPoints.append(QPointF(timestamp, bucketY[bucket]));
        }
    } else {
        chartPoints = rawPoints;
    }

    if (chartPoints.size() == 1) {
        const QPointF point = chartPoints.first();
        chartPoints.prepend(QPointF(point.x() - SINGLE_POINT_OFFSET_SECONDS, point.y()));
    }

    return chartPoints;
}
