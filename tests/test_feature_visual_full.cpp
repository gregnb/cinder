#include <gtest/gtest.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

#include "StyleLoader.h"
#include "VisualTestUtils.h"
#include "crypto/Keypair.h"
#include "db/ContactDb.h"
#include "db/Database.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "db/WalletDb.h"
#include "services/AvatarCache.h"
#include "services/IdlRegistry.h"
#include "services/NetworkStatsService.h"
#include "services/PriceService.h"
#include "services/SolanaApi.h"

#include "features/agents/AgentsPage.h"
#include "features/lockscreen/LockScreen.h"
#define private public
#include "features/sendreceive/SendReceivePage.h"
#include "features/txlookup/TxLookupPage.h"
#undef private
#include "features/settings/SettingsPage.h"
#include "features/terminal/TerminalPage.h"
#include "services/model/TransactionResponse.h"

namespace {
    using VisualTestUtils::capturePage;
    using VisualTestUtils::settleUi;
    using VisualRecorder = VisualTestUtils::VisualRecorder;
    // Capturing the send/receive page still shows small raster/focus drift in
    // full-suite execution. 3500px covers the measured worst-case while keeping
    // the frame under visual test coverage.
    constexpr int kSendReceiveTolerance = 3500;

    void seedTransactionDb(const QString& ownerAddress, int count) {
        qint64 now = QDateTime::currentSecsSinceEpoch();
        for (int i = 0; i < count; ++i) {
            QString sig =
                QString("Sig%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")
                    .arg(i, 5, 10, QChar('0'));
            int slot = 280000000 - i;
            qint64 blockTime = now - (i * 60);
            int fee = 5000;
            bool err = false;

            QString counterparty = QString("Addr%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")
                                       .arg(i % 20, 4, 10, QChar('0'));
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

    void seedTokenAccounts(const QString& ownerAddress, int count) {
        for (int i = 0; i < count; ++i) {
            QString mint =
                QString("Mint%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
            QString symbol = QString("TKN%1").arg(i);
            QString name = QString("Token %1").arg(i);
            TokenAccountDb::upsertToken(mint, symbol, name, 9,
                                        "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");

            QString acctAddr =
                QString("Acct%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
            double balance = 100.0 + i * 10.5;
            TokenAccountDb::upsertAccount(acctAddr, mint, ownerAddress,
                                          QString::number(balance, 'f', 9), 1.50 + i * 0.1);
        }

        TokenAccountDb::upsertToken("So11111111111111111111111111111111111111112", "SOL",
                                    "Wrapped SOL", 9,
                                    "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
        TokenAccountDb::upsertAccount("SolAcct111111111111111111111111111111111111",
                                      "So11111111111111111111111111111111111111112", ownerAddress,
                                      "5.000000000", 150.0);
    }

    void seedContacts(int count) {
        for (int i = 0; i < count; ++i) {
            QString name = QString("Contact %1").arg(i);
            QString address =
                QString("Addr%1AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA").arg(i, 4, 10, QChar('0'));
            ContactDb::insertContact(name, address);
        }
    }

    void seedPortfolioSnapshots(int count) {
        qint64 now = QDateTime::currentSecsSinceEpoch();
        for (int i = 0; i < count; ++i) {
            qint64 ts = now - (count - i) * 3600;
            double totalUsd = 500.0 + i * 2.5;
            double solPrice = 150.0 + (i % 10) * 0.5;
            int snapId =
                PortfolioDb::insertSnapshot(QStringLiteral("test_address"), ts, totalUsd, solPrice);
            if (snapId > 0) {
                PortfolioDb::insertTokenSnapshot(snapId,
                                                 "So11111111111111111111111111111111111111112",
                                                 "SOL", 5.0, solPrice, 5.0 * solPrice);
            }
        }
    }

    void seedWalletsForUi() {
        const QByteArray salt(16, 's');
        const QByteArray nonce(24, 'n');
        const QByteArray cipher(64, 'c');

        WalletDb::insertWallet(QStringLiteral("My Wallet"),
                               QStringLiteral("6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"),
                               QStringLiteral("private_key"), salt, nonce, cipher);
        WalletDb::insertWallet(QStringLiteral("Greg"),
                               QStringLiteral("BDBY9tWqj1Pe4qVJxmhjLYZoALfwSNNEG91HrbgZ73VH"),
                               QStringLiteral("mnemonic"), salt, nonce, cipher);
    }

    TransactionResponse buildSyntheticTokenTransferTx() {
        TransactionResponse tx;
        tx.slot = 123456789;
        tx.blockTime = 1772625600;
        tx.version = QStringLiteral("legacy");
        tx.message.recentBlockhash =
            QStringLiteral("RecentBlockhash1111111111111111111111111111111");

        AccountKey signer;
        signer.pubkey = QStringLiteral("Sender1111111111111111111111111111111111111");
        signer.signer = true;
        signer.writable = true;
        signer.source = QStringLiteral("transaction");
        tx.message.accountKeys.append(signer);

        AccountKey senderToken;
        senderToken.pubkey = QStringLiteral("SenderTokenAcct1111111111111111111111111111");
        senderToken.writable = true;
        senderToken.source = QStringLiteral("transaction");
        tx.message.accountKeys.append(senderToken);

        AccountKey receiverToken;
        receiverToken.pubkey = QStringLiteral("ReceiverTokenAcct1111111111111111111111111");
        receiverToken.writable = true;
        receiverToken.source = QStringLiteral("transaction");
        tx.message.accountKeys.append(receiverToken);

        Instruction ix;
        ix.program = QStringLiteral("spl-token");
        ix.programId = QStringLiteral("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
        ix.type = QStringLiteral("transferChecked");
        ix.info.insert(QStringLiteral("source"), senderToken.pubkey);
        ix.info.insert(QStringLiteral("destination"), receiverToken.pubkey);
        ix.info.insert(QStringLiteral("mint"),
                       QStringLiteral("Mint111111111111111111111111111111111111111"));
        QJsonObject tokenAmount;
        tokenAmount.insert(QStringLiteral("uiAmount"), 22.822957);
        tokenAmount.insert(QStringLiteral("uiAmountString"), QStringLiteral("22.822957"));
        tokenAmount.insert(QStringLiteral("decimals"), 6);
        tokenAmount.insert(QStringLiteral("amount"), QStringLiteral("22822957"));
        ix.info.insert(QStringLiteral("tokenAmount"), tokenAmount);
        tx.message.instructions.append(ix);

        tx.meta.fee = 5000;
        tx.meta.computeUnitsConsumed = 26820;
        tx.meta.preBalances = {1000000, 0, 0};
        tx.meta.postBalances = {995000, 0, 0};

        TokenBalance preSender;
        preSender.accountIndex = 1;
        preSender.mint = QStringLiteral("Mint111111111111111111111111111111111111111");
        preSender.owner = signer.pubkey;
        preSender.programId = QStringLiteral("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
        preSender.amount.amount = QStringLiteral("22822957");
        preSender.amount.decimals = 6;
        preSender.amount.uiAmount = 22.822957;
        preSender.amount.uiAmountString = QStringLiteral("22.822957");
        tx.meta.preTokenBalances.append(preSender);

        TokenBalance postSender = preSender;
        postSender.amount.amount = QStringLiteral("0");
        postSender.amount.uiAmount = 0.0;
        postSender.amount.uiAmountString = QStringLiteral("0");
        tx.meta.postTokenBalances.append(postSender);

        TokenBalance postReceiver;
        postReceiver.accountIndex = 2;
        postReceiver.mint = preSender.mint;
        postReceiver.owner = QStringLiteral("Receiver11111111111111111111111111111111111");
        postReceiver.programId = preSender.programId;
        postReceiver.amount.amount = QStringLiteral("22822957");
        postReceiver.amount.decimals = 6;
        postReceiver.amount.uiAmount = 22.822957;
        postReceiver.amount.uiAmountString = QStringLiteral("22.822957");
        tx.meta.postTokenBalances.append(postReceiver);

        return tx;
    }

    class FeatureVisualFullTest : public ::testing::Test {
      protected:
        static void SetUpTestSuite() {
            QFontDatabase::addApplicationFont(":/fonts/Exo2-Variable.ttf");
            QFont defaultFont("Exo 2", 14);
            qApp->setFont(defaultFont);
            qApp->setStyleSheet(StyleLoader::loadTheme());
        }

        void SetUp() override {
            m_tmpDir = std::make_unique<QTemporaryDir>();
            ASSERT_TRUE(m_tmpDir->isValid());

            QSettings settings;
            settings.clear();
            settings.sync();

            if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
                QSqlDatabase::database().close();
                QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
            }

            const QString dbPath = m_tmpDir->path() + "/test.db";
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
            db.setDatabaseName(dbPath);
            ASSERT_TRUE(db.open());

            QSqlQuery pragma(db);
            pragma.exec("PRAGMA journal_mode=WAL");
            pragma.exec("PRAGMA foreign_keys=ON");

            ASSERT_TRUE(Database::migrate());

            m_owner = QStringLiteral("TestWallet1111111111111111111111111111111111");
            seedTransactionDb(m_owner, 120);
            seedPortfolioSnapshots(60);
            seedTokenAccounts(m_owner, 12);
            seedContacts(30);
            seedWalletsForUi();
        }

        void TearDown() override {
            if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
                QSqlDatabase::database().close();
                QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
            }
            m_tmpDir.reset();
        }

        QString m_owner;

      private:
        std::unique_ptr<QTemporaryDir> m_tmpDir;
    };

    TEST_F(FeatureVisualFullTest, CaptureRemainingFeatureBaselines) {
        VisualRecorder recorder(VisualTestUtils::repoOwnedVisualRoot("feature"),
                                "FEATURE_VISUAL_ROOT_DIR", "FEATURE_VISUAL_UPDATE_BASELINE",
                                "FEATURE_VISUAL_MAX_DIFF_PIXELS");

        SolanaApi api;
        AvatarCache avatarCache;
        IdlRegistry idlRegistry(&api);
        NetworkStatsService networkStats(&api);
        PriceService priceService;
        const Keypair kp = Keypair::generate();

        SendReceivePage sendReceive;
        sendReceive.setSolanaApi(&api);
        sendReceive.setAvatarCache(&avatarCache);
        sendReceive.setWalletAddress(m_owner);
        sendReceive.setKeypair(kp);
        sendReceive.refreshBalances();
        capturePage(sendReceive, recorder, "feature_sendreceive", 80, kSendReceiveTolerance, 1440,
                    900, "featureVisualHost");

        TerminalPage terminal(&api, &idlRegistry, &networkStats, &priceService);
        terminal.setWalletAddress(m_owner);
        terminal.setKeypair(kp);
        capturePage(terminal, recorder, "feature_terminal", 80, -1, 1440, 900, "featureVisualHost");

        SettingsPage settings(&api);
        settings.setWalletAddress(QStringLiteral("6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"));
        capturePage(settings, recorder, "feature_settings", 80, -1, 1440, 900, "featureVisualHost");

        AgentsPage agents;
        agents.setWalletAddress(QStringLiteral("6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"));
        agents.setSolanaApi(&api);
        capturePage(agents, recorder, "feature_agents", 80, -1, 1440, 900, "featureVisualHost");

        LockScreen lockScreen;
        capturePage(lockScreen, recorder, "feature_lockscreen", 80, -1, 1440, 900,
                    "featureVisualHost");
    }

    TEST_F(FeatureVisualFullTest, CreateTokenReviewButtonDisabledUntilRequiredFieldsFilled) {
        SolanaApi api;
        AvatarCache avatarCache;
        const Keypair kp = Keypair::generate();

        SendReceivePage sendReceive;
        sendReceive.setSolanaApi(&api);
        sendReceive.setAvatarCache(&avatarCache);
        sendReceive.setWalletAddress(m_owner);
        sendReceive.setKeypair(kp);
        sendReceive.setCurrentPage(SendReceivePage::StackPage::CreateToken);
        sendReceive.show();
        settleUi(60);

        QList<QLineEdit*> lineEdits =
            sendReceive.findChildren<QLineEdit*>(QString(), Qt::FindChildrenRecursively);

        QPushButton* createReviewBtn = nullptr;
        for (QPushButton* button : sendReceive.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Review Create Token")) {
                createReviewBtn = button;
                break;
            }
        }
        ASSERT_NE(createReviewBtn, nullptr);

        QLineEdit* nameInput = nullptr;
        QLineEdit* symbolInput = nullptr;
        for (QLineEdit* edit : lineEdits) {
            if (edit->placeholderText() == QStringLiteral("e.g. My Token")) {
                nameInput = edit;
            } else if (edit->placeholderText() == QStringLiteral("e.g. MTK")) {
                symbolInput = edit;
            }
        }
        ASSERT_NE(nameInput, nullptr);
        ASSERT_NE(symbolInput, nullptr);

        EXPECT_FALSE(createReviewBtn->isEnabled());

        nameInput->setText(QStringLiteral("Example Token"));
        settleUi(20);
        EXPECT_FALSE(createReviewBtn->isEnabled());

        symbolInput->setText(QStringLiteral("EXT"));
        settleUi(20);
        EXPECT_TRUE(createReviewBtn->isEnabled());

        sendReceive.hide();
        settleUi(20);
    }

    TEST_F(FeatureVisualFullTest, TxLookupUsesRealParticipantWhenWalletIsUnrelated) {
        TxLookupPage txLookup(nullptr);
        txLookup.setWalletAddress(QStringLiteral("UnrelatedWallet1111111111111111111111111111"));

        const QString signature =
            QStringLiteral("SigSynthetic111111111111111111111111111111111111111111111111");
        const TransactionResponse tx = buildSyntheticTokenTransferTx();

        txLookup.handleFetchedTransaction(signature, tx);
        txLookup.show();
        settleUi(60);

        ASSERT_NE(txLookup.m_summaryLayout, nullptr);
        ASSERT_GT(txLookup.m_summaryLayout->count(), 0);

        QWidget* titleRow = txLookup.m_summaryLayout->itemAt(0)->widget();
        ASSERT_NE(titleRow, nullptr);

        QList<QLabel*> labels = titleRow->findChildren<QLabel*>();
        QStringList texts;
        for (QLabel* label : labels) {
            texts.append(label->text());
        }

        EXPECT_TRUE(texts.contains(QStringLiteral("<b>Token Send</b>")) ||
                    texts.contains(QStringLiteral("Token Send")));
        EXPECT_TRUE(texts.contains(QStringLiteral("22.8230")) ||
                    texts.contains(QStringLiteral("22.822957")));
        EXPECT_FALSE(texts.contains(QStringLiteral("0")));

        EXPECT_EQ(txLookup.m_sigLabel->text(), signature);
        EXPECT_EQ(txLookup.m_resultBadge->text(), QStringLiteral("SUCCESS"));

        txLookup.hide();
        settleUi(20);
    }

} // namespace

int main(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);

    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
