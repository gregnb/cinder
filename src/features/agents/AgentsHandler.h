#ifndef AGENTSHANDLER_H
#define AGENTSHANDLER_H

#include "agents/McpToolDefs.h"
#include "db/McpDb.h"
#include "db/WalletDb.h"
#include "models/Agents.h"
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

class AgentsHandler {
  public:
    enum class AccessMode { Blocked = 0, Approval = 1, Allow = 2 };

    struct PolicyWalletBindingView {
        QString address;
        QString label;
        bool bound = false;
    };

    struct PolicyToolAccessView {
        QString name;
        QString description;
        AccessMode access = AccessMode::Blocked;
        bool fundRisk = false;
        McpToolCategory category = McpToolCategory::Read;
    };

    struct PolicyDetailView {
        int id = 0;
        QString name;
        QString apiKey;
        QList<PolicyWalletBindingView> wallets;
        QList<PolicyToolAccessView> tools;
    };

    QString mcpBinaryPath() const;

    QList<AgentPolicyCard> policyCards() const;
    std::optional<McpPolicyRecord> policyById(int policyId) const;
    std::optional<PolicyDetailView> buildPolicyDetail(int policyId) const;
    QString policyPermissionSummary(int policyId) const;

    int createPolicy(const QString& name) const;
    bool deletePolicy(int policyId) const;
    bool regeneratePolicyKey(int policyId) const;
    bool renamePolicy(int policyId, const QString& name) const;
    bool renamePolicyIfValid(int policyId, const QString& name) const;

    QStringList policyWallets(int policyId) const;
    QList<WalletSummaryRecord> wallets() const;
    bool setPolicyWalletBound(int policyId, const QString& address, bool bound) const;

    QMap<QString, int> policyToolConfigMap(int policyId) const;
    int policyToolAccess(int policyId, const QString& toolName) const;
    bool setPolicyToolAccess(int policyId, const QString& toolName, int access) const;
    void setPolicyAllTools(int policyId, int access) const;
    bool setPolicyToolAccessMode(int policyId, const QString& toolName, AccessMode mode) const;
    void setPolicyAllToolsMode(int policyId, AccessMode mode) const;

    QList<QString> fundRiskToolsNeedingAllow(int policyId) const;

    QList<McpApprovalRecord> pendingApprovals() const;
    QString policyNameById(int policyId) const;
    bool approve(const QString& approvalId) const;
    bool reject(const QString& approvalId) const;

    int activityCount() const;
    QList<McpActivityRecord> activityPage(qint64 maxId, int limit) const;
    bool clearActivityLog() const;
    QHash<int, QString> policyNameCache() const;

    static AccessMode accessModeFromInt(int access);
    static int accessModeToInt(AccessMode mode);

  private:
    QString runtimeApiKeyForPolicy(int policyId, const QString& storedApiKey) const;

    mutable QHash<int, QString> m_runtimePolicyKeys;
};

#endif // AGENTSHANDLER_H
