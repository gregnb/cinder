#include "DashboardHandler.h"
#include "Theme.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include <QHash>
#include <QLocale>
#include <limits>

namespace {
    constexpr int PORTFOLIO_SNAPSHOT_LIMIT = 2000;
    constexpr int RECENT_ACTIVITY_LIMIT = 10;
    constexpr int CHART_MAX_BUCKETS = 200;

    constexpr double DEFAULT_TIME_SPAN_SECS = 86400.0;
    constexpr double MIN_RAMP_LENGTH_SECS = 3600.0;
    constexpr double RAMP_SPAN_FACTOR = 0.25;

    constexpr double RAMP_T1 = 0.05;
    constexpr double RAMP_Y1 = 0.95;
    constexpr double RAMP_T2 = 0.15;
    constexpr double RAMP_Y2 = 0.80;
    constexpr double RAMP_T3 = 0.40;
    constexpr double RAMP_Y3 = 0.50;
    constexpr double RAMP_T4 = 0.70;
    constexpr double RAMP_Y4 = 0.20;

    enum class ActivityKind {
        Receive,
        Send,
        Mint,
        Burn,
        CloseAccount,
        CreateAccount,
        CreateNonce,
        InitAccount,
        Unknown
    };

    ActivityKind activityKindFromType(const QString& type) {
        static const QHash<QString, ActivityKind> kByType = {
            {"receive", ActivityKind::Receive},
            {"send", ActivityKind::Send},
            {"mint", ActivityKind::Mint},
            {"burn", ActivityKind::Burn},
            {"close_account", ActivityKind::CloseAccount},
            {"create_account", ActivityKind::CreateAccount},
            {"create_nonce", ActivityKind::CreateNonce},
            {"init_account", ActivityKind::InitAccount},
        };
        return kByType.value(type, ActivityKind::Unknown);
    }

    static QString formatAmount(double amount, const QString& symbol) {
        QString s;
        if (amount >= 1000.0) {
            s = QLocale(QLocale::English).toString(amount, 'f', 2);
        } else if (amount >= 1.0) {
            s = QString::number(amount, 'f', 4);
        } else if (amount > 0) {
            s = QString::number(amount, 'f', 6);
            while (s.endsWith('0') && !s.endsWith(".00")) {
                s.chop(1);
            }
        } else {
            s = "0";
        }
        return s + " " + symbol;
    }

    static QString formatUsd(double value) {
        if (value >= 1.0) {
            return "$" + QLocale(QLocale::English).toString(value, 'f', 2);
        }
        if (value >= 0.01) {
            return "$" + QString::number(value, 'f', 2);
        }
        if (value > 0) {
            return "$" + QString::number(value, 'g', 4);
        }
        return "$0.00";
    }

} // namespace

DashboardViewData DashboardHandler::buildViewData(const QString& ownerAddress) const {
    DashboardViewData data;

    auto snapshot = PortfolioDb::getLatestSnapshotRecord(ownerAddress);
    double totalUsd = snapshot.has_value() ? snapshot->totalUsd : 0.0;
    double solPrice = snapshot.has_value() ? snapshot->solPrice : 0.0;
    double solEquiv = (solPrice > 0) ? totalUsd / solPrice : 0.0;

    data.balanceAmountText = formatAmount(solEquiv, "SOL");
    data.balanceUsdText = formatUsd(totalUsd) + " USD";

    QList<PortfolioSnapshotRecord> snapshots = PortfolioDb::getSnapshotsRecords(
        ownerAddress, 0, std::numeric_limits<qint64>::max(), PORTFOLIO_SNAPSHOT_LIMIT);
    data.chartPoints = buildChartPoints(snapshots, totalUsd);
    data.showChart = totalUsd > 0;

    const QList<TransactionRecord> txns =
        TransactionDb::getTransactionsRecords(ownerAddress, {}, {}, RECENT_ACTIVITY_LIMIT);
    data.activities.reserve(txns.size());
    for (const auto& tx : txns) {
        data.activities.append(buildActivityView(tx));
    }

    return data;
}

QList<QPointF> DashboardHandler::buildChartPoints(const QList<PortfolioSnapshotRecord>& snapshots,
                                                  double totalUsd) const {
    Q_UNUSED(totalUsd)

    QList<QPointF> rawPoints;
    rawPoints.reserve(snapshots.size());
    for (const auto& snap : snapshots) {
        double ts = static_cast<double>(snap.timestamp);
        double solValue = (snap.solPrice > 0) ? snap.totalUsd / snap.solPrice : 0;
        rawPoints.append(QPointF(ts, solValue));
    }

    QList<QPointF> chartPoints;
    const int maxBuckets = CHART_MAX_BUCKETS;
    if (rawPoints.size() > maxBuckets) {
        double tMin = rawPoints.first().x();
        double tMax = rawPoints.last().x();
        double bucketWidth = (tMax - tMin) / maxBuckets;

        QVector<double> bucketY(maxBuckets, 0);
        QVector<bool> bucketFilled(maxBuckets, false);
        for (const auto& pt : rawPoints) {
            int b = qMin(static_cast<int>((pt.x() - tMin) / bucketWidth), maxBuckets - 1);
            if (!bucketFilled[b]) {
                bucketY[b] = pt.y();
                bucketFilled[b] = true;
            } else {
                bucketY[b] = (bucketY[b] + pt.y()) * 0.5;
            }
        }

        int prev = -1;
        for (int b = 0; b < maxBuckets; ++b) {
            if (bucketFilled[b]) {
                if (prev >= 0 && b - prev > 1) {
                    for (int g = prev + 1; g < b; ++g) {
                        double t = static_cast<double>(g - prev) / (b - prev);
                        bucketY[g] = bucketY[prev] * (1.0 - t) + bucketY[b] * t;
                        bucketFilled[g] = true;
                    }
                }
                prev = b;
            }
        }

        for (int b = 0; b < maxBuckets; ++b) {
            if (bucketFilled[b]) {
                double ts = tMin + (b + 0.5) * bucketWidth;
                chartPoints.append(QPointF(ts, bucketY[b]));
            }
        }
    } else {
        chartPoints = rawPoints;
    }

    if (!chartPoints.isEmpty()) {
        double firstTs = chartPoints.first().x();
        double firstY = chartPoints.first().y();
        double span =
            chartPoints.size() > 1 ? chartPoints.last().x() - firstTs : DEFAULT_TIME_SPAN_SECS;
        double rampLen = qMax(span * RAMP_SPAN_FACTOR, MIN_RAMP_LENGTH_SECS);
        chartPoints.prepend(QPointF(firstTs - rampLen * RAMP_T1, firstY * RAMP_Y1));
        chartPoints.prepend(QPointF(firstTs - rampLen * RAMP_T2, firstY * RAMP_Y2));
        chartPoints.prepend(QPointF(firstTs - rampLen * RAMP_T3, firstY * RAMP_Y3));
        chartPoints.prepend(QPointF(firstTs - rampLen * RAMP_T4, firstY * RAMP_Y4));
        chartPoints.prepend(QPointF(firstTs - rampLen, 0.0));
    }

    return chartPoints;
}

DashboardActivityView DashboardHandler::buildActivityView(const TransactionRecord& tx) const {
    DashboardActivityView view;
    view.signature = tx.signature;
    view.blockTime = tx.blockTime;

    QString symbol = tx.token;
    if (tx.token != "SOL" && !tx.token.isEmpty()) {
        auto tokenInfo = TokenAccountDb::getTokenRecord(tx.token);
        symbol = tokenInfo.has_value() ? tokenInfo->symbol : QString();
        if (symbol.isEmpty()) {
            symbol = tx.token.left(4) + "..." + tx.token.right(4);
        }
    }

    switch (activityKindFromType(tx.activityType)) {
        case ActivityKind::Receive:
            view.iconName = "receive";
            view.title = QObject::tr("Received from %1").arg(tx.fromAddress);
            view.amountText = "+" + formatAmount(tx.amount, symbol);
            view.iconObjectName = "txIconPositive";
            view.iconBackground = Theme::txIconPositive;
            view.amountObjectName = "txAmountPositive";
            view.hasAmount = true;
            break;
        case ActivityKind::Send:
            view.iconName = "send";
            view.title = QObject::tr("Sent to %1").arg(tx.toAddress);
            view.amountText = "-" + formatAmount(tx.amount, symbol);
            view.iconObjectName = "txIconNegative";
            view.iconBackground = Theme::txIconNegative;
            view.amountObjectName = "txAmountNegative";
            view.hasAmount = true;
            break;
        case ActivityKind::Mint:
            view.iconName = "mint";
            view.title = QObject::tr("Minted %1").arg(symbol);
            view.amountText = "+" + formatAmount(tx.amount, symbol);
            view.iconObjectName = "txIconPositive";
            view.iconBackground = Theme::txIconMint;
            view.amountObjectName = "txAmountPositive";
            view.hasAmount = true;
            break;
        case ActivityKind::Burn:
            view.iconName = "burn";
            view.title = QObject::tr("Burned %1").arg(symbol);
            view.amountText = "-" + formatAmount(tx.amount, symbol);
            view.iconObjectName = "txIconNegative";
            view.iconBackground = Theme::txIconBurn;
            view.amountObjectName = "txAmountNegative";
            view.hasAmount = true;
            break;
        case ActivityKind::CloseAccount:
            view.iconName = "close";
            view.title = QObject::tr("Closed account %1").arg(tx.fromAddress);
            view.iconObjectName = "txIconNegative";
            view.iconBackground = Theme::txIconNeutral;
            view.amountObjectName = "txAmountNegative";
            break;
        case ActivityKind::CreateAccount:
            view.iconName = "create";
            view.title = QObject::tr("Created account %1").arg(tx.toAddress);
            view.amountText = "-" + formatAmount(tx.amount, "SOL");
            view.iconObjectName = "txIconNegative";
            view.iconBackground = Theme::txIconNeutral;
            view.amountObjectName = "txAmountNegative";
            view.hasAmount = true;
            break;
        case ActivityKind::CreateNonce:
            view.iconName = "lock";
            view.title = QObject::tr("Initialized nonce %1").arg(tx.toAddress);
            view.iconObjectName = "txIconPositive";
            view.iconBackground = Theme::txIconNeutral;
            view.amountObjectName = "txAmountPositive";
            break;
        case ActivityKind::InitAccount:
            view.iconName = "create";
            view.title = QObject::tr("Opened token account %1").arg(tx.toAddress);
            view.iconObjectName = "txIconPositive";
            view.iconBackground = Theme::txIconNeutral;
            view.amountObjectName = "txAmountPositive";
            break;
        case ActivityKind::Unknown:
            view.iconName = "default";
            view.title = tx.activityType;
            view.amountText = formatAmount(tx.amount, symbol);
            view.iconObjectName = "txIconNegative";
            view.iconBackground = Theme::txIconNeutral;
            view.amountObjectName = "txAmountNegative";
            view.hasAmount = true;
            break;
    }

    return view;
}
