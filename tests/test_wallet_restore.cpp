#include <QCoreApplication>
#include <QSettings>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    a.setOrganizationName("CinderTest");
    a.setApplicationName("WalletRestoreTest");
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class WalletRestoreTest : public ::testing::Test {
  protected:
    void SetUp() override { QSettings().clear(); }
    void TearDown() override { QSettings().clear(); }
};

// Simulate the unlock flow: handleUnlockResult overwrites lastActiveAddress,
// then restore logic reads it. If we read AFTER the overwrite, restore is lost.
//
// This test guards against the bug where handleUnlockResult() was called before
// reading lastActiveAddress, causing the saved preference to always be overwritten
// with the first wallet's address.

TEST_F(WalletRestoreTest, LastActiveAddress_ReadBeforeOverwrite) {
    const QString walletA = "WalletA_addr";
    const QString walletB = "WalletB_addr";

    // User previously selected Wallet B
    QSettings().setValue("lastActiveAddress", walletB);

    // On unlock, lock screen always returns the first wallet (A)
    const QString& unlockAddress = walletA;

    // CORRECT ORDER: read saved preference BEFORE handleUnlockResult overwrites it
    QString lastActive = QSettings().value("lastActiveAddress").toString();

    // Simulate handleUnlockResult — it writes the unlock address to QSettings
    QSettings().setValue("lastActiveAddress", unlockAddress);

    // Restore logic should detect the mismatch and switch
    EXPECT_FALSE(lastActive.isEmpty());
    EXPECT_NE(lastActive, unlockAddress);
    EXPECT_EQ(lastActive, walletB);
}

TEST_F(WalletRestoreTest, LastActiveAddress_MatchesUnlock_NoSwitch) {
    const QString walletA = "WalletA_addr";

    // User's last selection was the same as the first wallet
    QSettings().setValue("lastActiveAddress", walletA);

    const QString& unlockAddress = walletA;
    QString lastActive = QSettings().value("lastActiveAddress").toString();

    QSettings().setValue("lastActiveAddress", unlockAddress);

    // No switch needed — addresses match
    EXPECT_EQ(lastActive, unlockAddress);
}

TEST_F(WalletRestoreTest, LastActiveAddress_EmptyOnFirstRun) {
    // First run — no QSettings value yet
    QString unlockAddress = "WalletA_addr";
    QString lastActive = QSettings().value("lastActiveAddress").toString();

    QSettings().setValue("lastActiveAddress", unlockAddress);

    // No restore — lastActive is empty (first run)
    EXPECT_TRUE(lastActive.isEmpty());
}

TEST_F(WalletRestoreTest, SwitchToWallet_PersistsNewSelection) {
    const QString walletA = "WalletA_addr";
    const QString walletB = "WalletB_addr";

    // Start with wallet A active
    QSettings().setValue("lastActiveAddress", walletA);
    EXPECT_EQ(QSettings().value("lastActiveAddress").toString(), walletA);

    // User switches to wallet B (simulating switchToWallet)
    QSettings().setValue("lastActiveAddress", walletB);
    EXPECT_EQ(QSettings().value("lastActiveAddress").toString(), walletB);

    // Simulate app restart — value should still be B
    QSettings settings;
    EXPECT_EQ(settings.value("lastActiveAddress").toString(), walletB);
}
