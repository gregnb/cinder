#include "DashboardPage.h"
#include "Theme.h"
#include "crypto/SignerManager.h"
#include "db/StakeAccountDb.h"
#include "db/ValidatorCacheDb.h"
#include "features/dashboard/DashboardHandler.h"
#include "services/AvatarCache.h"
#include "services/model/NetworkStats.h"
#include "util/TimeUtils.h"
#include "util/TxIconUtils.h"
#include "widgets/CardListItem.h"
#include "widgets/FlapDisplay.h"
#include "widgets/SplineChart.h"
#include "widgets/TpsBarChart.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

    QList<DashboardStakeItem> buildDashboardStakeItems(const QList<StakeAccountInfo>& accounts) {
        QList<DashboardStakeItem> items;
        items.reserve(accounts.size());
        for (const auto& sa : accounts) {
            DashboardStakeItem item;
            item.stakeAddress = sa.address;
            item.stateString = sa.stateString();
            item.solAmount = sa.solAmount();
            auto rec = ValidatorCacheDb::getByVoteAccountRecord(sa.voteAccount);
            if (rec) {
                item.validatorName = rec->name;
                item.avatarUrl = rec->avatarUrl;
            }
            if (item.validatorName.isEmpty()) {
                item.validatorName = sa.voteAccount.isEmpty() ? QObject::tr("Unknown")
                                                              : sa.voteAccount.left(8) + "...";
            }
            items.append(item);
        }
        return items;
    }

} // namespace

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent) {
    // Scroll area wrapping all dashboard content
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("dashboardContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(24);

    // Dashboard title
    QLabel* titleLabel = new QLabel(tr("Dashboard"));
    titleLabel->setObjectName("dashboardTitle");
    layout->addWidget(titleLabel);

    // Top section: balance card + staked SOL side by side
    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(24);
    topLayout->addWidget(createBalanceCard(), 3);
    topLayout->addWidget(createStakedSOL(), 1);
    layout->addLayout(topLayout);

    // Network stats (full width)
    layout->addWidget(createNetworkStatsCard());

    // Recent activity (full width)
    layout->addWidget(createRecentActivity());

    layout->addStretch();

    scroll->setWidget(content);

    outerLayout->addWidget(scroll);

    // Update relative timestamps every 60 seconds without DB access
    m_relativeTimeTimer.setInterval(60000);
    connect(&m_relativeTimeTimer, &QTimer::timeout, this,
            &DashboardPage::refreshRelativeTimestamps);
    m_relativeTimeTimer.start();
}

QDateTime DashboardPage::displayNow() const {
    return m_testNow.has_value() ? *m_testNow : QDateTime::currentDateTime();
}

void DashboardPage::setCurrentTimeForTesting(const QDateTime& now) { m_testNow = now; }

// ── Refresh from DB ──────────────────────────────────────────────

void DashboardPage::refresh(const QString& ownerAddress) {
    m_ownerAddress = ownerAddress;

    // Always initialize the staking card from persisted stake positions for this wallet.
    const QList<StakeAccountInfo> cached = StakeAccountDb::load(ownerAddress);
    updateStakingSummary(buildDashboardStakeItems(cached));

    const DashboardViewData data = m_dashboardHandler.buildViewData(ownerAddress);

    if (m_balanceAmount) {
        m_balanceAmount->setText(data.balanceAmountText);
    }
    if (m_balanceUsd) {
        m_balanceUsd->setText(data.balanceUsdText);
    }
    if (m_balanceUpdated) {
        m_balanceUpdated->setText(tr("Last updated %1").arg(displayNow().toString("h:mm:ss AP")));
    }

    if (m_chart) {
        m_chart->setVisible(data.showChart);
        bool chartChanged = m_chart->dataSize() != data.chartPoints.size();
        if (!chartChanged && !data.chartPoints.isEmpty()) {
            chartChanged = m_chart->lastDataPoint() != data.chartPoints.last();
        }
        if (chartChanged) {
            m_chart->setData(data.chartPoints);
        }
    }

    if (!m_activityLayout) {
        return;
    }

    QStringList newSigs;
    for (const auto& view : data.activities) {
        newSigs.append(view.signature);
    }

    QStringList oldSigs;
    for (const auto& ai : m_activityItems) {
        oldSigs.append(ai.signature);
    }

    if (newSigs == oldSigs && m_activityLoaded) {
        refreshRelativeTimestamps();
        return;
    }
    m_activityLoaded = true;

    while (m_activityLayout->count() > 1) {
        QLayoutItem* item = m_activityLayout->takeAt(m_activityLayout->count() - 1);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    m_activityItems.clear();

    if (data.activities.isEmpty()) {
        QLabel* empty = new QLabel(tr("No recent transactions"));
        empty->setObjectName("dashboardEmptyState");
        empty->setAlignment(Qt::AlignCenter);
        m_activityLayout->addWidget(empty);
    } else {
        for (const auto& view : data.activities) {
            QPixmap iconPx = txTypeIcon(view.iconName, 22, devicePixelRatioF());
            auto* item = new CardListItem();
            item->setIconPixmap(iconPx, view.iconObjectName, view.iconBackground);
            item->setTitle(view.title);
            item->setSubtitle(formatRelativeTime(view.blockTime));
            if (view.hasAmount) {
                item->setValue(view.amountText, view.amountObjectName);
            }

            item->setCursor(Qt::PointingHandCursor);
            item->setProperty("signature", view.signature);
            item->installEventFilter(this);

            m_activityLayout->addWidget(item);
            m_activityItems.append({item, view.blockTime, view.signature});
        }
    }

    m_activityLayout->addStretch();
}

void DashboardPage::refreshRelativeTimestamps() {
    for (const auto& entry : m_activityItems) {
        entry.widget->setSubtitle(formatRelativeTime(entry.blockTime));
    }
}

QWidget* DashboardPage::createBalanceCard() {
    QWidget* card = new QWidget();
    card->setObjectName("balanceCard");

    QGraphicsDropShadowEffect* cardShadow = new QGraphicsDropShadowEffect();
    cardShadow->setBlurRadius(30);
    cardShadow->setColor(Theme::shadowColor);
    cardShadow->setOffset(0, 8);
    card->setGraphicsEffect(cardShadow);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    // Title row: "Balance" on left, "Last updated" on right
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(30, 20, 30, 0);
    QLabel* balanceTitle = new QLabel(tr("Balance"));
    balanceTitle->setObjectName("cardTitle");
    titleRow->addWidget(balanceTitle);
    titleRow->addStretch();
    m_balanceUpdated = new QLabel();
    m_balanceUpdated->setObjectName("dashboardUpdatedLabel");
    m_balanceUpdated->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    titleRow->addWidget(m_balanceUpdated);
    cardLayout->addLayout(titleRow);

    // Balance amounts — padded
    QVBoxLayout* balanceSection = new QVBoxLayout();
    balanceSection->setContentsMargins(30, 8, 30, 15);

    m_balanceAmount = new QLabel("0 SOL");
    m_balanceAmount->setObjectName("balanceAmount");

    m_balanceUsd = new QLabel("$0.00 USD");
    m_balanceUsd->setObjectName("changeLabel");

    balanceSection->addWidget(m_balanceAmount);
    balanceSection->addWidget(m_balanceUsd);
    balanceSection->addStretch();

    cardLayout->addLayout(balanceSection);

    // Price chart — edge-to-edge within card, clipped to card's rounded corners
    m_chart = new SplineChart();
    m_chart->setBottomCornerRadius(Theme::cardRadius);
    m_chart->setShowYAxis(true);
    m_chart->setShowXAxis(true);
    m_chart->setValueFormatter(
        [](double, double y) { return QString::number(y, 'f', 6) + " SOL"; });
    cardLayout->addWidget(m_chart);

    return card;
}

QWidget* DashboardPage::createRecentActivity() {
    QWidget* card = new QWidget();
    card->setObjectName("activityCard");

    QGraphicsDropShadowEffect* cardShadow = new QGraphicsDropShadowEffect();
    cardShadow->setBlurRadius(25);
    cardShadow->setColor(Theme::shadowColorLight);
    cardShadow->setOffset(0, 6);
    card->setGraphicsEffect(cardShadow);

    m_activityLayout = new QVBoxLayout(card);
    m_activityLayout->setContentsMargins(25, 20, 25, 20);
    m_activityLayout->setSpacing(4);

    QLabel* title = new QLabel(tr("Recent Activity"));
    title->setObjectName("cardTitle");
    m_activityLayout->addWidget(title);

    // Placeholder until refresh() populates real data
    QLabel* loading = new QLabel(tr("Loading..."));
    loading->setObjectName("dashboardEmptyState");
    loading->setAlignment(Qt::AlignCenter);
    m_activityLayout->addWidget(loading);

    m_activityLayout->addStretch();
    return card;
}

QWidget* DashboardPage::createStakedSOL() {
    QWidget* card = new QWidget();
    card->setObjectName("stakedCard");

    QGraphicsDropShadowEffect* cardShadow = new QGraphicsDropShadowEffect();
    cardShadow->setBlurRadius(25);
    cardShadow->setColor(Theme::shadowColorLight);
    cardShadow->setOffset(0, 6);
    card->setGraphicsEffect(cardShadow);

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(25, 20, 25, 20);
    layout->setSpacing(4);

    m_stakingTitle = new QLabel(tr("Staking"));
    m_stakingTitle->setObjectName("cardTitle");
    layout->addWidget(m_stakingTitle);

    m_stakingContentLayout = new QVBoxLayout();
    m_stakingContentLayout->setContentsMargins(0, 0, 0, 0);
    m_stakingContentLayout->setSpacing(2);

    QLabel* empty = new QLabel(tr("No stake positions"));
    empty->setObjectName("dashboardEmptyState");
    empty->setAlignment(Qt::AlignCenter);
    m_stakingContentLayout->addWidget(empty);

    layout->addLayout(m_stakingContentLayout);
    layout->addStretch();
    return card;
}

static QString dashFormatSol(double sol) {
    if (sol >= 1000.0) {
        return QLocale(QLocale::English).toString(sol, 'f', 2) + " SOL";
    }
    if (sol >= 1.0) {
        return QString::number(sol, 'f', 4) + " SOL";
    }
    if (sol > 0) {
        return QString::number(sol, 'f', 6) + " SOL";
    }
    return "0 SOL";
}

void DashboardPage::updateStakingSummary(const QList<DashboardStakeItem>& items) {
    if (!m_stakingContentLayout) {
        return;
    }

    m_stakeItems = items;

    // Clear existing content
    while (m_stakingContentLayout->count() > 0) {
        QLayoutItem* child = m_stakingContentLayout->takeAt(0);
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    // Update title with count
    if (items.isEmpty()) {
        m_stakingTitle->setText(tr("Staking"));
        QLabel* empty = new QLabel(tr("No stake positions"));
        empty->setObjectName("dashboardEmptyState");
        empty->setAlignment(Qt::AlignCenter);
        m_stakingContentLayout->addWidget(empty);
        return;
    }

    m_stakingTitle->setText(tr("Staking (%1)").arg(items.size()));

    qreal dpr = devicePixelRatioF();
    for (const auto& stake : items) {
        auto* row = new CardListItem();
        row->layout()->setContentsMargins(0, 8, 0, 8);

        // Try loading validator avatar
        bool hasIcon = false;
        if (m_avatarCache && !stake.avatarUrl.isEmpty()) {
            QPixmap pm = m_avatarCache->get(stake.avatarUrl);
            if (!pm.isNull()) {
                row->setIconPixmap(AvatarCache::circleClip(pm, 36, dpr), "stakeValidatorIcon", "");
                hasIcon = true;
            }
        }
        if (!hasIcon) {
            // Fallback: first letter of validator name
            QString letter =
                stake.validatorName.isEmpty() ? "?" : stake.validatorName.left(1).toUpper();
            row->setIcon(letter, "stakeStateIcon",
                         "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                         " stop:0 rgba(99,102,241,0.6), stop:1 rgba(79,70,229,0.6))");
        }

        row->setTitle(stake.validatorName);
        row->setSubtitle(stake.stateString);
        row->setValue(dashFormatSol(stake.solAmount), "stakingAmount");
        row->setCursor(Qt::PointingHandCursor);
        row->setProperty("stakeAddress", stake.stakeAddress);
        row->installEventFilter(this);

        m_stakingContentLayout->addWidget(row);
    }
}

void DashboardPage::setAvatarCache(AvatarCache* cache) {
    m_avatarCache = cache;
    if (m_avatarCache) {
        connect(m_avatarCache, &AvatarCache::avatarReady, this, [this](const QString& url) {
            // Check if any displayed stake item uses this avatar URL
            bool relevant = false;
            for (const auto& item : std::as_const(m_stakeItems)) {
                if (item.avatarUrl == url) {
                    relevant = true;
                    break;
                }
            }
            if (relevant) {
                updateStakingSummary(m_stakeItems);
            }
        });
    }
}

// ── Network Stats Card ──────────────────────────────────────────

QWidget* DashboardPage::createNetworkStatsCard() {
    QWidget* card = new QWidget();
    card->setObjectName("networkCard");

    QGraphicsDropShadowEffect* cardShadow = new QGraphicsDropShadowEffect();
    cardShadow->setBlurRadius(25);
    cardShadow->setColor(Theme::shadowColorLight);
    cardShadow->setOffset(0, 6);
    card->setGraphicsEffect(cardShadow);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(25, 20, 25, 16);
    cardLayout->setSpacing(12);

    QHBoxLayout* netTitleRow = new QHBoxLayout();
    QLabel* title = new QLabel(tr("Network"));
    title->setObjectName("cardTitle");
    netTitleRow->addWidget(title);
    netTitleRow->addStretch();
    m_netUpdated = new QLabel();
    m_netUpdated->setObjectName("dashboardUpdatedLabel");
    m_netUpdated->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    netTitleRow->addWidget(m_netUpdated);
    cardLayout->addLayout(netTitleRow);

    // 2-column grid of stats
    auto addStat = [&](QGridLayout* grid, int row, int col, const QString& name,
                       QLabel*& valueLabel) {
        QLabel* lbl = new QLabel(name);
        lbl->setProperty("uiClass", "dashboardStatLabel");
        valueLabel = new QLabel("—");
        valueLabel->setObjectName("dashboardStatValue");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lbl, row, col * 2);
        grid->addWidget(valueLabel, row, col * 2 + 1);
    };

    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(30);
    grid->setVerticalSpacing(8);
    // Give value columns more stretch so they right-align nicely
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);

    // TPS — flap display
    auto* tpsLabel = new QLabel(tr("TPS"));
    tpsLabel->setProperty("uiClass", "dashboardStatLabel");
    m_netTps = new FlapDisplay();
    grid->addWidget(tpsLabel, 0, 0);
    grid->addWidget(m_netTps, 0, 1, Qt::AlignRight | Qt::AlignVCenter);

    // Slot — flap display
    auto* slotLabel = new QLabel(tr("Slot"));
    slotLabel->setProperty("uiClass", "dashboardStatLabel");
    m_netSlot = new FlapDisplay();
    grid->addWidget(slotLabel, 0, 2);
    grid->addWidget(m_netSlot, 0, 3, Qt::AlignRight | Qt::AlignVCenter);

    addStat(grid, 1, 0, tr("Epoch"), m_netEpoch);
    addStat(grid, 1, 1, tr("Progress"), m_netEpochProgress);
    addStat(grid, 2, 0, tr("Validators"), m_netValidators);
    addStat(grid, 2, 1, tr("Delinquent"), m_netDelinquent);

    // Supply — flap display (full number, no abbreviation)
    auto* supplyLabel = new QLabel(tr("Supply"));
    supplyLabel->setProperty("uiClass", "dashboardStatLabel");
    m_netSupply = new FlapDisplay();
    grid->addWidget(supplyLabel, 3, 0);
    grid->addWidget(m_netSupply, 3, 1, Qt::AlignRight | Qt::AlignVCenter);

    addStat(grid, 3, 1, tr("Circulating"), m_netCirculating); // col 1 → grid cols 2,3
    addStat(grid, 4, 0, tr("Inflation"), m_netInflation);
    addStat(grid, 4, 1, tr("Version"), m_netVersion);

    cardLayout->addLayout(grid);

    // TPS stacked bar chart (vote + non-vote breakdown)
    m_tpsChart = new TpsBarChart();
    m_tpsChart->setFixedHeight(120);
    cardLayout->addWidget(m_tpsChart);

    return card;
}

void DashboardPage::updateNetworkStats(const NetworkStats& stats) {
    QLocale loc(QLocale::English);

    if (m_netTps) {
        m_netTps->setValue(loc.toString(static_cast<int>(stats.currentTps)));
    }
    if (m_netSlot) {
        m_netSlot->setValue(loc.toString(static_cast<qlonglong>(stats.absoluteSlot)));
    }
    if (m_netEpoch) {
        m_netEpoch->setText(loc.toString(static_cast<qlonglong>(stats.epoch)));
    }
    if (m_netEpochProgress) {
        m_netEpochProgress->setText(QString::number(stats.epochProgressPct, 'f', 1) + "%");
    }
    if (m_netValidators) {
        m_netValidators->setText(loc.toString(stats.validatorCount));
    }
    if (m_netDelinquent) {
        m_netDelinquent->setText(QString::number(stats.delinquentPct, 'f', 1) + "%");
    }
    if (m_netSupply && stats.totalSupply > 0) {
        quint64 wholeSol = stats.totalSupply / 1000000000ULL;
        m_netSupply->setValue(loc.toString(static_cast<qlonglong>(wholeSol)) + " SOL");
    }
    if (m_netCirculating) {
        if (stats.totalSupply > 0) {
            double pct = static_cast<double>(stats.circulatingSupply) / stats.totalSupply * 100.0;
            m_netCirculating->setText(QString::number(pct, 'f', 1) + "%");
        }
    }
    if (m_netInflation) {
        m_netInflation->setText(QString::number(stats.inflationRate, 'f', 1) + "%");
    }
    if (m_netVersion) {
        m_netVersion->setText(stats.solanaVersion.isEmpty() ? "—" : stats.solanaVersion);
    }

    if (m_netUpdated) {
        m_netUpdated->setText(tr("Last updated %1").arg(displayNow().toString("h:mm:ss AP")));
    }

    // TPS stacked bar chart
    if (m_tpsChart && !stats.tpsSamples.isEmpty()) {
        m_tpsChart->setData(stats.tpsSamples);
    }
}

// ── Event Filter: clickable activity items ──────────────────────

bool DashboardPage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QString stakeAddress = obj->property("stakeAddress").toString();
        if (!stakeAddress.isEmpty()) {
            emit stakeClicked(stakeAddress);
            return true;
        }
        QString sig = obj->property("signature").toString();
        if (!sig.isEmpty()) {
            emit transactionClicked(sig);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Hardware Wallet Debug Card ───────────────────────────────────

void DashboardPage::setSignerManager(SignerManager* mgr) { m_signerManager = mgr; }
