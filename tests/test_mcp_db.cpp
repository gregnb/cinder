#include "TestMigrationUtils.h"
#include "db/Database.h"
#include "db/McpDb.h"
#include <QCoreApplication>
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

// ── Test fixture: fresh DB per test ──────────────────────

class McpDbTest : public ::testing::Test {
  protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
            QSqlDatabase::database().close();
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
        }

        ASSERT_TRUE(m_tempDir.isValid());
        QString dbPath = m_tempDir.path() + "/test_mcp.db";

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

    // Helper: create a policy and return its ID
    int createTestPolicy(const QString& name = "TestPolicy",
                         const QString& apiKey = "test_key_123") {
        return McpDb::createPolicy(name, apiKey);
    }

    QString storedApiKeyForPolicy(int policyId) {
        QSqlQuery q(QSqlDatabase::database());
        q.prepare("SELECT api_key FROM mcp_access_policies WHERE id = :id");
        q.bindValue(":id", policyId);
        if (!q.exec() || !q.next()) {
            return {};
        }
        return q.value(0).toString();
    }
};

// ══════════════════════════════════════════════════════════
// Policy CRUD
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, CreatePolicy) {
    int id = createTestPolicy("MyPolicy", "key_abc");
    EXPECT_GT(id, 0);

    auto record = McpDb::getPolicyRecord(id);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->name, "MyPolicy");
    EXPECT_TRUE(record->apiKey.isEmpty());
    EXPECT_GT(record->createdAt, 0);
    EXPECT_TRUE(storedApiKeyForPolicy(id).startsWith("sha256:"));
    EXPECT_NE(storedApiKeyForPolicy(id), "key_abc");
}

TEST_F(McpDbTest, GetPolicyByApiKey) {
    int id = createTestPolicy("LookupPolicy", "lookup_key_999");

    auto record = McpDb::getPolicyByApiKeyRecord("lookup_key_999");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->id, id);
    EXPECT_EQ(record->name, "LookupPolicy");
}

TEST_F(McpDbTest, GetPolicyByApiKeyInvalid) {
    auto record = McpDb::getPolicyByApiKeyRecord("nonexistent_key");
    EXPECT_FALSE(record.has_value());
}

TEST_F(McpDbTest, RenamePolicy) {
    int id = createTestPolicy("OldName", "key_rename");
    EXPECT_TRUE(McpDb::renamePolicy(id, "NewName"));

    auto record = McpDb::getPolicyRecord(id);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->name, "NewName");
}

TEST_F(McpDbTest, RegenerateApiKey) {
    int id = createTestPolicy("RegenPolicy", "old_key");
    EXPECT_TRUE(McpDb::regenerateApiKey(id, "new_key"));

    auto record = McpDb::getPolicyRecord(id);
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(record->apiKey.isEmpty());
    EXPECT_TRUE(storedApiKeyForPolicy(id).startsWith("sha256:"));
    EXPECT_NE(storedApiKeyForPolicy(id), "new_key");

    // Old key should no longer resolve
    auto byOld = McpDb::getPolicyByApiKeyRecord("old_key");
    EXPECT_FALSE(byOld.has_value());

    // New key should resolve
    auto byNew = McpDb::getPolicyByApiKeyRecord("new_key");
    ASSERT_TRUE(byNew.has_value());
    EXPECT_EQ(byNew->id, id);
}

TEST_F(McpDbTest, LegacyPlaintextApiKeysAreMigratedOnRead) {
    QSqlQuery insert(QSqlDatabase::database());
    insert.prepare("INSERT INTO mcp_access_policies (name, api_key, created_at, updated_at) "
                   "VALUES (:name, :key, :created_at, :updated_at)");
    insert.bindValue(":name", "LegacyPolicy");
    insert.bindValue(":key", "legacy_plaintext_key");
    insert.bindValue(":created_at", 100);
    insert.bindValue(":updated_at", 100);
    ASSERT_TRUE(insert.exec());
    const int policyId = insert.lastInsertId().toInt();

    auto record = McpDb::getPolicyByApiKeyRecord("legacy_plaintext_key");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->id, policyId);
    EXPECT_TRUE(record->apiKey.isEmpty());
    EXPECT_TRUE(storedApiKeyForPolicy(policyId).startsWith("sha256:"));
    EXPECT_NE(storedApiKeyForPolicy(policyId), "legacy_plaintext_key");
}

TEST_F(McpDbTest, DeletePolicy) {
    int id = createTestPolicy("DeleteMe", "key_del");
    EXPECT_TRUE(McpDb::deletePolicy(id));

    auto record = McpDb::getPolicyRecord(id);
    EXPECT_FALSE(record.has_value());
    EXPECT_EQ(McpDb::policyCount(), 0);
}

TEST_F(McpDbTest, GetAllPolicies) {
    createTestPolicy("Policy1", "key_1");
    createTestPolicy("Policy2", "key_2");
    createTestPolicy("Policy3", "key_3");

    auto all = McpDb::getAllPoliciesRecords();
    EXPECT_EQ(all.size(), 3);
    EXPECT_EQ(McpDb::policyCount(), 3);
}

// ══════════════════════════════════════════════════════════
// Policy-Wallet Bindings
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, AddAndGetPolicyWallets) {
    int id = createTestPolicy();
    EXPECT_TRUE(McpDb::addPolicyWallet(id, "WalletAddr1"));
    EXPECT_TRUE(McpDb::addPolicyWallet(id, "WalletAddr2"));

    auto wallets = McpDb::getPolicyWallets(id);
    EXPECT_EQ(wallets.size(), 2);
    EXPECT_TRUE(wallets.contains("WalletAddr1"));
    EXPECT_TRUE(wallets.contains("WalletAddr2"));
}

TEST_F(McpDbTest, RemovePolicyWallet) {
    int id = createTestPolicy();
    McpDb::addPolicyWallet(id, "WalletAddr1");
    McpDb::addPolicyWallet(id, "WalletAddr2");

    EXPECT_TRUE(McpDb::removePolicyWallet(id, "WalletAddr1"));

    auto wallets = McpDb::getPolicyWallets(id);
    EXPECT_EQ(wallets.size(), 1);
    EXPECT_EQ(wallets.first(), "WalletAddr2");
}

TEST_F(McpDbTest, PolicyWalletsEmptyByDefault) {
    int id = createTestPolicy();
    auto wallets = McpDb::getPolicyWallets(id);
    EXPECT_TRUE(wallets.isEmpty());
}

// ══════════════════════════════════════════════════════════
// Policy Tool Access — Blocked / Approval / Allow
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, DefaultAccessIsBlocked) {
    int id = createTestPolicy();
    int access = McpDb::policyToolAccess(id, "wallet_get_balance");
    EXPECT_EQ(access, McpDb::AccessBlocked);
}

TEST_F(McpDbTest, SetAccessBlocked) {
    int id = createTestPolicy();
    EXPECT_TRUE(McpDb::setPolicyToolAccess(id, "wallet_send_sol", McpDb::AccessBlocked));

    int access = McpDb::policyToolAccess(id, "wallet_send_sol");
    EXPECT_EQ(access, McpDb::AccessBlocked);
}

TEST_F(McpDbTest, SetAccessApproval) {
    int id = createTestPolicy();
    EXPECT_TRUE(McpDb::setPolicyToolAccess(id, "wallet_send_sol", McpDb::AccessApproval));

    int access = McpDb::policyToolAccess(id, "wallet_send_sol");
    EXPECT_EQ(access, McpDb::AccessApproval);
}

TEST_F(McpDbTest, SetAccessAllow) {
    int id = createTestPolicy();
    EXPECT_TRUE(McpDb::setPolicyToolAccess(id, "wallet_send_sol", McpDb::AccessAllow));

    int access = McpDb::policyToolAccess(id, "wallet_send_sol");
    EXPECT_EQ(access, McpDb::AccessAllow);
}

TEST_F(McpDbTest, ChangeAccessLevel) {
    int id = createTestPolicy();

    // Start blocked → approval → allow → blocked
    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessBlocked);
    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_swap"), McpDb::AccessBlocked);

    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessApproval);
    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_swap"), McpDb::AccessApproval);

    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessAllow);
    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_swap"), McpDb::AccessAllow);

    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessBlocked);
    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_swap"), McpDb::AccessBlocked);
}

TEST_F(McpDbTest, MultipleToolsIndependent) {
    int id = createTestPolicy();

    McpDb::setPolicyToolAccess(id, "wallet_send_sol", McpDb::AccessAllow);
    McpDb::setPolicyToolAccess(id, "wallet_get_balance", McpDb::AccessApproval);
    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessBlocked);

    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_send_sol"), McpDb::AccessAllow);
    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_get_balance"), McpDb::AccessApproval);
    EXPECT_EQ(McpDb::policyToolAccess(id, "wallet_swap"), McpDb::AccessBlocked);
}

TEST_F(McpDbTest, GetPolicyToolConfigRecords) {
    int id = createTestPolicy();

    McpDb::setPolicyToolAccess(id, "wallet_send_sol", McpDb::AccessAllow);
    McpDb::setPolicyToolAccess(id, "wallet_get_balance", McpDb::AccessApproval);
    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessBlocked);

    auto records = McpDb::getPolicyToolConfigRecords(id);
    EXPECT_EQ(records.size(), 3);

    // Build a lookup map for verification
    QMap<QString, int> accessMap;
    for (const auto& r : records) {
        accessMap[r.toolName] = r.access;
    }
    EXPECT_EQ(accessMap["wallet_send_sol"], McpDb::AccessAllow);
    EXPECT_EQ(accessMap["wallet_get_balance"], McpDb::AccessApproval);
    EXPECT_EQ(accessMap["wallet_swap"], McpDb::AccessBlocked);
}

TEST_F(McpDbTest, PoliciesHaveIsolatedAccess) {
    int id1 = createTestPolicy("Policy1", "key_1");
    int id2 = createTestPolicy("Policy2", "key_2");

    McpDb::setPolicyToolAccess(id1, "wallet_send_sol", McpDb::AccessAllow);
    McpDb::setPolicyToolAccess(id2, "wallet_send_sol", McpDb::AccessBlocked);

    EXPECT_EQ(McpDb::policyToolAccess(id1, "wallet_send_sol"), McpDb::AccessAllow);
    EXPECT_EQ(McpDb::policyToolAccess(id2, "wallet_send_sol"), McpDb::AccessBlocked);
}

TEST_F(McpDbTest, DefaultAccessParamRespected) {
    int id = createTestPolicy();
    // No access set — should return the default parameter value
    int access = McpDb::policyToolAccess(id, "wallet_unknown_tool", McpDb::AccessAllow);
    EXPECT_EQ(access, McpDb::AccessAllow);
}

// ══════════════════════════════════════════════════════════
// Global Tool Config
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, GlobalToolEnabledByDefault) {
    // Tools not explicitly set should be considered enabled
    EXPECT_TRUE(McpDb::isToolEnabled("wallet_send_sol"));
}

TEST_F(McpDbTest, DisableAndEnableGlobalTool) {
    EXPECT_TRUE(McpDb::setToolEnabled("wallet_send_sol", false));
    EXPECT_FALSE(McpDb::isToolEnabled("wallet_send_sol"));

    EXPECT_TRUE(McpDb::setToolEnabled("wallet_send_sol", true));
    EXPECT_TRUE(McpDb::isToolEnabled("wallet_send_sol"));
}

TEST_F(McpDbTest, GetAllGlobalToolConfigs) {
    McpDb::setToolEnabled("wallet_send_sol", false);
    McpDb::setToolEnabled("wallet_swap", true);

    auto configs = McpDb::getAllToolConfigRecords();
    EXPECT_GE(configs.size(), 2);
}

// ══════════════════════════════════════════════════════════
// Activity Log
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, InsertAndRetrieveActivity) {
    int policyId = createTestPolicy();

    EXPECT_TRUE(McpDb::insertActivity("wallet_get_balance", "{}", "{\"balance\":100}", 42, true,
                                      "WalletAddr", policyId));

    auto activities = McpDb::getRecentActivityRecords(10);
    ASSERT_EQ(activities.size(), 1);
    EXPECT_EQ(activities[0].toolName, "wallet_get_balance");
    EXPECT_EQ(activities[0].durationMs, 42);
    EXPECT_TRUE(activities[0].success);
    EXPECT_EQ(activities[0].walletAddress, "WalletAddr");
    EXPECT_EQ(activities[0].policyId, policyId);
}

TEST_F(McpDbTest, ActivityCount) {
    int policyId = createTestPolicy();

    McpDb::insertActivity("wallet_get_balance", "{}", "{}", 10, true, "W1", policyId);
    McpDb::insertActivity("wallet_send_sol", "{}", "{}", 20, false, "W1", policyId);
    McpDb::insertActivity("wallet_swap", "{}", "{}", 30, true, "W2", policyId);

    EXPECT_EQ(McpDb::activityCount(), 3);
    EXPECT_EQ(McpDb::activityCountForWallet("W1"), 2);
    EXPECT_EQ(McpDb::activityCountForWallet("W2"), 1);
    EXPECT_EQ(McpDb::activityCountForPolicy(policyId), 3);
}

TEST_F(McpDbTest, ClearActivityLog) {
    int policyId = createTestPolicy();
    McpDb::insertActivity("wallet_get_balance", "{}", "{}", 10, true, "W1", policyId);
    McpDb::insertActivity("wallet_send_sol", "{}", "{}", 20, true, "W1", policyId);

    EXPECT_EQ(McpDb::activityCount(), 2);
    EXPECT_TRUE(McpDb::clearActivityLog());
    EXPECT_EQ(McpDb::activityCount(), 0);
}

// ══════════════════════════════════════════════════════════
// Pending Approvals
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, InsertAndGetPendingApproval) {
    int policyId = createTestPolicy();

    EXPECT_TRUE(McpDb::insertPendingApproval("approval-001", "wallet_send_sol",
                                             "{\"to\":\"abc\",\"amount\":1.0}", "Send 1 SOL to abc",
                                             "WalletAddr", policyId));

    auto pending = McpDb::getPendingApprovalsRecords();
    ASSERT_EQ(pending.size(), 1);
    EXPECT_EQ(pending[0].id, "approval-001");
    EXPECT_EQ(pending[0].toolName, "wallet_send_sol");
    EXPECT_EQ(pending[0].status, "pending");
    EXPECT_EQ(pending[0].policyId, policyId);
}

TEST_F(McpDbTest, ResolveApprovalCompleted) {
    int policyId = createTestPolicy();
    McpDb::insertPendingApproval("approval-002", "wallet_send_sol", "{}", "Send SOL", "W1",
                                 policyId);

    EXPECT_TRUE(McpDb::resolveApproval("approval-002", "completed", "{\"txid\":\"abc123\"}"));

    auto record = McpDb::getApprovalRecord("approval-002");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, "completed");
    EXPECT_EQ(record->result, "{\"txid\":\"abc123\"}");
    EXPECT_GT(record->resolvedAt, 0);
}

TEST_F(McpDbTest, ResolveApprovalRejected) {
    int policyId = createTestPolicy();
    McpDb::insertPendingApproval("approval-003", "wallet_swap", "{}", "Swap tokens", "W1",
                                 policyId);

    EXPECT_TRUE(McpDb::resolveApproval("approval-003", "rejected", "", "User declined"));

    auto record = McpDb::getApprovalRecord("approval-003");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, "rejected");
    EXPECT_EQ(record->errorMsg, "User declined");
}

TEST_F(McpDbTest, PendingApprovalCounts) {
    int p1 = createTestPolicy("P1", "key_1");
    int p2 = createTestPolicy("P2", "key_2");

    McpDb::insertPendingApproval("a1", "wallet_send_sol", "{}", "desc", "W1", p1);
    McpDb::insertPendingApproval("a2", "wallet_swap", "{}", "desc", "W1", p1);
    McpDb::insertPendingApproval("a3", "wallet_send_sol", "{}", "desc", "W2", p2);

    EXPECT_EQ(McpDb::pendingApprovalCountForWallet("W1"), 2);
    EXPECT_EQ(McpDb::pendingApprovalCountForWallet("W2"), 1);
    EXPECT_EQ(McpDb::pendingApprovalCountForPolicy(p1), 2);
    EXPECT_EQ(McpDb::pendingApprovalCountForPolicy(p2), 1);
}

// ══════════════════════════════════════════════════════════
// Policy name lookup
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, PolicyNameById) {
    int id = createTestPolicy("NamedPolicy", "key_named");
    EXPECT_EQ(McpDb::policyNameById(id), "NamedPolicy");
    EXPECT_EQ(McpDb::policyNameById(99999), "");
}

// ══════════════════════════════════════════════════════════
// Delete policy cascades
// ══════════════════════════════════════════════════════════

// ══════════════════════════════════════════════════════════
// Execution Pipeline (executed column)
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, ApprovalNotExecutedByDefault) {
    int policyId = createTestPolicy();
    McpDb::insertPendingApproval("exec-001", "wallet_send_sol", "{}", "Send SOL", "W1", policyId);

    auto record = McpDb::getApprovalRecord("exec-001");
    ASSERT_TRUE(record.has_value());
    EXPECT_FALSE(record->executed);
}

TEST_F(McpDbTest, ResolvedApprovalStillNotExecuted) {
    int policyId = createTestPolicy();
    McpDb::insertPendingApproval("exec-002", "wallet_send_sol", "{}", "Send SOL", "W1", policyId);
    McpDb::resolveApproval("exec-002", "completed");

    auto record = McpDb::getApprovalRecord("exec-002");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, "completed");
    EXPECT_FALSE(record->executed);
}

TEST_F(McpDbTest, GetApprovedUnexecutedRecords) {
    int policyId = createTestPolicy();

    // One pending, one completed-unexecuted, one rejected
    McpDb::insertPendingApproval("exec-010", "wallet_send_sol", "{}", "desc", "W1", policyId);
    McpDb::insertPendingApproval("exec-011", "wallet_add_contact", "{}", "desc", "W1", policyId);
    McpDb::insertPendingApproval("exec-012", "wallet_swap", "{}", "desc", "W1", policyId);

    McpDb::resolveApproval("exec-011", "completed");
    McpDb::resolveApproval("exec-012", "rejected");

    auto unexecuted = McpDb::getApprovedUnexecutedRecords();
    ASSERT_EQ(unexecuted.size(), 1);
    EXPECT_EQ(unexecuted[0].id, "exec-011");
    EXPECT_EQ(unexecuted[0].toolName, "wallet_add_contact");
}

TEST_F(McpDbTest, MarkExecutedSetsFlag) {
    int policyId = createTestPolicy();
    McpDb::insertPendingApproval("exec-020", "wallet_send_sol", "{\"to\":\"abc\"}", "Send SOL",
                                 "W1", policyId);
    McpDb::resolveApproval("exec-020", "completed");

    EXPECT_TRUE(McpDb::markExecuted("exec-020", "{\"signature\":\"txhash123\"}"));

    auto record = McpDb::getApprovalRecord("exec-020");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(record->executed);
    EXPECT_EQ(record->result, "{\"signature\":\"txhash123\"}");
}

TEST_F(McpDbTest, MarkExecutedRemovesFromUnexecutedList) {
    int policyId = createTestPolicy();
    McpDb::insertPendingApproval("exec-030", "wallet_send_sol", "{}", "desc", "W1", policyId);
    McpDb::insertPendingApproval("exec-031", "wallet_add_contact", "{}", "desc", "W1", policyId);

    McpDb::resolveApproval("exec-030", "completed");
    McpDb::resolveApproval("exec-031", "completed");

    auto before = McpDb::getApprovedUnexecutedRecords();
    EXPECT_EQ(before.size(), 2);

    McpDb::markExecuted("exec-030", "{\"status\":\"completed\"}");

    auto after = McpDb::getApprovedUnexecutedRecords();
    ASSERT_EQ(after.size(), 1);
    EXPECT_EQ(after[0].id, "exec-031");
}

TEST_F(McpDbTest, MultipleApprovedUnexecutedOrderedByCreatedAt) {
    int policyId = createTestPolicy();

    McpDb::insertPendingApproval("exec-040", "wallet_send_sol", "{}", "first", "W1", policyId);
    McpDb::insertPendingApproval("exec-041", "wallet_add_contact", "{}", "second", "W1", policyId);
    McpDb::insertPendingApproval("exec-042", "wallet_send_token", "{}", "third", "W1", policyId);

    McpDb::resolveApproval("exec-040", "completed");
    McpDb::resolveApproval("exec-041", "completed");
    McpDb::resolveApproval("exec-042", "completed");

    auto records = McpDb::getApprovedUnexecutedRecords();
    ASSERT_EQ(records.size(), 3);
    EXPECT_EQ(records[0].id, "exec-040");
    EXPECT_EQ(records[1].id, "exec-041");
    EXPECT_EQ(records[2].id, "exec-042");
}

// ══════════════════════════════════════════════════════════
// Delete policy cascades
// ══════════════════════════════════════════════════════════

TEST_F(McpDbTest, DeletePolicyCascadesWalletsAndAccess) {
    int id = createTestPolicy("CascadePolicy", "key_cascade");

    McpDb::addPolicyWallet(id, "W1");
    McpDb::addPolicyWallet(id, "W2");
    McpDb::setPolicyToolAccess(id, "wallet_send_sol", McpDb::AccessAllow);
    McpDb::setPolicyToolAccess(id, "wallet_swap", McpDb::AccessApproval);

    EXPECT_EQ(McpDb::getPolicyWallets(id).size(), 2);
    EXPECT_EQ(McpDb::getPolicyToolConfigRecords(id).size(), 2);

    EXPECT_TRUE(McpDb::deletePolicy(id));

    // Everything should be gone
    EXPECT_FALSE(McpDb::getPolicyRecord(id).has_value());
    EXPECT_TRUE(McpDb::getPolicyWallets(id).isEmpty());
    EXPECT_TRUE(McpDb::getPolicyToolConfigRecords(id).isEmpty());
}
