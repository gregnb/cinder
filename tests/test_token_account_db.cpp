#include "TestMigrationUtils.h"
#include "db/Database.h"
#include "db/TokenAccountDb.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── Fixture: fresh DB with consolidated schema ────────────

class TokenAccountDbTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
            QSqlDatabase::database().close();
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }

        ASSERT_TRUE(m_tempDir.isValid());
        QString dbPath = m_tempDir.path() + "/test_tokens.db";

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath);
        ASSERT_TRUE(db.open());

        QSqlQuery pragma(db);
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA foreign_keys=ON");

        QString migrationDir = m_tempDir.path() + "/migrations";
        TestMigrationUtils::copyAllMigrationsToDir(migrationDir);

        ASSERT_TRUE(Database::migrate(migrationDir));
    }

    void TearDown() override {
        {
            QSqlDatabase db = QSqlDatabase::database();
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }
};

// ── Migration ────────────────────────────────────────────────

TEST_F(TokenAccountDbTest, MigrationSetsVersion) {
    EXPECT_EQ(Database::currentVersion(), TestMigrationUtils::latestMigrationVersion());
}

// ── Token registry ──────────────────────────────────────────

TEST_F(TokenAccountDbTest, UpsertAndGetToken) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));

    auto t = TokenAccountDb::getTokenRecord("native");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->address, QString("native"));
    EXPECT_EQ(t->symbol, QString("SOL"));
    EXPECT_EQ(t->name, QString("Solana"));
    EXPECT_EQ(t->decimals, 9);
}

TEST_F(TokenAccountDbTest, UpsertTokenUpdatesExisting) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", "USDC",
                                            "USD Coin", 6, "TokenkegQEcfiz"));

    // Update name
    EXPECT_TRUE(TokenAccountDb::upsertToken("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", "USDC",
                                            "USD Coin (Circle)", 6, "TokenkegQEcfiz"));

    auto t = TokenAccountDb::getTokenRecord("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->name, QString("USD Coin (Circle)"));
}

TEST_F(TokenAccountDbTest, EmptySymbolRejected) {
    EXPECT_FALSE(TokenAccountDb::upsertToken("mint1", "", "Name", 6, "prog"));
}

TEST_F(TokenAccountDbTest, EmptyNameRejected) {
    EXPECT_FALSE(TokenAccountDb::upsertToken("mint1", "SYM", "", 6, "prog"));
}

// ── Token accounts ──────────────────────────────────────────

TEST_F(TokenAccountDbTest, UpsertAndGetAccount) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));

    EXPECT_TRUE(TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "5000000000",
                                              145.50));

    auto acct = TokenAccountDb::getAccountRecord("WalletAddr1", "native");
    ASSERT_TRUE(acct.has_value());
    EXPECT_EQ(acct->balance, QString("5000000000"));
    EXPECT_DOUBLE_EQ(acct->usdPrice, 145.50);
    EXPECT_EQ(acct->symbol, QString("SOL"));
    EXPECT_EQ(acct->decimals, 9);
}

TEST_F(TokenAccountDbTest, UpsertAccountUpdatesBalance) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));
    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "1000", 100.0));

    // Upsert again with new balance
    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "2000", 150.0));

    auto acct = TokenAccountDb::getAccountRecord("WalletAddr1", "native");
    ASSERT_TRUE(acct.has_value());
    EXPECT_EQ(acct->balance, QString("2000"));
    EXPECT_DOUBLE_EQ(acct->usdPrice, 150.0);
}

TEST_F(TokenAccountDbTest, UpdateBalance) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));
    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "1000", 100.0));

    EXPECT_TRUE(TokenAccountDb::updateBalance("WalletAddr1", "9999", 200.0));

    auto acct = TokenAccountDb::getAccountRecord("WalletAddr1", "native");
    ASSERT_TRUE(acct.has_value());
    EXPECT_EQ(acct->balance, QString("9999"));
    EXPECT_DOUBLE_EQ(acct->usdPrice, 200.0);
}

TEST_F(TokenAccountDbTest, UpdateBalanceNonExistentReturnsFalse) {
    EXPECT_FALSE(TokenAccountDb::updateBalance("nonexistent", "100", 1.0));
}

TEST_F(TokenAccountDbTest, DeleteAccount) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));
    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "1000", 100.0));

    EXPECT_TRUE(TokenAccountDb::deleteAccount("WalletAddr1"));
    EXPECT_FALSE(TokenAccountDb::getAccountRecord("WalletAddr1", "native").has_value());
}

TEST_F(TokenAccountDbTest, DeleteNonExistentReturnsFalse) {
    EXPECT_FALSE(TokenAccountDb::deleteAccount("nonexistent"));
}

// ── Joined query ────────────────────────────────────────────

TEST_F(TokenAccountDbTest, GetAccountsByOwnerJoinsTokenData) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));
    EXPECT_TRUE(TokenAccountDb::upsertToken("EPjFWdd5", "USDC", "USD Coin", 6, "TokenkegQEcfiz"));

    EXPECT_TRUE(TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "5000000000",
                                              145.50));
    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("AtaAddr1", "EPjFWdd5", "WalletAddr1", "100000000", 1.00));

    auto accounts = TokenAccountDb::getAccountsByOwnerRecords("WalletAddr1");
    ASSERT_EQ(accounts.size(), 2);

    // Ordered by USD value descending: SOL (5B * 145.5) > USDC (100M * 1.0)
    EXPECT_EQ(accounts[0].symbol, QString("SOL"));
    EXPECT_EQ(accounts[1].symbol, QString("USDC"));
}

TEST_F(TokenAccountDbTest, GetAccountsByOwnerEmpty) {
    auto accounts = TokenAccountDb::getAccountsByOwnerRecords("nobody");
    EXPECT_EQ(accounts.size(), 0);
}

// ── Unique constraint ───────────────────────────────────────

TEST_F(TokenAccountDbTest, UniqueOwnerTokenConstraint) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));

    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "1000", 100.0));

    // Different account_address but same owner+token should fail
    // (UNIQUE constraint on owner_address, token_address)
    EXPECT_FALSE(
        TokenAccountDb::upsertAccount("DifferentAta", "native", "WalletAddr1", "2000", 100.0));
}

// ── State handling ──────────────────────────────────────────

TEST_F(TokenAccountDbTest, FrozenState) {
    EXPECT_TRUE(TokenAccountDb::upsertToken("mint1", "FREEZE", "Freezable Token", 6, "Token22"));

    EXPECT_TRUE(TokenAccountDb::upsertAccount("Ata1", "mint1", "Owner1", "500", 2.0, "frozen"));

    auto acct = TokenAccountDb::getAccountRecord("Owner1", "mint1");
    ASSERT_TRUE(acct.has_value());
    EXPECT_EQ(acct->state, QString("frozen"));
}

// ── Timestamps ──────────────────────────────────────────────

// ── Native SOL name protection ──────────────────────────────
// Regression: on-chain Metaplex metadata for the WSOL mint returns
// "Wrapped SOL" which was overwriting our "Solana" display name.

static const QString WSOL_MINT = "So11111111111111111111111111111111111111112";

TEST_F(TokenAccountDbTest, NativeSolNameNotOverwrittenByMetadata) {
    // SyncService stores native SOL with correct name
    EXPECT_TRUE(TokenAccountDb::upsertToken(WSOL_MINT, "SOL", "Solana", 9, "native"));

    auto before = TokenAccountDb::getTokenRecord(WSOL_MINT);
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->name, QString("Solana"));
    EXPECT_EQ(before->symbol, QString("SOL"));

    // Simulate what TokenMetadataService does for the WSOL mint:
    // it should write "Solana" / "SOL", NOT the on-chain "Wrapped SOL"
    TokenAccountDb::updateTokenMetadata(WSOL_MINT, "Solana", "SOL", ":/icons/tokens/sol.png");

    auto after = TokenAccountDb::getTokenRecord(WSOL_MINT);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->name, QString("Solana"));
    EXPECT_EQ(after->symbol, QString("SOL"));
}

TEST_F(TokenAccountDbTest, NativeSolNameSurvivesWrappedSolOverwrite) {
    // Set up native SOL correctly
    EXPECT_TRUE(TokenAccountDb::upsertToken(WSOL_MINT, "SOL", "Solana", 9, "native"));

    // Simulate what the old (broken) code did: updateTokenMetadata called
    // with on-chain "Wrapped SOL" from Metaplex metadata. The DB-level guard
    // should force it back to "Solana".
    TokenAccountDb::updateTokenMetadata(WSOL_MINT, "Wrapped SOL", "WSOL", "");

    auto record = TokenAccountDb::getTokenRecord(WSOL_MINT);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->name, QString("Solana"))
        << "updateTokenMetadata must never store 'Wrapped SOL' for native SOL";
    EXPECT_EQ(record->symbol, QString("SOL"))
        << "updateTokenMetadata must never store 'WSOL' symbol for native SOL";
}

// ── Timestamps ──────────────────────────────────────────────

TEST_F(TokenAccountDbTest, TimestampsAutoSet) {
    qint64 before = QDateTime::currentSecsSinceEpoch();

    EXPECT_TRUE(TokenAccountDb::upsertToken("native", "SOL", "Solana", 9, "system"));
    EXPECT_TRUE(
        TokenAccountDb::upsertAccount("WalletAddr1", "native", "WalletAddr1", "1000", 100.0));

    qint64 after = QDateTime::currentSecsSinceEpoch();

    auto token = TokenAccountDb::getTokenRecord("native");
    ASSERT_TRUE(token.has_value());
    EXPECT_GE(token->createdAt, before);
    EXPECT_LE(token->createdAt, after);

    auto acct = TokenAccountDb::getAccountRecord("WalletAddr1", "native");
    ASSERT_TRUE(acct.has_value());
    EXPECT_GE(acct->createdAt, before);
    EXPECT_LE(acct->createdAt, after);
}
