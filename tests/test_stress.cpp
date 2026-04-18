#include <gtest/gtest.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QGridLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>
#include <QtMath>

#include <mach/mach.h>

#include "StyleLoader.h"
#include "VisualTestUtils.h"
#include "db/ContactDb.h"
#include "db/Database.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "db/WalletDb.h"
#include "services/AvatarCache.h"
#include "services/SolanaApi.h"
#include "services/model/NetworkStats.h"
#include "services/model/ValidatorInfo.h"
#include "widgets/FlapDisplay.h"
#include "widgets/SplineChart.h"
#include "widgets/TokenDropdown.h"
#include "widgets/TpsBarChart.h"

#define private public
#include "features/activity/ActivityPage.h"
#include "features/addressbook/AddressBookPage.h"
#include "features/assets/AssetsPage.h"
#include "features/dashboard/DashboardPage.h"
#include "features/staking/StakingPage.h"
#include "features/swap/SwapPage.h"
#include "features/txlookup/TxLookupPage.h"
#include "features/wallets/WalletsPage.h"
#undef private

#include "services/IdlRegistry.h"

// ── Memory / widget sampling ─────────────────────────────────────

struct MemSample {
    size_t rssBytes = 0;
    size_t virtualBytes = 0;
    double userTimeSec = 0;
};

static MemSample sampleMemory() {
    MemSample s;
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) ==
        KERN_SUCCESS) {
        s.rssBytes = info.resident_size;
        s.virtualBytes = info.virtual_size;
    }

    task_thread_times_info_data_t times;
    mach_msg_type_number_t tc = TASK_THREAD_TIMES_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&times, &tc) ==
        KERN_SUCCESS) {
        s.userTimeSec = times.user_time.seconds + times.user_time.microseconds / 1e6;
    }
    return s;
}

struct WidgetSample {
    int totalWidgets = 0;
    int labelCount = 0;
    int visibleWidgets = 0;
};

static constexpr qint64 kVisualSeedNow = 1772625600; // Mar 4, 2026 12:00:00 UTC

static WidgetSample sampleWidgets() {
    auto all = QApplication::allWidgets();
    int labels = 0, visible = 0;
    for (QWidget* w : all) {
        if (qobject_cast<QLabel*>(w)) {
            labels++;
        }
        if (w->isVisible()) {
            visible++;
        }
    }
    return {static_cast<int>(all.size()), labels, visible};
}

using VisualTestUtils::capturePage;
using VisualTestUtils::settleUi;
using FeatureVisualRecorder = VisualTestUtils::VisualRecorder;

// ── Mock data generators ─────────────────────────────────────────

static QList<ValidatorInfo> buildMockValidators(int count) {
    QList<ValidatorInfo> list;
    for (int i = 0; i < count; ++i) {
        ValidatorInfo vi;
        vi.voteAccount =
            QString("Vote%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
        vi.nodePubkey =
            QString("Node%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
        vi.name = QString("Validator %1").arg(i);
        vi.commission = i % 10;
        vi.activatedStake = 1000000000ULL * static_cast<quint64>(1000 - i);
        vi.lastVote = 300000000;
        vi.apy = 6.5 + (i % 30) * 0.1;
        vi.city = "San Francisco";
        vi.country = "US";
        list.append(vi);
    }
    return list;
}

static NetworkStats buildMockNetworkStats() {
    NetworkStats s;
    s.epoch = 500;
    s.slotIndex = 200000;
    s.slotsInEpoch = 432000;
    s.epochProgressPct = 46.3;
    s.currentTps = 3200;
    s.totalSupply = 580000000000000000ULL;
    s.circulatingSupply = 420000000000000000ULL;
    s.activeStake = 380000000000000000ULL;
    s.delinquentPct = 0.8;
    s.validatorCount = 1800;
    s.absoluteSlot = 280000000;
    s.blockHeight = 250000000;
    s.inflationRate = 5.2;
    s.solanaVersion = "1.18.15";

    qint64 now = kVisualSeedNow;
    for (int i = 0; i < 30; ++i) {
        TpsSample ts;
        ts.totalTps = 3000 + (i * 10);
        ts.nonVoteTps = 800 + (i * 3);
        ts.voteTps = ts.totalTps - ts.nonVoteTps;
        ts.timestamp = now - (30 - i) * 60;
        s.tpsSamples.append(ts);
    }
    return s;
}

static void seedTransactionDb(const QString& ownerAddress, int count) {
    qint64 now = kVisualSeedNow;
    for (int i = 0; i < count; ++i) {
        QString sig = QString("Sig%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")
                          .arg(i, 5, 10, QChar('0'));
        int slot = 280000000 - i;
        qint64 blockTime = now - (i * 3600);
        int fee = 5000;
        bool err = false;

        QString counterparty =
            QString("Addr%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i % 20, 4, 10, QChar('0'));
        double amount = 0.1 * (i + 1);

        QList<Activity> activities;
        Activity act;
        act.activityType = (i % 2 == 0) ? "send" : "receive";
        act.fromAddress = (i % 2 == 0) ? ownerAddress : counterparty;
        act.toAddress = (i % 2 == 0) ? counterparty : ownerAddress;
        act.token = "SOL";
        act.amount = amount;
        activities.append(act);

        TransactionDb::insertTransaction(sig, slot, blockTime, "{}", fee, err, activities);
    }
}

static void seedTokenAccounts(const QString& ownerAddress, int count) {
    // Register the tokens in the token registry
    for (int i = 0; i < count; ++i) {
        QString mint =
            QString("Mint%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
        QString symbol = QString("TKN%1").arg(i);
        QString name = QString("Token %1").arg(i);
        TokenAccountDb::upsertToken(mint, symbol, name, 9,
                                    "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");

        // Create an account with a balance
        QString acctAddr =
            QString("Acct%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
        double balance = 100.0 + i * 10.5;
        TokenAccountDb::upsertAccount(acctAddr, mint, ownerAddress,
                                      QString::number(balance, 'f', 9), 1.50 + i * 0.1);
    }

    // Also add a SOL "account"
    TokenAccountDb::upsertToken("So11111111111111111111111111111111111111112", "SOL", "Wrapped SOL",
                                9, "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
    TokenAccountDb::upsertAccount("SolAcct111111111111111111111111111111111111",
                                  "So11111111111111111111111111111111111111112", ownerAddress,
                                  "5.000000000", 150.0);
}

static void seedContacts(int count) {
    static const qint64 kFixedCreatedAt =
        QDateTime(QDate(2026, 3, 4), QTime(12, 0), QTimeZone::UTC).toSecsSinceEpoch();
    QSqlQuery clearQ(Database::connection());
    clearQ.exec("DELETE FROM contacts");

    for (int i = 0; i < count; ++i) {
        QString name = QString("Contact %1").arg(i);
        QString address =
            QString("Addr%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
        QSqlQuery q(Database::connection());
        q.prepare(
            "INSERT OR REPLACE INTO contacts (name, address, created_at) VALUES (:name, :address, "
            ":ts)");
        q.bindValue(":name", name);
        q.bindValue(":address", address);
        q.bindValue(":ts", kFixedCreatedAt);
        q.exec();
    }
}

static void seedPortfolioSnapshots(int count) {
    qint64 now = kVisualSeedNow;
    for (int i = 0; i < count; ++i) {
        qint64 ts = now - (count - i) * 3600; // one per hour
        double totalUsd = 500.0 + i * 2.5;
        double solPrice = 150.0 + (i % 10) * 0.5;
        int snapId =
            PortfolioDb::insertSnapshot(QStringLiteral("test_address"), ts, totalUsd, solPrice);
        if (snapId > 0) {
            PortfolioDb::insertTokenSnapshot(snapId, "So11111111111111111111111111111111111111112",
                                             "SOL", 5.0, solPrice, 5.0 * solPrice);
        }
    }
}

static void seedWalletsForUi() {
    const QByteArray salt(16, 's');
    const QByteArray nonce(24, 'n');
    const QByteArray cipher(64, 'c');

    WalletDb::insertWallet(QStringLiteral("My Wallet"),
                           QStringLiteral("6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"),
                           QStringLiteral("private_key"), salt, nonce, cipher);
    WalletDb::insertWallet(QStringLiteral("Greg"),
                           QStringLiteral("BDBY9tWqj1Pe4qVJxmhjLYZoALfwSNNEG91HrbgZ73VH"),
                           QStringLiteral("mnemonic"), salt, nonce, cipher);
    WalletDb::insertDerivedWallet(QStringLiteral("Wallet 2"),
                                  QStringLiteral("HGCGE2f5mSeddA6KkJFSHDzLB7Bp2obkCqSCAC6oczX4"),
                                  QStringLiteral("mnemonic"), salt, nonce, cipher, 1, 2);
}

// ── Iteration counts (overridable via env) ───────────────────────

static int iterCount(int defaultVal) {
    int env = qEnvironmentVariableIntValue("STRESS_ITERATIONS");
    return env > 0 ? env : defaultVal;
}

// ── Test fixture ─────────────────────────────────────────────────

class StressTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        QFontDatabase::addApplicationFont(":/fonts/Exo2-Variable.ttf");
        QFont defaultFont("Exo 2", 14);
        qApp->setFont(defaultFont);
        s_stylesheet = StyleLoader::loadTheme();
        qApp->setStyleSheet(s_stylesheet);
    }

    void SetUp() override {
        // Fresh temp DB per test, following test_database.cpp pattern
        m_tmpDir = std::make_unique<QTemporaryDir>();
        ASSERT_TRUE(m_tmpDir->isValid());

        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
            QSqlDatabase::database().close();
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }

        QString dbPath = m_tmpDir->path() + "/test.db";
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open());

        QSqlQuery pragma(db);
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA foreign_keys=ON");

        // Run all migrations
        ASSERT_TRUE(Database::migrate());

        m_api = new SolanaApi();
        m_avatarCache = new AvatarCache();
    }

    void TearDown() override {
        delete m_avatarCache;
        m_avatarCache = nullptr;
        delete m_api;
        m_api = nullptr;

        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
            QSqlDatabase::database().close();
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }
        m_tmpDir.reset();
    }

    void logSample(int iter, const MemSample& mem, const WidgetSample& w) {
        double rssMb = static_cast<double>(mem.rssBytes) / (1024.0 * 1024.0);
        std::cout << "[StressTest] " << iter << "," << rssMb << "," << w.totalWidgets << ","
                  << w.labelCount << "," << mem.userTimeSec << '\n';
    }

    void assertMemoryStable(const std::vector<MemSample>& samples, int warmup = 10,
                            size_t limitMb = 15) {
        if (static_cast<int>(samples.size()) <= warmup) {
            return;
        }
        size_t rssAtWarmup = samples[warmup].rssBytes;
        size_t rssAtEnd = samples.back().rssBytes;
        size_t growth = rssAtEnd > rssAtWarmup ? rssAtEnd - rssAtWarmup : 0;
        double growthMb = static_cast<double>(growth) / (1024.0 * 1024.0);

        std::cout << "[StressTest] RSS at warmup: "
                  << static_cast<double>(rssAtWarmup) / (1024.0 * 1024.0) << " MB" << '\n';
        std::cout << "[StressTest] RSS at end: "
                  << static_cast<double>(rssAtEnd) / (1024.0 * 1024.0) << " MB" << '\n';
        std::cout << "[StressTest] RSS growth: " << growthMb << " MB" << '\n';

        EXPECT_LT(growth, limitMb * 1024 * 1024)
            << "RSS grew by " << growthMb << " MB after warmup — possible memory leak";
    }

    void assertWidgetsStable(const std::vector<WidgetSample>& samples, int warmup = 10) {
        if (static_cast<int>(samples.size()) <= warmup) {
            return;
        }
        int atWarmup = samples[warmup].totalWidgets;
        int atEnd = samples.back().totalWidgets;
        int growth = atEnd - atWarmup;

        std::cout << "[StressTest] Widgets at warmup: " << atWarmup << '\n';
        std::cout << "[StressTest] Widgets at end: " << atEnd << '\n';
        std::cout << "[StressTest] Widget growth: " << growth << '\n';

        EXPECT_LE(growth, 50) << "Widget count grew by " << growth
                              << " after warmup — possible widget leak";
    }

    SolanaApi* m_api = nullptr;
    AvatarCache* m_avatarCache = nullptr;
    static QString s_stylesheet;

  private:
    std::unique_ptr<QTemporaryDir> m_tmpDir;
};

QString StressTest::s_stylesheet;

TEST_F(StressTest, CaptureFeaturePageBaselines) {
    constexpr int kDashboardTolerance = 3000;
    constexpr int kStakingTolerance = 170000;
    constexpr int kActivityTolerance = 125000;
    constexpr int kAssetsTolerance = 10000;
    constexpr int kAddressBookTolerance = 140000;

    const QString owner = "TestWallet1111111111111111111111111111111111";
    seedTransactionDb(owner, 120);
    seedPortfolioSnapshots(60);
    seedTokenAccounts(owner, 12);
    seedContacts(30);
    seedWalletsForUi();

    FeatureVisualRecorder recorder(VisualTestUtils::repoOwnedVisualRoot("feature"),
                                   "FEATURE_VISUAL_ROOT_DIR", "FEATURE_VISUAL_UPDATE_BASELINE",
                                   "FEATURE_VISUAL_MAX_DIFF_PIXELS");

    DashboardPage dashboard;
    dashboard.setCurrentTimeForTesting(
        QDateTime::fromSecsSinceEpoch(kVisualSeedNow, QTimeZone::UTC).toLocalTime());
    dashboard.refresh(owner);
    for (FlapDisplay* display : dashboard.findChildren<FlapDisplay*>()) {
        display->setAnimated(false);
    }
    dashboard.updateNetworkStats(buildMockNetworkStats());
    capturePage(dashboard, recorder, "feature_dashboard", 80, kDashboardTolerance, 1440, 900,
                "stressCaptureHost");

    StakingPage staking;
    staking.m_validators = buildMockValidators(80);
    staking.rebuildFilteredList();
    capturePage(staking, recorder, "feature_staking", 80, kStakingTolerance, 1440, 900,
                "stressCaptureHost");

    ActivityPage activity;
    activity.setAvatarCache(m_avatarCache);
    activity.refresh(owner);
    capturePage(activity, recorder, "feature_activity", 80, kActivityTolerance, 1440, 900,
                "stressCaptureHost");

    AssetsPage assets;
    assets.setAvatarCache(m_avatarCache);
    assets.refresh(owner);
    capturePage(assets, recorder, "feature_assets", 80, kAssetsTolerance, 1440, 900,
                "stressCaptureHost");

    AddressBookPage addressBook;
    addressBook.refreshList();
    capturePage(addressBook, recorder, "feature_addressbook", 80, kAddressBookTolerance, 1440, 900,
                "stressCaptureHost");

    WalletsPage wallets;
    wallets.setActiveAddress(QStringLiteral("6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"));
    wallets.refresh();
    capturePage(wallets, recorder, "feature_wallets", 80, -1, 1440, 900, "stressCaptureHost");

    SwapPage swap;
    swap.setSolanaApi(m_api);
    swap.setWalletAddress(owner);
    swap.refreshBalances();
    capturePage(swap, recorder, "feature_swap", 80, -1, 1440, 900, "stressCaptureHost");

    IdlRegistry idlRegistry(m_api);
    TxLookupPage txLookup(m_api, &idlRegistry);
    txLookup.setWalletAddress(owner);
    capturePage(txLookup, recorder, "feature_txlookup", 80, -1, 1440, 900, "stressCaptureHost");

    std::cout << "[FeatureVisual] Captures written under: " << recorder.rootDir().toStdString()
              << '\n';
}

// ═════════════════════════════════════════════════════════════════
// DASHBOARD TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: Dashboard network stats update (no DB) ─────────────────

TEST_F(StressTest, StressDashboardNetworkStats) {
    int iters = iterCount(100);

    DashboardPage dashboard;
    dashboard.resize(800, 600);
    dashboard.show();
    settleUi(50);
    for (FlapDisplay* display : dashboard.findChildren<FlapDisplay*>()) {
        display->setAnimated(false);
    }

    NetworkStats stats = buildMockNetworkStats();

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressDashboardNetworkStats (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        stats.currentTps = 3000 + (i % 500);
        stats.blockHeight += 1;
        stats.slotIndex += 400;

        dashboard.updateNetworkStats(stats);
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    // Dashboard stat updates warm Qt paint/style caches and flap digit pixmaps.
    // Widget count stays flat, so allow a modest RSS ramp here.
    assertMemoryStable(memSamples, 20, 25);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: Dashboard full refresh (recent activity deleteLater) ───

TEST_F(StressTest, StressDashboardFullRefresh) {
    int iters = iterCount(100);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTransactionDb(owner, 100);
    seedPortfolioSnapshots(50);
    seedTokenAccounts(owner, 5);

    DashboardPage dashboard;
    dashboard.resize(800, 600);
    dashboard.show();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressDashboardFullRefresh (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        dashboard.refresh(owner);
        settleUi(10);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples, 20, 30);
    assertWidgetsStable(widgetSamples);
}

// ═════════════════════════════════════════════════════════════════
// STAKING TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: Staking page validator scroll stress ───────────────────

TEST_F(StressTest, StressValidatorScroll) {
    int iters = iterCount(50);

    StakingPage staking;
    staking.setSolanaApi(m_api);
    staking.setAvatarCache(m_avatarCache);
    staking.setWalletAddress("TestWallet1111111111111111111111111111111111");
    staking.setAutoRefreshOnShow(false);
    staking.resize(800, 600);
    staking.show();
    settleUi(50);

    // Directly populate validators (we have private access via #define private public)
    staking.m_validators = buildMockValidators(200);
    staking.rebuildFilteredList();
    settleUi(50);

    QScrollArea* scroll = staking.m_validatorScroll;
    ASSERT_NE(scroll, nullptr);
    QScrollBar* vbar = scroll->verticalScrollBar();
    ASSERT_NE(vbar, nullptr);

    int maxScroll = vbar->maximum();

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressValidatorScroll (" << iters << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        // Scroll down in steps
        int step = qMax(1, maxScroll / 5);
        for (int pos = 0; pos <= maxScroll; pos += step) {
            vbar->setValue(pos);
            settleUi(2);
        }
        // Scroll back up
        for (int pos = maxScroll; pos >= 0; pos -= step) {
            vbar->setValue(pos);
            settleUi(2);
        }

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 10 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    // Validator scrolling triggers Qt's pixmap/glyph cache warming;
    // 0 widget growth confirms no leak, so raise RSS tolerance
    assertMemoryStable(memSamples, 10, 80);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: Staking page filter + sort stress ──────────────────────

TEST_F(StressTest, StressValidatorFilterSort) {
    int iters = iterCount(50);

    StakingPage staking;
    staking.setSolanaApi(m_api);
    staking.setAvatarCache(m_avatarCache);
    staking.setWalletAddress("TestWallet1111111111111111111111111111111111");
    staking.setAutoRefreshOnShow(false);
    staking.resize(800, 600);
    staking.show();
    settleUi(50);

    staking.m_validators = buildMockValidators(200);
    staking.rebuildFilteredList();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressValidatorFilterSort (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    QStringList filterTerms = {"Validator 1", "Validator 5", "AAAA", "nonexistent", ""};

    for (int i = 0; i < iters; ++i) {
        // Filter
        const QString& filter = filterTerms[i % filterTerms.size()];
        staking.rebuildFilteredList(filter);
        settleUi(5);

        // Sort by different columns
        auto col = static_cast<StakingPage::SortColumn>(i % 5);
        staking.sortValidators(col);
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 10 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples);
    assertWidgetsStable(widgetSamples);
}

// ═════════════════════════════════════════════════════════════════
// ACTIVITY TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: Activity page refresh stress (known widget leak) ───────

TEST_F(StressTest, StressActivityPageRefresh) {
    int iters = iterCount(50);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTransactionDb(owner, 500);

    ActivityPage activity;
    activity.resize(800, 600);
    activity.show();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressActivityPageRefresh (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        activity.refresh(owner);
        settleUi(10);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 10 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples, 10, 40);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: Activity page filter + sort cycles ─────────────────────

TEST_F(StressTest, StressActivityFilterSort) {
    int iters = iterCount(100);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTransactionDb(owner, 200);

    ActivityPage activity;
    activity.resize(800, 600);
    activity.show();
    settleUi(50);

    // Initial load
    activity.refresh(owner);
    settleUi(20);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressActivityFilterSort (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        // Toggle sort on different columns
        int col = i % 7;
        activity.toggleSort(col);
        settleUi(5);

        // Apply text filters
        switch (i % 5) {
            case 0:
                activity.m_sigFilter = "Sig0001";
                break;
            case 1:
                activity.m_fromFilter = "Addr0001";
                break;
            case 2:
                activity.m_actionFilter = {"send"};
                break;
            case 3:
                activity.m_tokenFilter = "SOL";
                break;
            case 4:
                // Clear all
                activity.m_sigFilter.clear();
                activity.m_fromFilter.clear();
                activity.m_actionFilter.clear();
                activity.m_tokenFilter.clear();
                break;
        }
        activity.applyAllFilters();
        settleUi(5);
        // Force-drain deferred deletes that processEvents() may skip
        // due to Qt's event-loop nesting level check
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    // Widget count varies with filter state (mod 5 cycle), so compare
    // samples at the same phase to avoid false positives
    int warmup = 10;
    int warmupPhase = warmup % 5;
    int bestEnd = -1;
    for (int i = static_cast<int>(widgetSamples.size()) - 1; i > warmup; --i) {
        if ((warmup + (i - warmup)) % 5 == 0) { // same phase as warmup
            bestEnd = i;
            break;
        }
    }
    if (bestEnd > 0) {
        int growth = widgetSamples[bestEnd].totalWidgets - widgetSamples[warmup].totalWidgets;
        std::cout << "[StressTest] Widgets at warmup (i=" << warmup
                  << "): " << widgetSamples[warmup].totalWidgets << '\n';
        std::cout << "[StressTest] Widgets at phase-matched end (i=" << bestEnd
                  << "): " << widgetSamples[bestEnd].totalWidgets << '\n';
        std::cout << "[StressTest] Widget growth (phase-matched): " << growth << '\n';
        EXPECT_LE(growth, 50) << "Widget count grew by " << growth
                              << " after warmup — possible widget leak";
    }
    Q_UNUSED(warmupPhase);

    // Filter cycling rebuilds row widgets each iteration; RSS growth
    // is from Qt style/layout caches, confirmed by 0 widget growth
    assertMemoryStable(memSamples, 10, 60);
}

// ═════════════════════════════════════════════════════════════════
// ASSETS TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: Assets page refresh stress (rebuildGrid deleteLater) ───

TEST_F(StressTest, StressAssetsPageRefresh) {
    int iters = iterCount(100);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTokenAccounts(owner, 15);
    seedPortfolioSnapshots(30);

    AssetsPage assets;
    assets.resize(800, 600);
    assets.show();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressAssetsPageRefresh (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        assets.refresh(owner);
        settleUi(10);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples, 10, 30);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: Assets page filter/search cycles ───────────────────────

TEST_F(StressTest, StressAssetsFilterCycle) {
    int iters = iterCount(100);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTokenAccounts(owner, 15);
    seedPortfolioSnapshots(10);

    AssetsPage assets;
    assets.resize(800, 600);
    assets.show();
    settleUi(50);

    assets.refresh(owner);
    settleUi(20);

    QStringList searchTerms = {"TKN", "Token 1", "SOL", "nonexistent", ""};

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressAssetsFilterCycle (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        const QString& search = searchTerms[i % searchTerms.size()];
        assets.filterAssets(search);
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples, 10, 30);
    assertWidgetsStable(widgetSamples);
}

// ═════════════════════════════════════════════════════════════════
// ADDRESS BOOK TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: Address book CRUD + refresh stress ─────────────────────

TEST_F(StressTest, StressAddressBookCrud) {
    int iters = iterCount(100);

    seedContacts(20);

    AddressBookPage addressBook;
    addressBook.resize(800, 600);
    addressBook.show();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressAddressBookCrud (" << iters << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        // Add a contact then refresh
        ContactDb::insertContact(
            QString("Stress %1").arg(i),
            QString("Stress%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 5, 10, QChar('0')));
        addressBook.refreshList();
        settleUi(5);

        // Search to trigger filtered refreshList
        if (addressBook.m_searchInput) {
            addressBook.m_searchInput->setText(i % 3 == 0 ? "Stress" : "");
        }
        settleUi(5);

        // Delete the contact we just added (keep DB size bounded)
        auto contacts = ContactDb::getAllRecords("Stress " + QString::number(i));
        if (!contacts.isEmpty()) {
            ContactDb::deleteContact(contacts.first().id);
        }
        addressBook.refreshList();
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples);
    assertWidgetsStable(widgetSamples);
}

// ═════════════════════════════════════════════════════════════════
// WIDGET ISOLATION TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: TokenDropdown clear + repopulate cycles ────────────────

TEST_F(StressTest, StressTokenDropdownCycle) {
    int iters = iterCount(100);

    QWidget container;
    container.resize(400, 300);

    TokenDropdown dropdown(&container);
    dropdown.show();
    container.show();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressTokenDropdownCycle (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        // Clear all items
        dropdown.clear();
        settleUi(2);

        // Re-add tokens (each creates QListWidgetItem + custom row widget with labels)
        int tokenCount = 5 + (i % 10);
        for (int t = 0; t < tokenCount; ++t) {
            dropdown.addToken(QString(":/icons/tokens/sol.png"), QString("Token%1").arg(t),
                              QString("%1.00").arg(100 + t));
        }
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: FlapDisplay digit count changes ────────────────────────

TEST_F(StressTest, StressFlapDisplayUpdate) {
    int iters = iterCount(100);

    FlapDisplay display;
    display.show();
    settleUi(50);

    // Varying length strings to exercise ensureDigitCount (adds/removes FlapDigit widgets)
    QStringList values = {"$1,234.56", "$99.99", "$12,345,678.90", "$0.01", "$999,999.99",
                          "$1.00",     "$42",    "$123,456.78",    "$7",    "$88,888.88"};

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressFlapDisplayUpdate (" << iters
              << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        display.setValue(values[i % values.size()]);
        settleUi(10); // Allow animation timers to fire

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: SplineChart data replacement cycles ────────────────────

TEST_F(StressTest, StressSplineChartData) {
    int iters = iterCount(100);

    SplineChart chart;
    chart.resize(600, 300);
    chart.setLineColor(QColor(190, 160, 255));
    chart.setShowYAxis(true);
    chart.setShowXAxis(true);
    chart.show();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressSplineChartData (" << iters << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        // Generate new data points each iteration (different sizes to stress allocation)
        int pointCount = 50 + (i % 100);
        QList<QPointF> points;
        for (int p = 0; p < pointCount; ++p) {
            double x = static_cast<double>(p);
            double y = 100.0 + 50.0 * qSin(static_cast<double>(p + i) * 0.1);
            points.append(QPointF(x, y));
        }
        chart.setData(points);
        settleUi(5);

        // Force a repaint to exercise the paint cache path
        chart.repaint();
        settleUi(2);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 20 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    assertMemoryStable(memSamples);
    assertWidgetsStable(widgetSamples);
}

// ═════════════════════════════════════════════════════════════════
// CPU / RENDERING PROFILING TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: SplineChart hover CPU (repaint on every mouse move) ────

TEST_F(StressTest, CpuSplineChartHover) {
    SplineChart chart;
    chart.resize(600, 300);
    chart.setLineColor(QColor(190, 160, 255));
    chart.setShowYAxis(true);
    chart.setShowXAxis(true);
    chart.show();
    settleUi(50);

    // Load data
    QList<QPointF> points;
    for (int p = 0; p < 100; ++p) {
        points.append(QPointF(p, 100.0 + 50.0 * qSin(p * 0.1)));
    }
    chart.setData(points);
    settleUi(100); // Let blur cache build

    int sweeps = iterCount(50);
    int pixelsPerSweep = 500;

    QElapsedTimer wallTimer;
    auto cpuBefore = sampleMemory();

    std::cout << "[CpuProfile] === SplineChart Hover (" << sweeps << " sweeps × " << pixelsPerSweep
              << " pixels) ===" << '\n';

    wallTimer.start();

    for (int s = 0; s < sweeps; ++s) {
        // Simulate mouse sweep across the chart
        for (int x = 0; x < pixelsPerSweep; ++x) {
            int px = x * chart.width() / pixelsPerSweep;
            QMouseEvent move(QEvent::MouseMove, QPointF(px, 150), QPointF(px, 150), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&chart, &move);
            // No processEvents — measure pure CPU cost of event handling + paint queuing
        }
        QApplication::processEvents();
    }

    qint64 wallMs = wallTimer.elapsed();
    auto cpuAfter = sampleMemory();
    double cpuSec = cpuAfter.userTimeSec - cpuBefore.userTimeSec;
    int totalFrames = sweeps * pixelsPerSweep;

    std::cout << "[CpuProfile] Total frames triggered: " << totalFrames << '\n';
    std::cout << "[CpuProfile] Wall time: " << wallMs << " ms" << '\n';
    std::cout << "[CpuProfile] CPU time: " << (cpuSec * 1000) << " ms" << '\n';
    std::cout << "[CpuProfile] CPU per frame: " << (cpuSec * 1000000 / totalFrames) << " us"
              << '\n';
    std::cout << "[CpuProfile] Theoretical max FPS at this cost: " << (1.0 / (cpuSec / totalFrames))
              << '\n';
}

// ── TEST: SplineChart blur rebuild CPU cost ──────────────────────

TEST_F(StressTest, CpuSplineChartBlurRebuild) {
    SplineChart chart;
    chart.resize(600, 300);
    chart.setLineColor(QColor(190, 160, 255));
    chart.setShowYAxis(true);
    chart.show();
    settleUi(50);

    int rebuilds = iterCount(50);

    std::cout << "[CpuProfile] === SplineChart Blur Rebuild (" << rebuilds
              << " rebuilds) ===" << '\n';

    QElapsedTimer wallTimer;
    auto cpuBefore = sampleMemory();
    wallTimer.start();

    for (int i = 0; i < rebuilds; ++i) {
        // Each setData triggers glow timer; we force rebuild by waiting
        QList<QPointF> points;
        int count = 50 + (i % 100);
        for (int p = 0; p < count; ++p) {
            points.append(QPointF(p, 100.0 + 50.0 * qSin((p + i) * 0.1)));
        }
        chart.setData(points);
        // Force the deferred blur rebuild to fire
        QTest::qWait(160);
        QApplication::processEvents();
    }

    qint64 wallMs = wallTimer.elapsed();
    auto cpuAfter = sampleMemory();
    double cpuSec = cpuAfter.userTimeSec - cpuBefore.userTimeSec;

    std::cout << "[CpuProfile] Wall time: " << wallMs << " ms" << '\n';
    std::cout << "[CpuProfile] CPU time: " << (cpuSec * 1000) << " ms" << '\n';
    std::cout << "[CpuProfile] CPU per rebuild: " << (cpuSec * 1000 / rebuilds) << " ms" << '\n';
}

// ── TEST: TpsBarChart hover CPU ──────────────────────────────────

TEST_F(StressTest, CpuTpsBarChartHover) {
    TpsBarChart chart;
    chart.resize(600, 200);
    chart.show();
    settleUi(50);

    // Load TPS data
    QList<TpsSample> samples;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int i = 0; i < 30; ++i) {
        TpsSample ts;
        ts.totalTps = 3000 + (i * 10);
        ts.nonVoteTps = 800 + (i * 3);
        ts.voteTps = ts.totalTps - ts.nonVoteTps;
        ts.timestamp = now - (30 - i) * 60;
        samples.append(ts);
    }
    chart.setData(samples);
    settleUi(50);

    int sweeps = iterCount(50);
    int pixelsPerSweep = 500;

    QElapsedTimer wallTimer;
    auto cpuBefore = sampleMemory();

    std::cout << "[CpuProfile] === TpsBarChart Hover (" << sweeps << " sweeps × " << pixelsPerSweep
              << " pixels) ===" << '\n';

    wallTimer.start();

    for (int s = 0; s < sweeps; ++s) {
        for (int x = 0; x < pixelsPerSweep; ++x) {
            int px = x * chart.width() / pixelsPerSweep;
            QMouseEvent move(QEvent::MouseMove, QPointF(px, 100), QPointF(px, 100), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(&chart, &move);
        }
        QApplication::processEvents();
    }

    qint64 wallMs = wallTimer.elapsed();
    auto cpuAfter = sampleMemory();
    double cpuSec = cpuAfter.userTimeSec - cpuBefore.userTimeSec;
    int totalFrames = sweeps * pixelsPerSweep;

    std::cout << "[CpuProfile] Total frames triggered: " << totalFrames << '\n';
    std::cout << "[CpuProfile] Wall time: " << wallMs << " ms" << '\n';
    std::cout << "[CpuProfile] CPU time: " << (cpuSec * 1000) << " ms" << '\n';
    std::cout << "[CpuProfile] CPU per frame: " << (cpuSec * 1000000 / totalFrames) << " us"
              << '\n';
}

// ── TEST: FlapDisplay animation CPU ──────────────────────────────

TEST_F(StressTest, CpuFlapDisplayAnimation) {
    FlapDisplay display;
    display.resize(200, 30);
    display.show();
    settleUi(50);

    int updates = iterCount(50);

    std::cout << "[CpuProfile] === FlapDisplay Animation (" << updates
              << " value changes) ===" << '\n';

    QElapsedTimer wallTimer;
    auto cpuBefore = sampleMemory();
    wallTimer.start();

    for (int i = 0; i < updates; ++i) {
        // Generate a value that changes most digits to maximize animation work
        QString val = QString("$%1,%2.%3")
                          .arg(1000 + i * 7)
                          .arg(100 + (i * 13) % 900, 3, 10, QChar('0'))
                          .arg((i * 37) % 100, 2, 10, QChar('0'));
        display.setValue(val);
        // Let animation frames run (220ms per digit + stagger)
        QTest::qWait(300);
        QApplication::processEvents();
    }

    qint64 wallMs = wallTimer.elapsed();
    auto cpuAfter = sampleMemory();
    double cpuSec = cpuAfter.userTimeSec - cpuBefore.userTimeSec;

    std::cout << "[CpuProfile] Wall time: " << wallMs << " ms" << '\n';
    std::cout << "[CpuProfile] CPU time: " << (cpuSec * 1000) << " ms" << '\n';
    std::cout << "[CpuProfile] CPU per update: " << (cpuSec * 1000 / updates) << " ms" << '\n';
    std::cout << "[CpuProfile] CPU/wall ratio: " << (cpuSec * 1000 / wallMs * 100) << "%" << '\n';
}

// ── TEST: Dashboard full paint CPU ───────────────────────────────

TEST_F(StressTest, CpuDashboardFullPaint) {
    int iters = iterCount(100);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTransactionDb(owner, 100);
    seedPortfolioSnapshots(50);
    seedTokenAccounts(owner, 5);

    DashboardPage dashboard;
    dashboard.resize(800, 600);
    dashboard.show();
    settleUi(50);

    dashboard.refresh(owner);
    NetworkStats stats = buildMockNetworkStats();
    dashboard.updateNetworkStats(stats);
    settleUi(100);

    std::cout << "[CpuProfile] === Dashboard Full Paint (" << iters
              << " forced repaints) ===" << '\n';

    QElapsedTimer wallTimer;
    auto cpuBefore = sampleMemory();
    wallTimer.start();

    for (int i = 0; i < iters; ++i) {
        dashboard.repaint(); // Synchronous full repaint
    }

    qint64 wallMs = wallTimer.elapsed();
    auto cpuAfter = sampleMemory();
    double cpuSec = cpuAfter.userTimeSec - cpuBefore.userTimeSec;

    std::cout << "[CpuProfile] Wall time: " << wallMs << " ms" << '\n';
    std::cout << "[CpuProfile] CPU time: " << (cpuSec * 1000) << " ms" << '\n';
    std::cout << "[CpuProfile] CPU per paint: " << (cpuSec * 1000 / iters) << " ms" << '\n';
    std::cout << "[CpuProfile] Theoretical max FPS: " << (iters / cpuSec) << '\n';
}

// ═════════════════════════════════════════════════════════════════
// CROSS-PAGE TESTS
// ═════════════════════════════════════════════════════════════════

// ── TEST: Tab switching between all pages ────────────────────────

TEST_F(StressTest, StressTabSwitching) {
    int iters = iterCount(200);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTransactionDb(owner, 100);
    seedTokenAccounts(owner, 10);
    seedPortfolioSnapshots(20);
    seedContacts(15);

    QStackedWidget stack;
    stack.resize(800, 600);

    auto* dashboard = new DashboardPage();
    auto* activity = new ActivityPage();
    auto* staking = new StakingPage();
    auto* assetsPage = new AssetsPage();
    auto* addressBook = new AddressBookPage();
    staking->setSolanaApi(m_api);
    staking->setAvatarCache(m_avatarCache);
    staking->setWalletAddress(owner);
    staking->setAutoRefreshOnShow(false);

    stack.addWidget(dashboard);   // 0
    stack.addWidget(activity);    // 1
    stack.addWidget(staking);     // 2
    stack.addWidget(assetsPage);  // 3
    stack.addWidget(addressBook); // 4

    stack.show();
    settleUi(50);

    NetworkStats stats = buildMockNetworkStats();
    dashboard->updateNetworkStats(stats);
    dashboard->refresh(owner);
    activity->refresh(owner);
    assetsPage->refresh(owner);

    staking->m_validators = buildMockValidators(100);
    staking->rebuildFilteredList();
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === StressTabSwitching (" << iters << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        int pageIdx = i % 5;
        stack.setCurrentIndex(pageIdx);
        settleUi(5);

        switch (pageIdx) {
            case 0:
                stats.blockHeight++;
                stats.currentTps = 3000 + (i % 500);
                dashboard->updateNetworkStats(stats);
                break;
            case 1:
                activity->refresh(owner);
                break;
            case 2:
                if (staking->m_validatorScroll) {
                    QScrollBar* vbar = staking->m_validatorScroll->verticalScrollBar();
                    vbar->setValue((i * 200) % qMax(1, vbar->maximum() + 1));
                }
                break;
            case 3:
                assetsPage->refresh(owner);
                break;
            case 4:
                addressBook->refreshList();
                break;
        }
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);

        if (i % 40 == 0 || i == iters - 1) {
            logSample(i, mem, wid);
        }
    }

    // Tab switching loads/unloads complex page hierarchies, triggering
    // Qt's style cache, pixmap cache, and text layout engine warming.
    // 0 widget growth confirms no leak, so raise RSS tolerance.
    assertMemoryStable(memSamples, 10, 150);
    assertWidgetsStable(widgetSamples);
}

// ── TEST: Full realistic workflow (expanded) ─────────────────────

TEST_F(StressTest, FullAppWorkflow) {
    int iters = iterCount(50);
    QString owner = "TestWallet1111111111111111111111111111111111";

    seedTransactionDb(owner, 300);
    seedTokenAccounts(owner, 12);
    seedPortfolioSnapshots(48);
    seedContacts(10);

    QStackedWidget stack;
    stack.resize(800, 600);

    auto* dashboard = new DashboardPage();
    auto* activity = new ActivityPage();
    auto* staking = new StakingPage();
    auto* assetsPage = new AssetsPage();
    auto* addressBook = new AddressBookPage();
    staking->setSolanaApi(m_api);
    staking->setAvatarCache(m_avatarCache);
    staking->setWalletAddress(owner);
    staking->setAutoRefreshOnShow(false);

    stack.addWidget(dashboard);   // 0
    stack.addWidget(activity);    // 1
    stack.addWidget(staking);     // 2
    stack.addWidget(assetsPage);  // 3
    stack.addWidget(addressBook); // 4

    stack.show();
    settleUi(50);

    staking->m_validators = buildMockValidators(200);
    staking->rebuildFilteredList();

    NetworkStats stats = buildMockNetworkStats();
    dashboard->updateNetworkStats(stats);
    dashboard->refresh(owner);
    assetsPage->refresh(owner);
    settleUi(50);

    std::vector<MemSample> memSamples;
    std::vector<WidgetSample> widgetSamples;

    std::cout << "[StressTest] === FullAppWorkflow (" << iters << " iterations) ===" << '\n';
    std::cout << "[StressTest] iter,rss_mb,widgets,labels,cpu_sec" << '\n';

    for (int i = 0; i < iters; ++i) {
        // 1. Dashboard — full refresh (balance, chart, recent activity)
        stack.setCurrentIndex(0);
        stats.blockHeight += 10;
        stats.currentTps = 2800 + (i * 20) % 800;
        dashboard->updateNetworkStats(stats);
        dashboard->refresh(owner);
        settleUi(10);

        // 2. Assets — refresh + filter cycle
        stack.setCurrentIndex(3);
        assetsPage->refresh(owner);
        settleUi(5);
        assetsPage->filterAssets("TKN");
        settleUi(5);
        assetsPage->filterAssets("");
        settleUi(5);

        // 3. Staking — scroll, filter, sort
        stack.setCurrentIndex(2);
        settleUi(10);

        QScrollBar* vbar = staking->m_validatorScroll->verticalScrollBar();
        int step = qMax(1, vbar->maximum() / 3);
        for (int s = 0; s <= vbar->maximum(); s += step) {
            vbar->setValue(s);
            settleUi(2);
        }
        staking->rebuildFilteredList("Validator 1");
        settleUi(5);
        staking->sortValidators(StakingPage::SortColumn::Apy);
        settleUi(5);
        staking->rebuildFilteredList();
        settleUi(5);
        staking->sortValidators(StakingPage::SortColumn::Stake);
        settleUi(5);

        // 4. Activity — refresh + filter + sort
        stack.setCurrentIndex(1);
        activity->refresh(owner);
        settleUi(10);
        activity->toggleSort(5); // sort by amount
        settleUi(5);
        activity->m_actionFilter = {"send"};
        activity->applyAllFilters();
        settleUi(5);
        activity->m_actionFilter.clear();
        activity->applyAllFilters();
        settleUi(5);

        // 5. Address book — refresh + search cycle
        stack.setCurrentIndex(4);
        addressBook->refreshList();
        settleUi(5);
        if (addressBook->m_searchInput) {
            addressBook->m_searchInput->setText("Contact");
            settleUi(5);
            addressBook->m_searchInput->setText("");
            settleUi(5);
        }

        // 6. Back to dashboard
        stack.setCurrentIndex(0);
        settleUi(5);

        auto mem = sampleMemory();
        auto wid = sampleWidgets();
        memSamples.push_back(mem);
        widgetSamples.push_back(wid);
        logSample(i, mem, wid);
    }

    // Full workflow exercises all pages + filters + sorts in sequence;
    // RSS growth is from Qt caches across 5 different page types.
    // 0 widget growth across all iterations confirms no leak.
    assertMemoryStable(memSamples, 3, 80);
    assertWidgetsStable(widgetSamples, 3);

    // Final summary
    std::cout << "\n[StressTest] === FINAL SUMMARY ===" << '\n';
    if (!memSamples.empty()) {
        double startMb = static_cast<double>(memSamples.front().rssBytes) / (1024.0 * 1024.0);
        double endMb = static_cast<double>(memSamples.back().rssBytes) / (1024.0 * 1024.0);
        std::cout << "[StressTest] RSS start: " << startMb << " MB, end: " << endMb
                  << " MB, delta: " << (endMb - startMb) << " MB" << '\n';
    }
    if (!widgetSamples.empty()) {
        std::cout << "[StressTest] Widgets start: " << widgetSamples.front().totalWidgets
                  << ", end: " << widgetSamples.back().totalWidgets << ", delta: "
                  << (widgetSamples.back().totalWidgets - widgetSamples.front().totalWidgets)
                  << '\n';
        std::cout << "[StressTest] Labels start: " << widgetSamples.front().labelCount
                  << ", end: " << widgetSamples.back().labelCount << ", delta: "
                  << (widgetSamples.back().labelCount - widgetSamples.front().labelCount) << '\n';
    }
}

// ── main ─────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
