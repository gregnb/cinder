#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include "features/dashboard/DashboardHandler.h"
#include "models/DashboardStakeItem.h"
#include <QDateTime>
#include <QTimer>
#include <QWidget>
#include <optional>

class QLabel;
class QVBoxLayout;
class SplineChart;
class TpsBarChart;
class CardListItem;
class FlapDisplay;
class SignerManager;
class AvatarCache;
struct NetworkStats;

class TxStatusAnimationWidget;

class DashboardPage : public QWidget {
    Q_OBJECT

  public:
    explicit DashboardPage(QWidget* parent = nullptr);

    void refresh(const QString& ownerAddress);
    void updateNetworkStats(const NetworkStats& stats);
    void updateStakingSummary(const QList<DashboardStakeItem>& items);
    void setSignerManager(SignerManager* mgr);
    void setAvatarCache(AvatarCache* cache);
    void setCurrentTimeForTesting(const QDateTime& now);
  signals:
    void transactionClicked(const QString& signature);
    void stakeClicked(const QString& stakeAddress);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    QWidget* createBalanceCard();
    QWidget* createRecentActivity();
    QWidget* createStakedSOL();
    QWidget* createNetworkStatsCard();
    void refreshRelativeTimestamps();
    QDateTime displayNow() const;

    QLabel* m_balanceAmount = nullptr;
    QLabel* m_balanceUsd = nullptr;
    QLabel* m_balanceUpdated = nullptr;
    SplineChart* m_chart = nullptr;

    // Network stats card labels
    FlapDisplay* m_netTps = nullptr;
    FlapDisplay* m_netSlot = nullptr;
    QLabel* m_netEpoch = nullptr;
    QLabel* m_netEpochProgress = nullptr;
    QLabel* m_netValidators = nullptr;
    QLabel* m_netDelinquent = nullptr;
    FlapDisplay* m_netSupply = nullptr;
    QLabel* m_netCirculating = nullptr;
    QLabel* m_netInflation = nullptr;
    QLabel* m_netVersion = nullptr;
    TpsBarChart* m_tpsChart = nullptr;
    QLabel* m_netUpdated = nullptr;
    QVBoxLayout* m_activityLayout = nullptr;
    QLabel* m_stakingTitle = nullptr;
    QVBoxLayout* m_stakingContentLayout = nullptr;
    QString m_ownerAddress;
    AvatarCache* m_avatarCache = nullptr;
    QList<DashboardStakeItem> m_stakeItems;

    // Relative timestamp refresh (no DB hit)
    QTimer m_relativeTimeTimer;
    struct ActivityItem {
        CardListItem* widget;
        qint64 blockTime;
        QString signature;
    };
    QList<ActivityItem> m_activityItems;
    bool m_activityLoaded = false;
    std::optional<QDateTime> m_testNow;

    SignerManager* m_signerManager = nullptr;
    DashboardHandler m_dashboardHandler;
};

#endif // DASHBOARDPAGE_H
