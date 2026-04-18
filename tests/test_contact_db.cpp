#include "TestMigrationUtils.h"
#include "db/ContactDb.h"
#include "db/Database.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <gtest/gtest.h>

// Qt requires a QCoreApplication for QSqlDatabase
static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── Test fixture: fresh DB per test ──────────────────────

class ContactDbTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        // Close any leftover connection
        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
            QSqlDatabase::database().close();
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }

        ASSERT_TRUE(m_tempDir.isValid());
        QString dbPath = m_tempDir.path() + "/test_contacts.db";

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

// ── Migration tests ───────────────────────────────────────

TEST_F(ContactDbTest, MigrationSetsVersion) {
    EXPECT_EQ(Database::currentVersion(), TestMigrationUtils::latestMigrationVersion());
}

// ── Insert / Retrieve ─────────────────────────────────────

TEST_F(ContactDbTest, InsertAndRetrieve) {
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));

    auto contacts = ContactDb::getAllRecords();
    ASSERT_EQ(contacts.size(), 1);
    EXPECT_EQ(contacts[0].name, QString("Alice"));
    EXPECT_EQ(contacts[0].address, QString("7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
}

TEST_F(ContactDbTest, InsertDuplicateAddressFails) {
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    EXPECT_FALSE(ContactDb::insertContact("Bob", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));

    auto contacts = ContactDb::getAllRecords();
    EXPECT_EQ(contacts.size(), 1);
}

TEST_F(ContactDbTest, InsertDuplicateNameAllowed) {
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    EXPECT_TRUE(ContactDb::insertContact("Alice", "EVHeEbMBarH0csGYG1NElmzWpbNfFz"));

    auto contacts = ContactDb::getAllRecords();
    EXPECT_EQ(contacts.size(), 2);
}

// ── Update ────────────────────────────────────────────────

TEST_F(ContactDbTest, UpdateContact) {
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));

    auto contacts = ContactDb::getAllRecords();
    ASSERT_EQ(contacts.size(), 1);
    int id = contacts[0].id;

    EXPECT_TRUE(ContactDb::updateContact(id, "Bob", "NewAddr123"));

    auto updated = ContactDb::getByIdRecord(id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->name, QString("Bob"));
    EXPECT_EQ(updated->address, QString("NewAddr123"));
}

// ── Delete ────────────────────────────────────────────────

TEST_F(ContactDbTest, DeleteContact) {
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));

    auto contacts = ContactDb::getAllRecords();
    ASSERT_EQ(contacts.size(), 1);
    int id = contacts[0].id;

    EXPECT_TRUE(ContactDb::deleteContact(id));
    EXPECT_EQ(ContactDb::getAllRecords().size(), 0);
}

TEST_F(ContactDbTest, DeleteNonExistentReturnsFalse) {
    EXPECT_FALSE(ContactDb::deleteContact(9999));
}

// ── HasAddress ────────────────────────────────────────────

TEST_F(ContactDbTest, HasAddress) {
    EXPECT_FALSE(ContactDb::hasAddress("7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));

    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    EXPECT_TRUE(ContactDb::hasAddress("7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    EXPECT_FALSE(ContactDb::hasAddress("nonexistent"));
}

// ── Search / Filter ───────────────────────────────────────

TEST_F(ContactDbTest, GetAllWithSearch) {
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    EXPECT_TRUE(ContactDb::insertContact("Bob", "EVHeEbMBarH0csGYG1NElmzWpbNfFz"));
    EXPECT_TRUE(ContactDb::insertContact("Charlie", "BeHEblrEWin33ZVCmVtnh5yhl6kd"));

    // Search by partial name
    auto results = ContactDb::getAllRecords("ali");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, QString("Alice"));

    // Search by partial address
    results = ContactDb::getAllRecords("EVHe");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, QString("Bob"));

    // Empty search returns all
    results = ContactDb::getAllRecords("");
    EXPECT_EQ(results.size(), 3);
}

// ── Ordering ──────────────────────────────────────────────

TEST_F(ContactDbTest, GetAllOrderedByName) {
    EXPECT_TRUE(ContactDb::insertContact("Charlie", "addr3"));
    EXPECT_TRUE(ContactDb::insertContact("alice", "addr1")); // lowercase
    EXPECT_TRUE(ContactDb::insertContact("Bob", "addr2"));

    auto contacts = ContactDb::getAllRecords();
    ASSERT_EQ(contacts.size(), 3);
    EXPECT_EQ(contacts[0].name, QString("alice"));
    EXPECT_EQ(contacts[1].name, QString("Bob"));
    EXPECT_EQ(contacts[2].name, QString("Charlie"));
}

// ── CHECK constraints ─────────────────────────────────────

TEST_F(ContactDbTest, EmptyNameRejected) {
    EXPECT_FALSE(ContactDb::insertContact("", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    EXPECT_EQ(ContactDb::getAllRecords().size(), 0);
}

TEST_F(ContactDbTest, EmptyAddressRejected) {
    EXPECT_FALSE(ContactDb::insertContact("Alice", ""));
    EXPECT_EQ(ContactDb::getAllRecords().size(), 0);
}

// ── Timestamp ─────────────────────────────────────────────

TEST_F(ContactDbTest, CreatedAtAutoSet) {
    qint64 before = QDateTime::currentSecsSinceEpoch();
    EXPECT_TRUE(ContactDb::insertContact("Alice", "7Xw3a8yFoo92B5eZtZnMn8ntihAhwat"));
    qint64 after = QDateTime::currentSecsSinceEpoch();

    auto contacts = ContactDb::getAllRecords();
    ASSERT_EQ(contacts.size(), 1);

    qint64 createdAt = contacts[0].createdAt;
    EXPECT_GE(createdAt, before);
    EXPECT_LE(createdAt, after);
}
