#ifndef MCPDB_H
#define MCPDB_H

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

struct McpToolAccessRecord {
    QString toolName;
    int access = 0;
};

struct McpWalletToolAccessRecord {
    QString walletAddress;
    QString toolName;
    int access = 0;
};

struct McpPolicyRecord {
    int id = 0;
    QString name;
    QString apiKey;
    qint64 createdAt = 0;
    qint64 updatedAt = 0;
};

struct McpActivityRecord {
    qint64 id = 0;
    QString toolName;
    QString arguments;
    QString result;
    int durationMs = 0;
    bool success = false;
    QString walletAddress;
    int policyId = -1;
    qint64 createdAt = 0;
};

struct McpApprovalRecord {
    QString id;
    QString toolName;
    QString arguments;
    QString description;
    QString status;
    QString result;
    QString errorMsg;
    QString walletAddress;
    int policyId = -1;
    bool executed = false;
    qint64 createdAt = 0;
    qint64 resolvedAt = 0;
};

class QSqlDatabase;

class McpDb {
  public:
    // Access levels for tool control
    static constexpr int AccessBlocked = 0;
    static constexpr int AccessApproval = 1;
    static constexpr int AccessAllow = 2;

    // Named connection support for cinder-mcp subprocess
    static void setConnection(const QString& connectionName);

    // ── Global tool config ─────────────────────────────────
    static bool setToolEnabled(const QString& toolName, bool enabled);
    static bool isToolEnabled(const QString& toolName);
    static QList<McpToolAccessRecord> getAllToolConfigRecords();

    // ── Per-wallet tool access (legacy, still used by UI) ──
    static bool setWalletToolAccess(const QString& address, const QString& toolName, int access);
    static int walletToolAccess(const QString& address, const QString& toolName,
                                int defaultAccess = AccessBlocked);
    static QList<McpToolAccessRecord> getWalletToolConfigRecords(const QString& address);
    static QList<McpWalletToolAccessRecord> getAllWalletToolConfigRecords();

    // ── Access policies ─────────────────────────────────────────
    static int createPolicy(const QString& name, const QString& apiKey);
    static bool deletePolicy(int policyId);
    static bool renamePolicy(int policyId, const QString& name);
    static bool regenerateApiKey(int policyId, const QString& newKey);
    static std::optional<McpPolicyRecord> getPolicyByApiKeyRecord(const QString& apiKey);
    static std::optional<McpPolicyRecord> getPolicyRecord(int policyId);
    static QList<McpPolicyRecord> getAllPoliciesRecords();
    static int policyCount();

    // ── Policy-wallet bindings ──────────────────────────────────
    static bool addPolicyWallet(int policyId, const QString& address);
    static bool removePolicyWallet(int policyId, const QString& address);
    static QStringList getPolicyWallets(int policyId);

    // ── Policy tool access ──────────────────────────────────
    static bool setPolicyToolAccess(int policyId, const QString& toolName, int access);
    static int policyToolAccess(int policyId, const QString& toolName,
                                int defaultAccess = AccessBlocked);
    static QList<McpToolAccessRecord> getPolicyToolConfigRecords(int policyId);

    // ── Activity log ─────────────────────────────────────────
    static bool insertActivity(const QString& toolName, const QString& arguments,
                               const QString& result, int durationMs, bool success,
                               const QString& walletAddress = {}, int policyId = -1);
    static QList<McpActivityRecord> getRecentActivityRecords(int limit = 50);
    static int activityCount();
    static QList<McpActivityRecord> getActivityPageRecords(qint64 maxId, int limit);
    static int activityCountForWallet(const QString& walletAddress);
    static int activityCountForPolicy(int policyId);
    static bool clearActivityLog();

    // ── Pending approvals ────────────────────────────────────
    static bool insertPendingApproval(const QString& id, const QString& toolName,
                                      const QString& arguments, const QString& description,
                                      const QString& walletAddress = {}, int policyId = -1);
    static QList<McpApprovalRecord> getPendingApprovalsRecords();
    static int pendingApprovalCountForWallet(const QString& walletAddress);
    static int pendingApprovalCountForPolicy(int policyId);
    static bool resolveApproval(const QString& id, const QString& status,
                                const QString& result = {}, const QString& errorMsg = {});
    static std::optional<McpApprovalRecord> getApprovalRecord(const QString& id);
    static QList<McpApprovalRecord> getApprovedUnexecutedRecords();
    static bool markExecuted(const QString& id, const QString& result);

    // ── Policy name lookup (for UI) ────────────────────────────
    static QString policyNameById(int policyId);

  private:
    static QSqlDatabase db();
    static void ensureApiKeysHashed();
    static QString s_connectionName;
};

#endif // MCPDB_H
