#include "TestMigrationUtils.h"
#include "db/Database.h"
#include "db/TransactionDb.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <gtest/gtest.h>

// Qt requires a QCoreApplication for QSqlDatabase
static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    QCoreApplication::setApplicationName("Cinder");
    QCoreApplication::setOrganizationName("Cinder");
    QStandardPaths::setTestModeEnabled(true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

static void closeDefaultConnection() {
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        QSqlDatabase::database().close();
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
}

// ── Test fixture: fresh DB per test ──────────────────────

class DatabaseTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        // Close any leftover connection
        closeDefaultConnection();

        ASSERT_TRUE(m_tempDir.isValid());
        QString dbPath = m_tempDir.path() + "/test_wallet.db";

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open());

        QSqlQuery pragma(db);
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA foreign_keys=ON");

        QString migrationDir = m_tempDir.path() + "/migrations";
        TestMigrationUtils::copyAllMigrationsToDir(migrationDir);

        // Run migrations from temp dir
        ASSERT_TRUE(Database::migrate(migrationDir));
    }

    void TearDown() override { closeDefaultConnection(); }

    // ── Helpers ──

    static Activity makeActivity(const QString& type, const QString& from, const QString& to,
                                 const QString& token, double amount) {
        return {type, from, to, token, amount};
    }

    void insertSolTransfer() {
        QList<Activity> activities;
        activities.append(makeActivity("send", "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro",
                                       "D9gBpBQ4N2qwT194jYnNbALnnjysibUXZ4HXqEn1hHsZ", "SOL", 1.5));
        activities.append(makeActivity("send", "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro",
                                       "9xTvZb6y2A2iHk6SXubG9TLPF9zzqeq8QBVida91BjhA", "SOL", 2.0));

        ASSERT_TRUE(TransactionDb::insertTransaction("sig_sol_transfer_001", 399000440, 1770599885,
                                                     R"({"mock": "sol transfer raw json"})", 5000,
                                                     false, activities));
    }

    void insertUsdcReceive() {
        QList<Activity> activities;
        activities.append(makeActivity("receive", "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7",
                                       "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro",
                                       "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", 150.50));

        ASSERT_TRUE(TransactionDb::insertTransaction("sig_usdc_receive_001", 398974668, 1770589765,
                                                     R"({"mock": "usdc receive raw json"})", 5000,
                                                     false, activities));
    }

    // Insert a swap activity
    void insertSwap() {
        QList<Activity> activities;
        activities.append(makeActivity("swap", "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro",
                                       "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro", "SOL",
                                       10.0));

        ASSERT_TRUE(TransactionDb::insertTransaction("sig_swap_001", 399100000, 1770650000,
                                                     R"({"mock": "swap json"})", 5000, false,
                                                     activities));
    }

    // Insert N numbered transactions for pagination tests
    void insertMany(int count) {
        for (int i = 0; i < count; ++i) {
            QString sig = QString("sig_bulk_%1").arg(i, 4, 10, QChar('0'));
            qint64 blockTime = 1770000000 + i * 100;
            int slot = 399000000 + i;
            QString type = (i % 3 == 0) ? "send" : ((i % 3 == 1) ? "receive" : "swap");
            QString token = (i % 2 == 0) ? "SOL" : USDC_MINT;
            double amount = 1.0 + i;

            QList<Activity> activities;
            activities.append(makeActivity(
                type, WALLET, "D9gBpBQ4N2qwT194jYnNbALnnjysibUXZ4HXqEn1hHsZ", token, amount));

            ASSERT_TRUE(TransactionDb::insertTransaction(
                sig, slot, blockTime, R"({"mock": "bulk"})", 5000, false, activities));
        }
    }

    const QString WALLET = "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro";
    const QString USDC_MINT = "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v";
};

class RawMigrationDatabaseTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        closeDefaultConnection();

        ASSERT_TRUE(m_tempDir.isValid());
        const QString dbPath = m_tempDir.path() + "/test_wallet.db";

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open());

        QSqlQuery pragma(db);
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA foreign_keys=ON");
    }

    void TearDown() override { closeDefaultConnection(); }
};

class StartupDatabaseOpenTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        closeDefaultConnection();
        ASSERT_TRUE(m_tempDir.isValid());
        QDir appDataDir(m_tempDir.path());
        if (appDataDir.exists()) {
            appDataDir.removeRecursively();
        }
        QDir().mkpath(m_tempDir.path());
    }

    void TearDown() override {
        closeDefaultConnection();
        QDir appDataDir(m_tempDir.path());
        if (appDataDir.exists()) {
            appDataDir.removeRecursively();
        }
    }

    QString walletDbPath() const { return m_tempDir.path() + "/wallet.db"; }
};

// ── Migration tests ───────────────────────────────────────

TEST_F(DatabaseTest, MigrationSetsVersion) {
    EXPECT_EQ(Database::currentVersion(), TestMigrationUtils::latestMigrationVersion());
}

TEST_F(DatabaseTest, MigrationIsIdempotent) {
    // Running migrations again should be a no-op
    QString migrationDir = m_tempDir.path() + "/migrations";
    int vBefore = Database::currentVersion();
    EXPECT_TRUE(Database::migrate(migrationDir));
    EXPECT_EQ(Database::currentVersion(), vBefore);
}

TEST_F(RawMigrationDatabaseTest, FailedMigrationRollsBackAllStatements) {
    QTemporaryDir badDir;
    ASSERT_TRUE(badDir.isValid());

    QFile badMigration(badDir.path() + "/V001_partial_failure.sql");
    ASSERT_TRUE(badMigration.open(QIODevice::WriteOnly | QIODevice::Text));
    badMigration.write(R"SQL(
CREATE TABLE partial_table (
    id INTEGER PRIMARY KEY
);
INSERT INTO missing_table (id) VALUES (1);
)SQL");
    badMigration.close();

    EXPECT_FALSE(Database::migrate(badDir.path()));
    EXPECT_EQ(Database::currentVersion(), 0);

    QSqlQuery q(QSqlDatabase::database());
    ASSERT_TRUE(
        q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='partial_table'"));
    EXPECT_FALSE(q.next());
}

TEST_F(RawMigrationDatabaseTest, UpgradeFromOlderSchemaPreservesData) {
    QTemporaryDir olderDir;
    ASSERT_TRUE(olderDir.isValid());
    const QString olderMigrationDir = olderDir.path() + "/migrations";
    QDir().mkpath(olderMigrationDir);

    for (const QString& name : {"V001_initial_schema.sql", "V002_stake_accounts.sql"}) {
        QFile src(":/migrations/" + name);
        ASSERT_TRUE(src.open(QIODevice::ReadOnly));
        QFile dst(olderMigrationDir + "/" + name);
        ASSERT_TRUE(dst.open(QIODevice::WriteOnly));
        dst.write(src.readAll());
    }

    ASSERT_TRUE(Database::migrate(olderMigrationDir));
    EXPECT_EQ(Database::currentVersion(), 2);

    QSqlQuery insertStake(QSqlDatabase::database());
    insertStake.prepare(R"SQL(
        INSERT INTO stake_accounts
            (address, wallet_address, lamports, vote_account, stake, state,
             activation_epoch, deactivation_epoch)
        VALUES
            (:address, :wallet, :lamports, :vote, :stake, :state, :activation, :deactivation)
    )SQL");
    insertStake.bindValue(":address", "stake-addr-1");
    insertStake.bindValue(":wallet", "wallet-addr-1");
    insertStake.bindValue(":lamports", 123456LL);
    insertStake.bindValue(":vote", "vote-addr-1");
    insertStake.bindValue(":stake", 120000LL);
    insertStake.bindValue(":state", "Active");
    insertStake.bindValue(":activation", 100);
    insertStake.bindValue(":deactivation", 0);
    ASSERT_TRUE(insertStake.exec()) << insertStake.lastError().text().toStdString();

    EXPECT_TRUE(Database::migrate(":/migrations"));
    EXPECT_EQ(Database::currentVersion(), TestMigrationUtils::latestMigrationVersion());

    QSqlQuery verifyStake(QSqlDatabase::database());
    ASSERT_TRUE(verifyStake.exec(R"SQL(
        SELECT lamports, stake, state, total_rewards_lamports
        FROM stake_accounts
        WHERE address = 'stake-addr-1' AND wallet_address = 'wallet-addr-1'
    )SQL"));
    ASSERT_TRUE(verifyStake.next());
    EXPECT_EQ(verifyStake.value(0).toLongLong(), 123456LL);
    EXPECT_EQ(verifyStake.value(1).toLongLong(), 120000LL);
    EXPECT_EQ(verifyStake.value(2).toString(), QString("Active"));
    EXPECT_EQ(verifyStake.value(3).toLongLong(), 0LL);

    QSqlQuery rewardTable(QSqlDatabase::database());
    ASSERT_TRUE(rewardTable.exec(R"SQL(
        SELECT name FROM sqlite_master WHERE type='table' AND name='stake_account_rewards'
    )SQL"));
    ASSERT_TRUE(rewardTable.next());
}

TEST_F(StartupDatabaseOpenTest, OpenUpgradesExistingOnDiskDatabaseAndPreservesData) {
    QTemporaryDir olderDir;
    ASSERT_TRUE(olderDir.isValid());
    const QString olderMigrationDir = olderDir.path() + "/migrations";
    QDir().mkpath(olderMigrationDir);

    for (const QString& name : {"V001_initial_schema.sql", "V002_stake_accounts.sql"}) {
        QFile src(":/migrations/" + name);
        ASSERT_TRUE(src.open(QIODevice::ReadOnly));
        QFile dst(olderMigrationDir + "/" + name);
        ASSERT_TRUE(dst.open(QIODevice::WriteOnly));
        dst.write(src.readAll());
    }

    const QString walletDbPath = this->walletDbPath();
    ASSERT_FALSE(walletDbPath.isEmpty());
    ASSERT_TRUE(QDir().mkpath(QFileInfo(walletDbPath).absolutePath()));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(walletDbPath);
        ASSERT_TRUE(db.open());

        QSqlQuery pragma(db);
        ASSERT_TRUE(pragma.exec("PRAGMA journal_mode=WAL"));
        ASSERT_TRUE(pragma.exec("PRAGMA foreign_keys=ON"));

        ASSERT_TRUE(Database::migrate(olderMigrationDir));
        EXPECT_EQ(Database::currentVersion(), 2);

        QSqlQuery insertStake(db);
        insertStake.prepare(R"SQL(
            INSERT INTO stake_accounts
                (address, wallet_address, lamports, vote_account, stake, state,
                 activation_epoch, deactivation_epoch)
            VALUES
                (:address, :wallet, :lamports, :vote, :stake, :state, :activation, :deactivation)
        )SQL");
        insertStake.bindValue(":address", "startup-stake-addr-1");
        insertStake.bindValue(":wallet", "startup-wallet-addr-1");
        insertStake.bindValue(":lamports", 654321LL);
        insertStake.bindValue(":vote", "startup-vote-addr-1");
        insertStake.bindValue(":stake", 650000LL);
        insertStake.bindValue(":state", "Active");
        insertStake.bindValue(":activation", 200);
        insertStake.bindValue(":deactivation", 0);
        ASSERT_TRUE(insertStake.exec()) << insertStake.lastError().text().toStdString();
    }

    closeDefaultConnection();

    ASSERT_TRUE(Database::open(walletDbPath, QStringLiteral(":/migrations")));
    EXPECT_EQ(Database::currentVersion(), TestMigrationUtils::latestMigrationVersion());

    QSqlQuery verifyStake(QSqlDatabase::database());
    ASSERT_TRUE(verifyStake.exec(R"SQL(
        SELECT lamports, stake, state, total_rewards_lamports
        FROM stake_accounts
        WHERE address = 'startup-stake-addr-1' AND wallet_address = 'startup-wallet-addr-1'
    )SQL"));
    ASSERT_TRUE(verifyStake.next());
    EXPECT_EQ(verifyStake.value(0).toLongLong(), 654321LL);
    EXPECT_EQ(verifyStake.value(1).toLongLong(), 650000LL);
    EXPECT_EQ(verifyStake.value(2).toString(), QString("Active"));
    EXPECT_EQ(verifyStake.value(3).toLongLong(), 0LL);

    QSqlQuery rewardTable(QSqlDatabase::database());
    ASSERT_TRUE(rewardTable.exec(R"SQL(
        SELECT name FROM sqlite_master WHERE type='table' AND name='stake_account_rewards'
    )SQL"));
    ASSERT_TRUE(rewardTable.next());
}

TEST_F(StartupDatabaseOpenTest, OpenFailsCleanlyAndRollsBackOnDiskWhenMigrationIsInvalid) {
    QTemporaryDir badDir;
    ASSERT_TRUE(badDir.isValid());

    const QString badMigrationPath = badDir.path() + "/V001_partial_failure.sql";
    QFile badMigration(badMigrationPath);
    ASSERT_TRUE(badMigration.open(QIODevice::WriteOnly | QIODevice::Text));
    badMigration.write(R"SQL(
CREATE TABLE partial_table (
    id INTEGER PRIMARY KEY
);
INSERT INTO missing_table (id) VALUES (1);
)SQL");
    badMigration.close();

    const QString walletDbPath = this->walletDbPath();
    ASSERT_FALSE(walletDbPath.isEmpty());
    ASSERT_TRUE(QDir().mkpath(QFileInfo(walletDbPath).absolutePath()));

    ASSERT_FALSE(Database::open(walletDbPath, badDir.path()));
    EXPECT_EQ(Database::currentVersion(), 0);

    QSqlQuery partialTable(QSqlDatabase::database());
    ASSERT_TRUE(partialTable.exec(R"SQL(
        SELECT name FROM sqlite_master WHERE type='table' AND name='partial_table'
    )SQL"));
    EXPECT_FALSE(partialTable.next());

    Database::close();

    QSqlDatabase verifyDb = QSqlDatabase::addDatabase("QSQLITE");
    verifyDb.setDatabaseName(walletDbPath);
    ASSERT_TRUE(verifyDb.open());

    QSqlQuery versionQuery(verifyDb);
    ASSERT_TRUE(versionQuery.exec("PRAGMA user_version"));
    ASSERT_TRUE(versionQuery.next());
    EXPECT_EQ(versionQuery.value(0).toInt(), 0);

    QSqlQuery verifyTable(verifyDb);
    ASSERT_TRUE(verifyTable.exec(R"SQL(
        SELECT name FROM sqlite_master WHERE type='table' AND name='partial_table'
    )SQL"));
    EXPECT_FALSE(verifyTable.next());
}

// ── Basic DB tests ─────────────────────────────────────────

TEST_F(DatabaseTest, InsertAndHasTransaction) {
    EXPECT_FALSE(TransactionDb::hasTransaction("sig_sol_transfer_001"));
    insertSolTransfer();
    EXPECT_TRUE(TransactionDb::hasTransaction("sig_sol_transfer_001"));
}

TEST_F(DatabaseTest, InsertCreatesActivities) {
    insertSolTransfer();

    auto rows = TransactionDb::getTransactionsRecords(WALLET);
    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows[0].activityType, QString("send"));
    EXPECT_EQ(rows[0].token, QString("SOL"));
    EXPECT_DOUBLE_EQ(rows[0].amount, 1.5);
}

TEST_F(DatabaseTest, MultipleTransactions) {
    insertSolTransfer();
    insertUsdcReceive();

    auto rows = TransactionDb::getTransactionsRecords(WALLET);
    EXPECT_EQ(rows.size(), 3); // 2 SOL sends + 1 USDC receive
}

TEST_F(DatabaseTest, FilterByToken) {
    insertSolTransfer();
    insertUsdcReceive();

    auto usdcRows = TransactionDb::getTransactionsRecords({}, USDC_MINT);
    ASSERT_EQ(usdcRows.size(), 1);
    EXPECT_DOUBLE_EQ(usdcRows[0].amount, 150.50);

    auto solRows = TransactionDb::getTransactionsRecords({}, "SOL");
    EXPECT_EQ(solRows.size(), 2);
}

TEST_F(DatabaseTest, FilterByActivityType) {
    insertSolTransfer();
    insertUsdcReceive();

    auto sends = TransactionDb::getTransactionsRecords({}, {}, "send");
    EXPECT_EQ(sends.size(), 2);

    auto receives = TransactionDb::getTransactionsRecords({}, {}, "receive");
    EXPECT_EQ(receives.size(), 1);
}

TEST_F(DatabaseTest, GetRawJson) {
    insertSolTransfer();

    QString raw = TransactionDb::getRawJson("sig_sol_transfer_001");
    EXPECT_TRUE(raw.contains("sol transfer raw json"));

    EXPECT_TRUE(TransactionDb::getRawJson("nonexistent").isEmpty());
}

TEST_F(DatabaseTest, GetLatestBlockTime) {
    EXPECT_EQ(TransactionDb::getLatestBlockTime(), 0);

    insertSolTransfer();
    EXPECT_EQ(TransactionDb::getLatestBlockTime(), 1770599885);

    insertUsdcReceive();
    EXPECT_EQ(TransactionDb::getLatestBlockTime(), 1770599885);
}

TEST_F(DatabaseTest, DuplicateInsertIgnored) {
    insertSolTransfer();
    insertSolTransfer();

    auto rows = TransactionDb::getTransactionsRecords(WALLET);
    EXPECT_EQ(rows.size(), 2);
}

TEST_F(DatabaseTest, OrderByBlockTimeDesc) {
    insertUsdcReceive();
    insertSolTransfer();

    auto rows = TransactionDb::getTransactionsRecords();
    ASSERT_GE(rows.size(), 2);
    EXPECT_GE(rows[0].blockTime, rows[1].blockTime);
}

TEST_F(DatabaseTest, LimitAndOffset) {
    insertSolTransfer();
    insertUsdcReceive();

    auto rows = TransactionDb::getTransactionsRecords({}, {}, {}, 1, 0);
    ASSERT_EQ(rows.size(), 1);

    auto rows2 = TransactionDb::getTransactionsRecords({}, {}, {}, 1, 1);
    ASSERT_EQ(rows2.size(), 1);
    EXPECT_NE(rows[0].id, rows2[0].id);
}

TEST_F(DatabaseTest, FailedTransaction) {
    QList<Activity> activities;
    activities.append(
        makeActivity("send", WALLET, "D9gBpBQ4N2qwT194jYnNbALnnjysibUXZ4HXqEn1hHsZ", "SOL", 1.0));

    ASSERT_TRUE(TransactionDb::insertTransaction("sig_failed_001", 400000000, 1770700000,
                                                 R"({"mock": "failed tx"})", 5000, true,
                                                 activities));

    auto rows = TransactionDb::getTransactionsRecords();
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].err, 1);
}

// ═══════════════════════════════════════════════════════════
// ── Filtered query tests (TransactionFilter) ──────────────
// ═══════════════════════════════════════════════════════════

TEST_F(DatabaseTest, FilteredEmptyFilterReturnsAll) {
    insertSolTransfer();
    insertUsdcReceive();
    insertSwap();

    TransactionFilter f; // all defaults = no filter
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f), 4);

    auto rows = TransactionDb::getFilteredTransactionsRecords(WALLET, f);
    EXPECT_EQ(rows.size(), 4);
}

TEST_F(DatabaseTest, FilteredByActionType) {
    insertSolTransfer();
    insertUsdcReceive();
    insertSwap();

    TransactionFilter f;
    f.actionTypes = {"send"};
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f), 2);

    auto rows = TransactionDb::getFilteredTransactionsRecords(WALLET, f);
    ASSERT_EQ(rows.size(), 2);
    for (const auto& r : rows) {
        EXPECT_EQ(r.activityType, QString("send"));
    }
}

TEST_F(DatabaseTest, FilteredByMultipleActionTypes) {
    insertSolTransfer();
    insertUsdcReceive();
    insertSwap();

    TransactionFilter f;
    f.actionTypes = {"send", "swap"};
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f), 3);

    auto rows = TransactionDb::getFilteredTransactionsRecords(WALLET, f);
    ASSERT_EQ(rows.size(), 3);
    for (const auto& r : rows) {
        QString type = r.activityType;
        EXPECT_TRUE(type == "send" || type == "swap") << "Unexpected type: " << type.toStdString();
    }
}

TEST_F(DatabaseTest, FilteredBySignatureSubstring) {
    insertSolTransfer();
    insertUsdcReceive();

    TransactionFilter f;
    f.signature = "usdc";
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f), 1);

    auto rows = TransactionDb::getFilteredTransactionsRecords({}, f);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_TRUE(rows[0].signature.contains("usdc"));
}

TEST_F(DatabaseTest, FilteredByTimeRange) {
    insertSolTransfer(); // blockTime = 1770599885
    insertUsdcReceive(); // blockTime = 1770589765
    insertSwap();        // blockTime = 1770650000

    // Only the swap (newest)
    TransactionFilter f;
    f.timeFrom = 1770600000;
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f), 1);

    // Only the USDC receive (oldest)
    TransactionFilter f2;
    f2.timeTo = 1770590000;
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f2), 1);

    // Time range that captures the SOL transfer
    TransactionFilter f3;
    f3.timeFrom = 1770595000;
    f3.timeTo = 1770600000;
    auto rows = TransactionDb::getFilteredTransactionsRecords(WALLET, f3);
    ASSERT_EQ(rows.size(), 2); // 2 activities from the SOL transfer
    for (const auto& r : rows) {
        qint64 bt = r.blockTime;
        EXPECT_GE(bt, 1770595000);
        EXPECT_LE(bt, 1770600000);
    }
}

TEST_F(DatabaseTest, FilteredByFromAddress) {
    insertSolTransfer();
    insertUsdcReceive();

    TransactionFilter f;
    f.fromAddress = "7Ci23i"; // substring of the USDC sender
    auto rows = TransactionDb::getFilteredTransactionsRecords({}, f);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_TRUE(rows[0].fromAddress.contains("7Ci23i"));
}

TEST_F(DatabaseTest, FilteredByToAddress) {
    insertSolTransfer();
    insertUsdcReceive();

    TransactionFilter f;
    f.toAddress = "D9gBpBQ4"; // one of the SOL transfer destinations
    auto rows = TransactionDb::getFilteredTransactionsRecords({}, f);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_DOUBLE_EQ(rows[0].amount, 1.5);
}

TEST_F(DatabaseTest, FilteredByAmountRange) {
    insertSolTransfer(); // amounts: 1.5, 2.0
    insertUsdcReceive(); // amount: 150.50
    insertSwap();        // amount: 10.0

    // Min only
    TransactionFilter f;
    f.amountMin = 5.0;
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f), 2);

    // Max only
    TransactionFilter f2;
    f2.amountMax = 2.0;
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f2), 2);

    // Range
    TransactionFilter f3;
    f3.amountMin = 1.0;
    f3.amountMax = 2.0;
    auto rows = TransactionDb::getFilteredTransactionsRecords({}, f3);
    ASSERT_EQ(rows.size(), 2);
    for (const auto& r : rows) {
        double amt = r.amount;
        EXPECT_GE(amt, 1.0);
        EXPECT_LE(amt, 2.0);
    }
}

TEST_F(DatabaseTest, FilteredByTokenSubstring) {
    insertSolTransfer();
    insertUsdcReceive();

    // "SOL" matches the token column directly
    TransactionFilter f;
    f.token = "SOL";
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f), 2);

    // Partial mint address
    TransactionFilter f2;
    f2.token = "EPjFWdd5";
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f2), 1);
}

TEST_F(DatabaseTest, FilteredCombined_ActionAndAmount) {
    insertSolTransfer(); // 2 sends: 1.5, 2.0
    insertUsdcReceive(); // 1 receive: 150.50
    insertSwap();        // 1 swap: 10.0

    TransactionFilter f;
    f.actionTypes = {"send"};
    f.amountMin = 1.8;
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f), 1);

    auto rows = TransactionDb::getFilteredTransactionsRecords({}, f);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].activityType, QString("send"));
    EXPECT_DOUBLE_EQ(rows[0].amount, 2.0);
}

TEST_F(DatabaseTest, FilteredCombined_TimeAndAction) {
    insertSolTransfer(); // blockTime = 1770599885, type = send
    insertUsdcReceive(); // blockTime = 1770589765, type = receive
    insertSwap();        // blockTime = 1770650000, type = swap

    TransactionFilter f;
    f.timeFrom = 1770600000; // only swap
    f.actionTypes = {"swap"};
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f), 1);

    // Time range + action type that yields no results
    TransactionFilter f2;
    f2.timeFrom = 1770600000;
    f2.actionTypes = {"send"};
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f2), 0);
}

TEST_F(DatabaseTest, FilteredNoMatchReturnsEmpty) {
    insertSolTransfer();
    insertUsdcReceive();

    TransactionFilter f;
    f.signature = "nonexistent_signature_xyz";
    EXPECT_EQ(TransactionDb::countFilteredTransactions({}, f), 0);

    auto rows = TransactionDb::getFilteredTransactionsRecords({}, f);
    EXPECT_TRUE(rows.isEmpty());
}

TEST_F(DatabaseTest, FilterIsEmptyCheck) {
    TransactionFilter f;
    EXPECT_TRUE(f.isEmpty());

    f.actionTypes = {"send"};
    EXPECT_FALSE(f.isEmpty());

    TransactionFilter f2;
    f2.amountMin = 0;
    EXPECT_FALSE(f2.isEmpty());

    TransactionFilter f3;
    f3.timeFrom = 1;
    EXPECT_FALSE(f3.isEmpty());
}

// ═══════════════════════════════════════════════════════════
// ── Pagination with filters (the original bugs) ───────────
// ═══════════════════════════════════════════════════════════

TEST_F(DatabaseTest, PaginationWithFilter_CountReflectsFilter) {
    // This test catches Bug #2: pagination count must change with filter
    insertMany(30);

    int unfilteredCount = TransactionDb::countTransactions(WALLET);
    EXPECT_EQ(unfilteredCount, 30);

    // Filter to "send" only (every 3rd row: i=0,3,6,...  → 10 rows)
    TransactionFilter f;
    f.actionTypes = {"send"};
    int filteredCount = TransactionDb::countFilteredTransactions(WALLET, f);
    EXPECT_EQ(filteredCount, 10);

    // The filtered count must be different from unfiltered
    EXPECT_LT(filteredCount, unfilteredCount);
}

TEST_F(DatabaseTest, PaginationWithFilter_Page2RespectsFilter) {
    // This test catches Bug #1: page 2 must only contain filtered rows
    insertMany(30);

    TransactionFilter f;
    f.actionTypes = {"send"}; // 10 matching rows

    // Page 1 (first 5)
    auto page1 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 5, 0);
    ASSERT_EQ(page1.size(), 5);
    for (const auto& r : page1) {
        EXPECT_EQ(r.activityType, QString("send")) << "Page 1 row was not 'send' — filter leaked";
    }

    // Page 2 (next 5)
    auto page2 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 5, 5);
    ASSERT_EQ(page2.size(), 5);
    for (const auto& r : page2) {
        EXPECT_EQ(r.activityType, QString("send"))
            << "Page 2 row was not 'send' — filter did not persist across pages";
    }

    // Page 3 (should be empty — only 10 send rows total)
    auto page3 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 5, 10);
    EXPECT_TRUE(page3.isEmpty()) << "Page 3 should be empty for 10 matching rows with pageSize=5";
}

TEST_F(DatabaseTest, PaginationWithFilter_OffsetBasedOnFilteredRows) {
    // Verify that offset counts filtered rows, not all rows
    insertMany(20);

    // "receive" = i%3==1 → i=1,4,7,10,13,16,19 → 7 rows
    TransactionFilter f;
    f.actionTypes = {"receive"};
    int count = TransactionDb::countFilteredTransactions(WALLET, f);
    EXPECT_EQ(count, 7);

    // Fetch all filtered rows in pages of 3
    auto page1 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 3, 0);
    auto page2 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 3, 3);
    auto page3 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 3, 6);

    EXPECT_EQ(page1.size(), 3);
    EXPECT_EQ(page2.size(), 3);
    EXPECT_EQ(page3.size(), 1);

    // All returned rows must be "receive"
    for (const auto& page : {page1, page2, page3}) {
        for (const auto& r : page) {
            EXPECT_EQ(r.activityType, QString("receive"));
        }
    }

    // No duplicates across pages
    QSet<int> ids;
    for (const auto& page : {page1, page2, page3}) {
        for (const auto& r : page) {
            int id = r.id;
            EXPECT_FALSE(ids.contains(id)) << "Duplicate row id " << id << " across pages";
            ids.insert(id);
        }
    }
    EXPECT_EQ(ids.size(), 7);
}

TEST_F(DatabaseTest, PaginationWithFilter_ClearFilterRestoresAll) {
    insertMany(15);

    // Filter to "swap" (i%3==2 → i=2,5,8,11,14 → 5 rows)
    TransactionFilter f;
    f.actionTypes = {"swap"};
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, f), 5);
    auto filteredRows = TransactionDb::getFilteredTransactionsRecords(WALLET, f);
    EXPECT_EQ(filteredRows.size(), 5);

    // Clear filter → should get all 15 rows back
    TransactionFilter empty;
    EXPECT_EQ(TransactionDb::countFilteredTransactions(WALLET, empty), 15);
    auto allRows = TransactionDb::getFilteredTransactionsRecords(WALLET, empty, 100, 0);
    EXPECT_EQ(allRows.size(), 15);
}

TEST_F(DatabaseTest, PaginationWithFilter_AmountAndPagination) {
    insertMany(20);
    // amounts = 1.0 + i → 1,2,...,20
    // amountMin=10 → i >= 9 → rows with amount 10,11,...,20 → 11 rows

    TransactionFilter f;
    f.amountMin = 10.0;

    int count = TransactionDb::countFilteredTransactions(WALLET, f);
    EXPECT_EQ(count, 11);

    auto page1 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 5, 0);
    auto page2 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 5, 5);
    auto page3 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 5, 10);

    EXPECT_EQ(page1.size(), 5);
    EXPECT_EQ(page2.size(), 5);
    EXPECT_EQ(page3.size(), 1);

    for (const auto& page : {page1, page2, page3}) {
        for (const auto& r : page) {
            EXPECT_GE(r.amount, 10.0)
                << "Row with amount < 10 leaked through filter on a later page";
        }
    }
}

TEST_F(DatabaseTest, PaginationWithFilter_CombinedFilterAcrossPages) {
    insertMany(30);
    // send + SOL: i%3==0 AND i%2==0 → i=0,6,12,18,24 → 5 rows

    TransactionFilter f;
    f.actionTypes = {"send"};
    f.token = "SOL";

    int count = TransactionDb::countFilteredTransactions(WALLET, f);
    EXPECT_EQ(count, 5);

    // Page through in groups of 2
    auto p1 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 2, 0);
    auto p2 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 2, 2);
    auto p3 = TransactionDb::getFilteredTransactionsRecords(WALLET, f, 2, 4);

    EXPECT_EQ(p1.size(), 2);
    EXPECT_EQ(p2.size(), 2);
    EXPECT_EQ(p3.size(), 1);

    for (const auto& page : {p1, p2, p3}) {
        for (const auto& r : page) {
            EXPECT_EQ(r.activityType, QString("send"));
            EXPECT_EQ(r.token, QString("SOL"));
        }
    }
}

TEST_F(DatabaseTest, FilteredOrderByBlockTimeDesc) {
    insertMany(10);

    TransactionFilter f;
    f.actionTypes = {"send"};
    auto rows = TransactionDb::getFilteredTransactionsRecords(WALLET, f);

    for (int i = 1; i < rows.size(); ++i) {
        EXPECT_GE(rows[i - 1].blockTime, rows[i].blockTime)
            << "Filtered results not in descending block_time order";
    }
}
