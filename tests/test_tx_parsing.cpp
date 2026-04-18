#include "TestMigrationUtils.h"
#include "db/Database.h"
#include "db/TransactionDb.h"
#include "services/model/TransactionResponse.h"
#include "tx/TxClassifier.h"
#include "tx/TxParseUtils.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
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

// ── Test fixture ────────────────────────────────────────────

class TxParsingTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
            QSqlDatabase::database().close();
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }

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

    static const inline QString WALLET = "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm";
    static const inline QString NONCE_ACCOUNT = "2C8GAtCyZZLdRTReEUnMXcPXd6t5X56pijgmM1iKfQXQ";
    static const inline QString NONCE_TX_SIG =
        "SJGZFCfh6QQTdh1C8ve8oiPXFZ2zrd67hgK59MoeHYH4PTjrgh3JXPrGAcucKf9rME935M6VM6zd1xYabBhdU9F";

    // Real nonce account creation TX from mainnet (jsonParsed)
    QJsonObject nonceCreationTxJson() {
        const char* raw = R"({
            "blockTime": 1771637983,
            "meta": {
                "computeUnitsConsumed": 300,
                "err": null,
                "fee": 10000,
                "innerInstructions": [],
                "logMessages": [
                    "Program 11111111111111111111111111111111 invoke [1]",
                    "Program 11111111111111111111111111111111 success",
                    "Program 11111111111111111111111111111111 invoke [1]",
                    "Program 11111111111111111111111111111111 success"
                ],
                "postBalances": [8542320, 1447680, 1, 42706560, 1009200],
                "preBalances": [10000000, 0, 1, 42706560, 1009200],
                "postTokenBalances": [],
                "preTokenBalances": [],
                "rewards": [],
                "status": {"Ok": null}
            },
            "slot": 401648978,
            "transaction": {
                "message": {
                    "accountKeys": [
                        {"pubkey": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                         "signer": true, "source": "transaction", "writable": true},
                        {"pubkey": "2C8GAtCyZZLdRTReEUnMXcPXd6t5X56pijgmM1iKfQXQ",
                         "signer": true, "source": "transaction", "writable": true},
                        {"pubkey": "11111111111111111111111111111111",
                         "signer": false, "source": "transaction", "writable": false},
                        {"pubkey": "SysvarRecentB1ockHashes11111111111111111111",
                         "signer": false, "source": "transaction", "writable": false},
                        {"pubkey": "SysvarRent111111111111111111111111111111111",
                         "signer": false, "source": "transaction", "writable": false}
                    ],
                    "instructions": [
                        {
                            "parsed": {
                                "info": {
                                    "lamports": 1447680,
                                    "newAccount": "2C8GAtCyZZLdRTReEUnMXcPXd6t5X56pijgmM1iKfQXQ",
                                    "owner": "11111111111111111111111111111111",
                                    "source": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                                    "space": 80
                                },
                                "type": "createAccount"
                            },
                            "program": "system",
                            "programId": "11111111111111111111111111111111",
                            "stackHeight": 1
                        },
                        {
                            "parsed": {
                                "info": {
                                    "nonceAccount": "2C8GAtCyZZLdRTReEUnMXcPXd6t5X56pijgmM1iKfQXQ",
                                    "nonceAuthority": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                                    "recentBlockhashesSysvar": "SysvarRecentB1ockHashes11111111111111111111",
                                    "rentSysvar": "SysvarRent111111111111111111111111111111111"
                                },
                                "type": "initializeNonce"
                            },
                            "program": "system",
                            "programId": "11111111111111111111111111111111",
                            "stackHeight": 1
                        }
                    ],
                    "recentBlockhash": "9FQj6Xh6Turwk7aLgucykcpa76Tded2oqMKC8e3XFX8v"
                },
                "signatures": [
                    "SJGZFCfh6QQTdh1C8ve8oiPXFZ2zrd67hgK59MoeHYH4PTjrgh3JXPrGAcucKf9rME935M6VM6zd1xYabBhdU9F",
                    "3bqAAdHG1brCgeDrwHD3ua8wt2pt65ZBRsazSwoBFvxGFGkh1tBXHZGg3xtGy55mzTkzoK3G6m5Fqmcm1SywA7hK"
                ]
            },
            "version": "legacy"
        })";

        return QJsonDocument::fromJson(raw).object();
    }
};

// ── Tests: Nonce creation TX parsing ────────────────────────

TEST_F(TxParsingTest, HydrateNonceCreationTx) {
    QJsonObject json = nonceCreationTxJson();
    TransactionResponse tx = TransactionResponse::fromJson(json);

    EXPECT_EQ(tx.slot, 401648978);
    EXPECT_EQ(tx.blockTime, 1771637983);
    EXPECT_EQ(tx.version, "legacy");
    ASSERT_EQ(tx.message.instructions.size(), 2);

    // First instruction: createAccount
    const Instruction& ix0 = tx.message.instructions[0];
    EXPECT_TRUE(ix0.isParsed());
    EXPECT_EQ(ix0.program, "system");
    EXPECT_EQ(ix0.type, "createAccount");
    EXPECT_EQ(ix0.info["source"].toString(), WALLET);
    EXPECT_EQ(ix0.info["newAccount"].toString(), NONCE_ACCOUNT);
    EXPECT_EQ(ix0.info["lamports"].toInteger(), 1447680);

    // Second instruction: initializeNonce
    const Instruction& ix1 = tx.message.instructions[1];
    EXPECT_TRUE(ix1.isParsed());
    EXPECT_EQ(ix1.program, "system");
    EXPECT_EQ(ix1.type, "initializeNonce");
    EXPECT_EQ(ix1.info["nonceAccount"].toString(), NONCE_ACCOUNT);
    EXPECT_EQ(ix1.info["nonceAuthority"].toString(), WALLET);
}

TEST_F(TxParsingTest, ParseNonceCreationActivities) {
    TransactionResponse tx = TransactionResponse::fromJson(nonceCreationTxJson());
    QList<Activity> activities = TxParseUtils::extractActivities(tx, WALLET);

    ASSERT_EQ(activities.size(), 2);

    // Activity 0: createAccount
    EXPECT_EQ(activities[0].activityType, "create_account");
    EXPECT_EQ(activities[0].token, "SOL");
    EXPECT_EQ(activities[0].fromAddress, WALLET);
    EXPECT_EQ(activities[0].toAddress, NONCE_ACCOUNT);
    EXPECT_NEAR(activities[0].amount, 0.00144768, 1e-9);

    // Activity 1: initializeNonce
    EXPECT_EQ(activities[1].activityType, "create_nonce");
    EXPECT_EQ(activities[1].token, "SOL");
    EXPECT_EQ(activities[1].fromAddress, WALLET);
    EXPECT_EQ(activities[1].toAddress, NONCE_ACCOUNT);
    EXPECT_DOUBLE_EQ(activities[1].amount, 0.0);
}

TEST_F(TxParsingTest, InsertNonceCreationIntoDb) {
    TransactionResponse tx = TransactionResponse::fromJson(nonceCreationTxJson());
    QList<Activity> activities = TxParseUtils::extractActivities(tx, WALLET);
    QString rawJson = QJsonDocument(tx.rawJson).toJson(QJsonDocument::Compact);

    ASSERT_TRUE(TransactionDb::insertTransaction(NONCE_TX_SIG, tx.slot, tx.blockTime, rawJson,
                                                 static_cast<int>(tx.meta.fee), tx.meta.hasError,
                                                 activities));

    EXPECT_TRUE(TransactionDb::hasTransaction(NONCE_TX_SIG));

    // Verify activities were stored
    auto rows = TransactionDb::getTransactionsRecords(WALLET);
    ASSERT_EQ(rows.size(), 2);

    // Ordered by block_time DESC — both have the same time, so order may vary
    bool foundCreateAccount = false;
    bool foundCreateNonce = false;

    for (const auto& row : rows) {
        QString type = row.activityType;
        if (type == "create_account") {
            foundCreateAccount = true;
            EXPECT_EQ(row.fromAddress, WALLET);
            EXPECT_EQ(row.toAddress, NONCE_ACCOUNT);
            EXPECT_NEAR(row.amount, 0.00144768, 1e-9);
            EXPECT_EQ(row.token, "SOL");
            EXPECT_EQ(row.fee, 10000);
            EXPECT_EQ(row.err, 0);
        } else if (type == "create_nonce") {
            foundCreateNonce = true;
            EXPECT_EQ(row.fromAddress, WALLET);
            EXPECT_EQ(row.toAddress, NONCE_ACCOUNT);
            EXPECT_DOUBLE_EQ(row.amount, 0.0);
        }
    }

    EXPECT_TRUE(foundCreateAccount) << "create_account activity not found";
    EXPECT_TRUE(foundCreateNonce) << "create_nonce activity not found";

    // Verify raw JSON was stored
    QString storedRaw = TransactionDb::getRawJson(NONCE_TX_SIG);
    EXPECT_FALSE(storedRaw.isEmpty());
    EXPECT_TRUE(storedRaw.contains("initializeNonce"));
}

// ── Tests: SPL token activity parsing ───────────────────────

TEST_F(TxParsingTest, ParseMintToActivity) {
    // Synthetic mintTo instruction — mintAuthority is our wallet
    const char* raw = R"({
        "blockTime": 1771700000, "slot": 402000000,
        "version": "legacy",
        "meta": {"fee": 5000, "err": null, "innerInstructions": [],
                 "preBalances": [], "postBalances": [], "logMessages": [],
                 "preTokenBalances": [], "postTokenBalances": []},
        "transaction": {
            "signatures": ["mint_sig_001"],
            "message": {
                "accountKeys": [],
                "recentBlockhash": "AAAA",
                "instructions": [{
                    "parsed": {
                        "type": "mintTo",
                        "info": {
                            "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                            "mintAuthority": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                            "account": "ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL",
                            "amount": "1000000"
                        }
                    },
                    "program": "spl-token",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"
                }]
            }
        }
    })";

    TransactionResponse tx = TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());
    auto activities = TxParseUtils::extractActivities(tx, WALLET);

    // mintAuthority == WALLET → involvesWallet → stored
    ASSERT_EQ(activities.size(), 1);
    EXPECT_EQ(activities[0].activityType, "mint");
    EXPECT_EQ(activities[0].token, "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
    EXPECT_EQ(activities[0].fromAddress, WALLET);
    EXPECT_DOUBLE_EQ(activities[0].amount, 1000000.0);

    // Insert and verify DB
    ASSERT_TRUE(TransactionDb::insertTransaction("mint_sig_001", tx.slot, tx.blockTime, "{}", 5000,
                                                 false, activities));

    auto rows = TransactionDb::getTransactionsRecords({}, {}, "mint");
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].activityType, "mint");
}

TEST_F(TxParsingTest, ClassifyTokenTransferFallsBackWhenWalletPerspectiveIsNotParticipant) {
    const char* raw = R"({
        "blockTime": 1774451612,
        "meta": {
            "computeUnitsConsumed": 26820,
            "err": null,
            "fee": 817000,
            "innerInstructions": [],
            "logMessages": [],
            "postBalances": [1000000, 1000000, 1000000, 1000000],
            "preBalances": [1817000, 1000000, 1000000, 1000000],
            "postTokenBalances": [
                {
                    "accountIndex": 1,
                    "mint": "MintAlpha111111111111111111111111111111111",
                    "owner": "ReceiverOwner111111111111111111111111111111",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "22822957",
                        "decimals": 6,
                        "uiAmount": 22.822957,
                        "uiAmountString": "22.822957"
                    }
                },
                {
                    "accountIndex": 2,
                    "mint": "MintAlpha111111111111111111111111111111111",
                    "owner": "SignerOwner11111111111111111111111111111111",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "354542374",
                        "decimals": 6,
                        "uiAmount": 354.542374,
                        "uiAmountString": "354.542374"
                    }
                }
            ],
            "preTokenBalances": [
                {
                    "accountIndex": 2,
                    "mint": "MintAlpha111111111111111111111111111111111",
                    "owner": "SignerOwner11111111111111111111111111111111",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "377365331",
                        "decimals": 6,
                        "uiAmount": 377.365331,
                        "uiAmountString": "377.365331"
                    }
                }
            ],
            "rewards": [],
            "status": {"Ok": null}
        },
        "slot": 408790312,
        "transaction": {
            "message": {
                "accountKeys": [
                    {
                        "pubkey": "SignerOwner11111111111111111111111111111111",
                        "signer": true,
                        "source": "transaction",
                        "writable": true
                    },
                    {
                        "pubkey": "ReceiverTokenAcct111111111111111111111111111",
                        "signer": false,
                        "source": "transaction",
                        "writable": true
                    },
                    {
                        "pubkey": "SenderTokenAcct11111111111111111111111111111",
                        "signer": false,
                        "source": "transaction",
                        "writable": true
                    },
                    {
                        "pubkey": "MintAlpha111111111111111111111111111111111",
                        "signer": false,
                        "source": "transaction",
                        "writable": false
                    }
                ],
                "instructions": [
                    {
                        "parsed": {
                            "info": {
                                "authority": "SignerOwner11111111111111111111111111111111",
                                "destination": "ReceiverTokenAcct111111111111111111111111111",
                                "mint": "MintAlpha111111111111111111111111111111111",
                                "source": "SenderTokenAcct11111111111111111111111111111",
                                "tokenAmount": {
                                    "amount": "22822957",
                                    "decimals": 6,
                                    "uiAmount": 22.822957,
                                    "uiAmountString": "22.822957"
                                }
                            },
                            "type": "transferChecked"
                        },
                        "program": "spl-token",
                        "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                        "stackHeight": 1
                    }
                ],
                "recentBlockhash": "RecentBlockhash111111111111111111111111111111"
            },
            "signatures": [
                "SyntheticSig111111111111111111111111111111111111111111111111111111"
            ]
        },
        "version": "legacy"
    })";

    const TransactionResponse tx =
        TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());

    const auto cls = TxClassifier::classify(tx, "UnrelatedWallet1111111111111111111111111111111");

    EXPECT_EQ(cls.type, TxClassifier::TxType::TokenTransfer);
    EXPECT_EQ(cls.label, QCoreApplication::translate("TxClassifier", "Token Send"));
    EXPECT_NEAR(cls.amount, 22.822957, 1e-9);
    EXPECT_EQ(cls.mint, QStringLiteral("MintAlpha111111111111111111111111111111111"));
    EXPECT_EQ(cls.from, QStringLiteral("SignerOwner11111111111111111111111111111111"));
    EXPECT_EQ(cls.to, QStringLiteral("ReceiverOwner111111111111111111111111111111"));
}

TEST_F(TxParsingTest, ParseBurnCheckedActivity) {
    // Burn from a token account owned by our wallet.
    // The token account "BurnAccountXXX" is resolved via postTokenBalances.
    const char* raw = R"({
        "blockTime": 1771700000, "slot": 402000000,
        "version": "legacy",
        "meta": {"fee": 5000, "err": null, "innerInstructions": [],
                 "preBalances": [100000000, 2039280], "postBalances": [99995000, 2039280],
                 "logMessages": [],
                 "preTokenBalances": [{
                     "accountIndex": 1,
                     "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                     "owner": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                     "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                     "uiTokenAmount": {"amount": "500000000", "decimals": 6, "uiAmount": 500.0}
                 }],
                 "postTokenBalances": [{
                     "accountIndex": 1,
                     "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                     "owner": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                     "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                     "uiTokenAmount": {"amount": "0", "decimals": 6, "uiAmount": 0.0}
                 }]},
        "transaction": {
            "signatures": ["burn_sig_001"],
            "message": {
                "accountKeys": [
                    {"pubkey": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                     "signer": true, "source": "transaction", "writable": true},
                    {"pubkey": "BurnAccountXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                     "signer": false, "source": "transaction", "writable": true}
                ],
                "recentBlockhash": "BBBB",
                "instructions": [{
                    "parsed": {
                        "type": "burnChecked",
                        "info": {
                            "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                            "account": "BurnAccountXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                            "authority": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                            "tokenAmount": {
                                "amount": "500000000",
                                "decimals": 6,
                                "uiAmount": 500.0,
                                "uiAmountString": "500"
                            }
                        }
                    },
                    "program": "spl-token",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"
                }]
            }
        }
    })";

    TransactionResponse tx = TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());
    auto activities = TxParseUtils::extractActivities(tx, WALLET);

    // "BurnAccountXXX" resolves to WALLET via postTokenBalances → involvesWallet
    ASSERT_EQ(activities.size(), 1);
    EXPECT_EQ(activities[0].activityType, "burn");
    EXPECT_EQ(activities[0].token, "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
    EXPECT_EQ(activities[0].fromAddress, WALLET); // resolved from token account
    EXPECT_TRUE(activities[0].toAddress.isEmpty());
    EXPECT_DOUBLE_EQ(activities[0].amount, 500.0);

    // Insert with empty toAddress (tests nullable column)
    ASSERT_TRUE(TransactionDb::insertTransaction("burn_sig_001", tx.slot, tx.blockTime, "{}", 5000,
                                                 false, activities));

    auto rows = TransactionDb::getTransactionsRecords({}, {}, "burn");
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].activityType, "burn");
}

TEST_F(TxParsingTest, ParseCloseAccountActivity) {
    const char* raw = R"({
        "blockTime": 1771700000, "slot": 402000000,
        "version": "legacy",
        "meta": {"fee": 5000, "err": null, "innerInstructions": [],
                 "preBalances": [], "postBalances": [], "logMessages": [],
                 "preTokenBalances": [], "postTokenBalances": []},
        "transaction": {
            "signatures": ["close_sig_001"],
            "message": {
                "accountKeys": [],
                "recentBlockhash": "CCCC",
                "instructions": [{
                    "parsed": {
                        "type": "closeAccount",
                        "info": {
                            "account": "ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL",
                            "destination": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm",
                            "owner": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"
                        }
                    },
                    "program": "spl-token",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"
                }]
            }
        }
    })";

    TransactionResponse tx = TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());
    auto activities = TxParseUtils::extractActivities(tx, WALLET);

    // destination == WALLET → involvesWallet
    ASSERT_EQ(activities.size(), 1);
    EXPECT_EQ(activities[0].activityType, "close_account");
    EXPECT_EQ(activities[0].toAddress, WALLET);
    EXPECT_DOUBLE_EQ(activities[0].amount, 0.0);

    ASSERT_TRUE(TransactionDb::insertTransaction("close_sig_001", tx.slot, tx.blockTime, "{}", 5000,
                                                 false, activities));

    auto rows = TransactionDb::getTransactionsRecords(WALLET, {}, "close_account");
    ASSERT_EQ(rows.size(), 1);
}

TEST_F(TxParsingTest, UnrecognizedInstructionProducesNoActivity) {
    // advanceNonce is not parsed — should produce zero activities
    const char* raw = R"({
        "blockTime": 1771700000, "slot": 402000000,
        "version": "legacy",
        "meta": {"fee": 5000, "err": null, "innerInstructions": [],
                 "preBalances": [], "postBalances": [], "logMessages": [],
                 "preTokenBalances": [], "postTokenBalances": []},
        "transaction": {
            "signatures": ["unknown_sig_001"],
            "message": {
                "accountKeys": [],
                "recentBlockhash": "DDDD",
                "instructions": [{
                    "parsed": {
                        "type": "advanceNonce",
                        "info": {
                            "nonceAccount": "2C8GAtCyZZLdRTReEUnMXcPXd6t5X56pijgmM1iKfQXQ",
                            "nonceAuthority": "6qRNVnNbrLF6UTCnfHdJ3vr31QtNPfVmjQ2BMZ3bozUm"
                        }
                    },
                    "program": "system",
                    "programId": "11111111111111111111111111111111"
                }]
            }
        }
    })";

    TransactionResponse tx = TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());
    auto activities = TxParseUtils::extractActivities(tx, WALLET);

    EXPECT_EQ(activities.size(), 0);
}

// ── Test: Atomic SOL send + SPL token receive ──────────────
// Regression test for: token transfers between ATAs were silently dropped
// because source/destination are token account addresses, not wallet addresses.
// The ownership resolution from pre/postTokenBalances must resolve ATAs to
// their owner wallets so the activity is captured.

TEST_F(TxParsingTest, AtomicSolSendAndTokenReceive) {
    // Real tx:
    // 5Lk8TZX1ABz4yBRq7sNK3wqe4LKxiBduSXREWh29L29m7UQkknxayN3rVRY1VMGArYqna8nyvMzKt86BYuwVfb2R
    // Wallet 7Ci2 sends 0.000422 SOL and receives 7,000 USDT in one atomic tx.
    const char* raw = R"({
        "blockTime": 1771901982,
        "meta": {
            "computeUnitsConsumed": 6499,
            "err": null,
            "fee": 422000,
            "innerInstructions": [],
            "logMessages": [],
            "postBalances": [411180528724, 5553295, 2039280, 2039280, 160707293622, 1, 1, 5569786489],
            "preBalances": [411181372724, 5131295, 2039280, 2039280, 160707293622, 1, 1, 5569786489],
            "postTokenBalances": [
                {
                    "accountIndex": 2,
                    "mint": "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB",
                    "owner": "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "0", "decimals": 6, "uiAmount": 0.0, "uiAmountString": "0"
                    }
                },
                {
                    "accountIndex": 3,
                    "mint": "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB",
                    "owner": "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "40271041268", "decimals": 6,
                        "uiAmount": 40271.041268, "uiAmountString": "40271.041268"
                    }
                }
            ],
            "preTokenBalances": [
                {
                    "accountIndex": 2,
                    "mint": "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB",
                    "owner": "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "7000000000", "decimals": 6,
                        "uiAmount": 7000.0, "uiAmountString": "7000"
                    }
                },
                {
                    "accountIndex": 3,
                    "mint": "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB",
                    "owner": "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                    "uiTokenAmount": {
                        "amount": "33271041268", "decimals": 6,
                        "uiAmount": 33271.041268, "uiAmountString": "33271.041268"
                    }
                }
            ],
            "rewards": [],
            "status": {"Ok": null}
        },
        "slot": 402319938,
        "transaction": {
            "message": {
                "accountKeys": [
                    {"pubkey": "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7",
                     "signer": true, "source": "transaction", "writable": true},
                    {"pubkey": "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR",
                     "signer": true, "source": "transaction", "writable": true},
                    {"pubkey": "vFYzzbm2jHd97tHScH6WEpeNBVTRGQQWF4fKVLeh8tu",
                     "signer": false, "source": "transaction", "writable": true},
                    {"pubkey": "EQ1mQrX7V5r3S5tj7tUbbFvS48gQGtcq5Jg5hMoRnUyN",
                     "signer": false, "source": "transaction", "writable": true},
                    {"pubkey": "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB",
                     "signer": false, "source": "transaction", "writable": false},
                    {"pubkey": "ComputeBudget111111111111111111111111111111",
                     "signer": false, "source": "transaction", "writable": false},
                    {"pubkey": "11111111111111111111111111111111",
                     "signer": false, "source": "transaction", "writable": false},
                    {"pubkey": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                     "signer": false, "source": "transaction", "writable": false}
                ],
                "instructions": [
                    {
                        "accounts": [],
                        "data": "3axL5qdEKYoR",
                        "programId": "ComputeBudget111111111111111111111111111111",
                        "stackHeight": 1
                    },
                    {
                        "parsed": {
                            "info": {
                                "destination": "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR",
                                "lamports": 422000,
                                "source": "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7"
                            },
                            "type": "transfer"
                        },
                        "program": "system",
                        "programId": "11111111111111111111111111111111",
                        "stackHeight": 1
                    },
                    {
                        "parsed": {
                            "info": {
                                "authority": "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR",
                                "destination": "EQ1mQrX7V5r3S5tj7tUbbFvS48gQGtcq5Jg5hMoRnUyN",
                                "mint": "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB",
                                "source": "vFYzzbm2jHd97tHScH6WEpeNBVTRGQQWF4fKVLeh8tu",
                                "tokenAmount": {
                                    "amount": "7000000000",
                                    "decimals": 6,
                                    "uiAmount": 7000.0,
                                    "uiAmountString": "7000"
                                }
                            },
                            "type": "transferChecked"
                        },
                        "program": "spl-token",
                        "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                        "stackHeight": 1
                    }
                ],
                "recentBlockhash": "2nHGNK55ENf9eAygmsR1NssLBqkFeXT66gUTTmpM8erP"
            },
            "signatures": [
                "5Lk8TZX1ABz4yBRq7sNK3wqe4LKxiBduSXREWh29L29m7UQkknxayN3rVRY1VMGArYqna8nyvMzKt86BYuwVfb2R",
                "3YxyD5FcqN92fvRtcnacEy6DjMmVLZrFYF85aziewdALc2kyCxJwAg1WiP8jL3oXPepQ7LSdUyT5PBuJmbMPcYkM"
            ]
        },
        "version": "legacy"
    })";

    const QString wallet = "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7";
    TransactionResponse tx = TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());
    auto activities = TxParseUtils::extractActivities(tx, wallet);

    // Should produce exactly 2 activities:
    //   1. SOL send (system transfer: wallet → 7tEz)
    //   2. USDT receive (transferChecked: vFYz→EQ1m, resolved to 7tEz→wallet)
    ASSERT_EQ(activities.size(), 2);

    // Activity 0: SOL send
    EXPECT_EQ(activities[0].activityType, "send");
    EXPECT_EQ(activities[0].token, "SOL");
    EXPECT_EQ(activities[0].fromAddress, wallet);
    EXPECT_EQ(activities[0].toAddress, "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR");
    EXPECT_NEAR(activities[0].amount, 0.000422, 1e-9);

    // Activity 1: USDT receive — the key regression test.
    // Before the fix, this was silently dropped because source/destination
    // were ATA addresses (vFYz/EQ1m), not wallet addresses.
    EXPECT_EQ(activities[1].activityType, "receive");
    EXPECT_EQ(activities[1].token, "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB");
    // Resolved via postTokenBalances: vFYz→7tEz (sender), EQ1m→7Ci2 (our wallet)
    EXPECT_EQ(activities[1].fromAddress, "7tEzHFqZNMYUndvbvEZSAa5R8vnE1ef357opsHbysbGR");
    EXPECT_EQ(activities[1].toAddress, wallet);
    EXPECT_DOUBLE_EQ(activities[1].amount, 7000.0);
}

// ── Test: SPL transfer between third parties is filtered out ──

TEST_F(TxParsingTest, ThirdPartyTokenTransferFiltered) {
    // A transferChecked where neither source nor destination is owned by our wallet.
    // This should produce zero activities.
    const char* raw = R"({
        "blockTime": 1771700000, "slot": 402000000,
        "version": "legacy",
        "meta": {"fee": 5000, "err": null, "innerInstructions": [],
                 "preBalances": [100000000, 2039280, 2039280],
                 "postBalances": [99995000, 2039280, 2039280],
                 "logMessages": [],
                 "preTokenBalances": [
                     {"accountIndex": 1,
                      "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                      "owner": "AliceXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                      "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                      "uiTokenAmount": {"amount": "1000000", "decimals": 6, "uiAmount": 1.0}},
                     {"accountIndex": 2,
                      "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                      "owner": "BobXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                      "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                      "uiTokenAmount": {"amount": "0", "decimals": 6, "uiAmount": 0.0}}
                 ],
                 "postTokenBalances": [
                     {"accountIndex": 1,
                      "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                      "owner": "AliceXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                      "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                      "uiTokenAmount": {"amount": "0", "decimals": 6, "uiAmount": 0.0}},
                     {"accountIndex": 2,
                      "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                      "owner": "BobXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                      "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA",
                      "uiTokenAmount": {"amount": "1000000", "decimals": 6, "uiAmount": 1.0}}
                 ]},
        "transaction": {
            "signatures": ["third_party_sig"],
            "message": {
                "accountKeys": [
                    {"pubkey": "AliceXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                     "signer": true, "source": "transaction", "writable": true},
                    {"pubkey": "AliceAtaXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                     "signer": false, "source": "transaction", "writable": true},
                    {"pubkey": "BobAtaXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                     "signer": false, "source": "transaction", "writable": true}
                ],
                "recentBlockhash": "EEEE",
                "instructions": [{
                    "parsed": {
                        "type": "transferChecked",
                        "info": {
                            "mint": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
                            "source": "AliceAtaXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                            "destination": "BobAtaXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                            "authority": "AliceXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
                            "tokenAmount": {
                                "amount": "1000000", "decimals": 6,
                                "uiAmount": 1.0, "uiAmountString": "1"
                            }
                        }
                    },
                    "program": "spl-token",
                    "programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"
                }]
            }
        }
    })";

    TransactionResponse tx = TransactionResponse::fromJson(QJsonDocument::fromJson(raw).object());
    // Our wallet is WALLET (6qRN...) — neither Alice nor Bob
    auto activities = TxParseUtils::extractActivities(tx, WALLET);

    EXPECT_EQ(activities.size(), 0) << "Third-party token transfer should be filtered out";
}
