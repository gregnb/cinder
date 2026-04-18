#include "features/agents/AgentsHandler.h"

#include "agents/McpToolDefs.h"
#include "db/WalletDb.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

QString AgentsHandler::mcpBinaryPath() const {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/cinder-mcp");
}

QString AgentsHandler::runtimeApiKeyForPolicy(int policyId, const QString& storedApiKey) const {
    const auto it = m_runtimePolicyKeys.constFind(policyId);
    if (it != m_runtimePolicyKeys.constEnd()) {
        return it.value();
    }
    return storedApiKey;
}

QList<AgentPolicyCard> AgentsHandler::policyCards() const {
    QList<AgentPolicyCard> cards;
    const auto policies = McpDb::getAllPoliciesRecords();
    cards.reserve(policies.size());

    for (const auto& policy : policies) {
        AgentPolicyCard card;
        card.id = policy.id;
        card.name = policy.name;
        card.apiKey = runtimeApiKeyForPolicy(policy.id, policy.apiKey);
        card.createdAt = policy.createdAt;
        card.walletCount = McpDb::getPolicyWallets(policy.id).size();
        card.apiCalls = McpDb::activityCountForPolicy(policy.id);
        card.pending = McpDb::pendingApprovalCountForPolicy(policy.id);
        card.permissionSummary = policyPermissionSummary(policy.id);
        cards.append(card);
    }

    return cards;
}

std::optional<McpPolicyRecord> AgentsHandler::policyById(int policyId) const {
    return McpDb::getPolicyRecord(policyId);
}

std::optional<AgentsHandler::PolicyDetailView>
AgentsHandler::buildPolicyDetail(int policyId) const {
    const auto policy = McpDb::getPolicyRecord(policyId);
    if (!policy.has_value()) {
        return std::nullopt;
    }

    PolicyDetailView view;
    view.id = policyId;
    view.name = policy->name;
    view.apiKey = runtimeApiKeyForPolicy(policyId, policy->apiKey);

    const QStringList boundWallets = McpDb::getPolicyWallets(policyId);
    const auto allWallets = WalletDb::getAllRecords();
    view.wallets.reserve(allWallets.size());
    for (const auto& wallet : allWallets) {
        PolicyWalletBindingView row;
        row.address = wallet.address;
        row.label = wallet.label;
        row.bound = boundWallets.contains(wallet.address);
        view.wallets.append(row);
    }

    const auto config = policyToolConfigMap(policyId);
    const auto tools = allMcpTools();
    view.tools.reserve(tools.size());
    for (const auto& tool : tools) {
        PolicyToolAccessView row;
        row.name = QString::fromLatin1(tool.name);
        row.description = QString::fromLatin1(tool.description);
        row.fundRisk = tool.fundRisk;
        row.category = tool.category;
        row.access = config.contains(row.name) ? accessModeFromInt(config.value(row.name))
                                               : AccessMode::Blocked;
        view.tools.append(row);
    }

    return view;
}

QString AgentsHandler::policyPermissionSummary(int policyId) const {
    const auto rows = McpDb::getPolicyToolConfigRecords(policyId);
    QMap<QString, int> config;
    for (const auto& row : rows) {
        config[row.toolName] = row.access;
    }

    int allowCount = 0;
    int approvalCount = 0;
    int blockedCount = 0;

    for (const auto& tool : allMcpTools()) {
        const QString name = QString::fromLatin1(tool.name);
        const int access = config.contains(name) ? config[name] : McpDb::AccessBlocked;
        switch (access) {
            case McpDb::AccessAllow:
                ++allowCount;
                break;
            case McpDb::AccessApproval:
                ++approvalCount;
                break;
            default:
                ++blockedCount;
                break;
        }
    }

    QStringList parts;
    if (allowCount > 0) {
        parts << QCoreApplication::translate("AgentsPage", "%1 allow").arg(allowCount);
    }
    if (approvalCount > 0) {
        parts << QCoreApplication::translate("AgentsPage", "%1 approval").arg(approvalCount);
    }
    if (blockedCount > 0) {
        parts << QCoreApplication::translate("AgentsPage", "%1 blocked").arg(blockedCount);
    }
    return parts.join(QStringLiteral(" · "));
}

int AgentsHandler::createPolicy(const QString& name) const {
    QString policyName = name.trimmed();
    if (policyName.isEmpty()) {
        policyName = QStringLiteral("New Policy");
    }

    const QString apiKey =
        QStringLiteral("cinder_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int policyId = McpDb::createPolicy(policyName, apiKey);
    if (policyId >= 0) {
        m_runtimePolicyKeys.insert(policyId, apiKey);
    }
    return policyId;
}

bool AgentsHandler::deletePolicy(int policyId) const {
    m_runtimePolicyKeys.remove(policyId);
    return McpDb::deletePolicy(policyId);
}

bool AgentsHandler::regeneratePolicyKey(int policyId) const {
    const QString newKey =
        QStringLiteral("cinder_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!McpDb::regenerateApiKey(policyId, newKey)) {
        return false;
    }
    m_runtimePolicyKeys.insert(policyId, newKey);
    return true;
}

bool AgentsHandler::renamePolicy(int policyId, const QString& name) const {
    return McpDb::renamePolicy(policyId, name);
}

bool AgentsHandler::renamePolicyIfValid(int policyId, const QString& name) const {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    return McpDb::renamePolicy(policyId, trimmed);
}

QStringList AgentsHandler::policyWallets(int policyId) const {
    return McpDb::getPolicyWallets(policyId);
}

QList<WalletSummaryRecord> AgentsHandler::wallets() const { return WalletDb::getAllRecords(); }

bool AgentsHandler::setPolicyWalletBound(int policyId, const QString& address, bool bound) const {
    return bound ? McpDb::addPolicyWallet(policyId, address)
                 : McpDb::removePolicyWallet(policyId, address);
}

QMap<QString, int> AgentsHandler::policyToolConfigMap(int policyId) const {
    QMap<QString, int> config;
    const auto rows = McpDb::getPolicyToolConfigRecords(policyId);
    for (const auto& row : rows) {
        config[row.toolName] = row.access;
    }
    return config;
}

int AgentsHandler::policyToolAccess(int policyId, const QString& toolName) const {
    return McpDb::policyToolAccess(policyId, toolName);
}

bool AgentsHandler::setPolicyToolAccess(int policyId, const QString& toolName, int access) const {
    return McpDb::setPolicyToolAccess(policyId, toolName, access);
}

void AgentsHandler::setPolicyAllTools(int policyId, int access) const {
    const auto tools = allMcpTools();
    for (const auto& tool : tools) {
        McpDb::setPolicyToolAccess(policyId, QString::fromLatin1(tool.name), access);
    }
}

bool AgentsHandler::setPolicyToolAccessMode(int policyId, const QString& toolName,
                                            AccessMode mode) const {
    return McpDb::setPolicyToolAccess(policyId, toolName, accessModeToInt(mode));
}

void AgentsHandler::setPolicyAllToolsMode(int policyId, AccessMode mode) const {
    setPolicyAllTools(policyId, accessModeToInt(mode));
}

QList<QString> AgentsHandler::fundRiskToolsNeedingAllow(int policyId) const {
    QList<QString> names;
    for (const auto& tool : allMcpTools()) {
        if (!tool.fundRisk) {
            continue;
        }

        const QString name = QString::fromLatin1(tool.name);
        if (McpDb::policyToolAccess(policyId, name) != McpDb::AccessAllow) {
            names.append(name);
        }
    }
    return names;
}

QList<McpApprovalRecord> AgentsHandler::pendingApprovals() const {
    return McpDb::getPendingApprovalsRecords();
}

QString AgentsHandler::policyNameById(int policyId) const {
    return McpDb::policyNameById(policyId);
}

bool AgentsHandler::approve(const QString& approvalId) const {
    // Just mark as completed — the ApprovalExecutor picks up approved-but-unexecuted
    // records and performs the actual work (tx signing, contact ops, etc.)
    return McpDb::resolveApproval(approvalId, QStringLiteral("completed"));
}

bool AgentsHandler::reject(const QString& approvalId) const {
    return McpDb::resolveApproval(approvalId, QStringLiteral("rejected"), {},
                                  QStringLiteral("Rejected by user"));
}

int AgentsHandler::activityCount() const { return McpDb::activityCount(); }

QList<McpActivityRecord> AgentsHandler::activityPage(qint64 maxId, int limit) const {
    return McpDb::getActivityPageRecords(maxId, limit);
}

bool AgentsHandler::clearActivityLog() const { return McpDb::clearActivityLog(); }

QHash<int, QString> AgentsHandler::policyNameCache() const {
    QHash<int, QString> cache;
    const auto policies = McpDb::getAllPoliciesRecords();
    for (const auto& policy : policies) {
        cache.insert(policy.id, policy.name);
    }
    return cache;
}

AgentsHandler::AccessMode AgentsHandler::accessModeFromInt(int access) {
    switch (access) {
        case McpDb::AccessAllow:
            return AccessMode::Allow;
        case McpDb::AccessApproval:
            return AccessMode::Approval;
        default:
            return AccessMode::Blocked;
    }
}

int AgentsHandler::accessModeToInt(AccessMode mode) {
    switch (mode) {
        case AccessMode::Allow:
            return McpDb::AccessAllow;
        case AccessMode::Approval:
            return McpDb::AccessApproval;
        case AccessMode::Blocked:
            return McpDb::AccessBlocked;
    }
}
