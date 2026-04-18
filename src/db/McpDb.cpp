#include "McpDb.h"
#include "DbUtil.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QVector>

namespace {

    QString hashApiKey(const QString& apiKey) {
        const QByteArray digest =
            QCryptographicHash::hash(apiKey.toUtf8(), QCryptographicHash::Sha256).toHex();
        return QStringLiteral("sha256:") + QString::fromLatin1(digest);
    }

    bool isHashedApiKey(const QString& apiKey) {
        return apiKey.startsWith(QStringLiteral("sha256:"));
    }

    McpPolicyRecord policyFromQuery(const QSqlQuery& q) {
        McpPolicyRecord r;
        r.id = q.value("id").toInt();
        r.name = q.value("name").toString();
        const QString storedApiKey = q.value("api_key").toString();
        r.apiKey = isHashedApiKey(storedApiKey) ? QString{} : storedApiKey;
        r.createdAt = q.value("created_at").toLongLong();
        r.updatedAt = q.value("updated_at").toLongLong();
        return r;
    }

    McpToolAccessRecord toolAccessFromQuery(const QSqlQuery& q, const char* accessKey) {
        McpToolAccessRecord r;
        r.toolName = q.value("tool_name").toString();
        r.access = q.value(accessKey).toInt();
        return r;
    }

    McpWalletToolAccessRecord walletToolAccessFromQuery(const QSqlQuery& q, const char* accessKey) {
        McpWalletToolAccessRecord r;
        r.walletAddress = q.value("wallet_address").toString();
        r.toolName = q.value("tool_name").toString();
        r.access = q.value(accessKey).toInt();
        return r;
    }

    McpActivityRecord activityFromQuery(const QSqlQuery& q) {
        McpActivityRecord r;
        r.id = q.value("id").toLongLong();
        r.toolName = q.value("tool_name").toString();
        r.arguments = q.value("arguments").toString();
        r.result = q.value("result").toString();
        r.durationMs = q.value("duration_ms").toInt();
        r.success = q.value("success").toInt() != 0;
        r.walletAddress = q.value("wallet_address").toString();
        r.policyId = q.value("policy_id").isNull() ? -1 : q.value("policy_id").toInt();
        r.createdAt = q.value("created_at").toLongLong();
        return r;
    }

    McpApprovalRecord approvalFromQuery(const QSqlQuery& q) {
        McpApprovalRecord r;
        r.id = q.value("id").toString();
        r.toolName = q.value("tool_name").toString();
        r.arguments = q.value("arguments").toString();
        r.description = q.value("description").toString();
        r.status = q.value("status").toString();
        r.result = q.value("result").toString();
        r.errorMsg = q.value("error_msg").toString();
        r.walletAddress = q.value("wallet_address").toString();
        r.policyId = q.value("policy_id").isNull() ? -1 : q.value("policy_id").toInt();
        r.executed = q.value("executed").toInt() != 0;
        r.createdAt = q.value("created_at").toLongLong();
        r.resolvedAt = q.value("resolved_at").isNull() ? 0 : q.value("resolved_at").toLongLong();
        return r;
    }

} // namespace

QString McpDb::s_connectionName;

void McpDb::setConnection(const QString& connectionName) { s_connectionName = connectionName; }

QSqlDatabase McpDb::db() {
    if (s_connectionName.isEmpty()) {
        return QSqlDatabase::database();
    }
    return QSqlDatabase::database(s_connectionName);
}

void McpDb::ensureApiKeysHashed() {
    QSqlDatabase database = db();
    if (!database.isValid() || !database.isOpen()) {
        return;
    }

    QSqlQuery selectQuery(database);
    if (!selectQuery.exec(QStringLiteral("SELECT id, api_key FROM mcp_access_policies"))) {
        return;
    }

    QVector<QPair<int, QString>> pendingUpdates;
    while (selectQuery.next()) {
        const int policyId = selectQuery.value("id").toInt();
        const QString apiKey = selectQuery.value("api_key").toString();
        if (!apiKey.isEmpty() && !isHashedApiKey(apiKey)) {
            pendingUpdates.append({policyId, hashApiKey(apiKey)});
        }
    }

    if (pendingUpdates.isEmpty()) {
        return;
    }

    if (!database.transaction()) {
        return;
    }

    QSqlQuery updateQuery(database);
    if (!updateQuery.prepare(
            QStringLiteral("UPDATE mcp_access_policies SET api_key = :key WHERE id = :id"))) {
        database.rollback();
        return;
    }

    for (const auto& update : pendingUpdates) {
        updateQuery.bindValue(":key", update.second);
        updateQuery.bindValue(":id", update.first);
        if (!updateQuery.exec()) {
            database.rollback();
            return;
        }
    }

    database.commit();
}

// ── Global tool config ──────────────────────────────────────────

bool McpDb::setToolEnabled(const QString& toolName, bool enabled) {
    static const QString kSql = R"(
        INSERT INTO mcp_tool_config (tool_name, enabled, updated_at)
        VALUES (:name, :enabled, :ts)
        ON CONFLICT(tool_name) DO UPDATE SET
            enabled = :enabled2,
            updated_at = :ts2
    )";

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return DbUtil::exec(db(), kSql,
                        {{":name", toolName},
                         {":enabled", enabled ? 1 : 0},
                         {":ts", now},
                         {":enabled2", enabled ? 1 : 0},
                         {":ts2", now}});
}

bool McpDb::isToolEnabled(const QString& toolName) {
    return DbUtil::scalarInt(db(),
                             "SELECT enabled FROM mcp_tool_config WHERE tool_name = :name LIMIT 1",
                             {{":name", toolName}})
               .value_or(1) != 0;
}

QList<McpToolAccessRecord> McpDb::getAllToolConfigRecords() {
    return DbUtil::many<McpToolAccessRecord>(
        db(), "SELECT tool_name, enabled FROM mcp_tool_config ORDER BY tool_name", {},
        [](const QSqlQuery& q) { return toolAccessFromQuery(q, "enabled"); });
}

// ── Per-wallet tool access ──────────────────────────────────────

bool McpDb::setWalletToolAccess(const QString& address, const QString& toolName, int access) {
    static const QString kSql = R"(
        INSERT INTO mcp_wallet_tool_access (wallet_address, tool_name, enabled, updated_at)
        VALUES (:addr, :name, :access, :ts)
        ON CONFLICT(wallet_address, tool_name) DO UPDATE SET
            enabled = :access2,
            updated_at = :ts2
    )";

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return DbUtil::exec(db(), kSql,
                        {{":addr", address},
                         {":name", toolName},
                         {":access", access},
                         {":ts", now},
                         {":access2", access},
                         {":ts2", now}});
}

int McpDb::walletToolAccess(const QString& address, const QString& toolName, int defaultAccess) {
    return DbUtil::scalarInt(db(),
                             "SELECT enabled FROM mcp_wallet_tool_access "
                             "WHERE wallet_address = :addr AND tool_name = :name LIMIT 1",
                             {{":addr", address}, {":name", toolName}})
        .value_or(defaultAccess);
}

QList<McpToolAccessRecord> McpDb::getWalletToolConfigRecords(const QString& address) {
    return DbUtil::many<McpToolAccessRecord>(
        db(),
        "SELECT tool_name, enabled FROM mcp_wallet_tool_access "
        "WHERE wallet_address = :addr ORDER BY tool_name",
        {{":addr", address}}, [](const QSqlQuery& q) { return toolAccessFromQuery(q, "enabled"); });
}

QList<McpWalletToolAccessRecord> McpDb::getAllWalletToolConfigRecords() {
    return DbUtil::many<McpWalletToolAccessRecord>(
        db(),
        "SELECT wallet_address, tool_name, enabled FROM mcp_wallet_tool_access "
        "ORDER BY wallet_address, tool_name",
        {}, [](const QSqlQuery& q) { return walletToolAccessFromQuery(q, "enabled"); });
}

// ── Access policies ─────────────────────────────────────────────

int McpDb::createPolicy(const QString& name, const QString& apiKey) {
    static const QString kSql = R"(
        INSERT INTO mcp_access_policies (name, api_key, created_at, updated_at)
        VALUES (:name, :key, :ts, :ts2)
    )";

    QSqlQuery q(db());
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (!DbUtil::prepareBindExec(
            q, kSql,
            {{":name", name}, {":key", hashApiKey(apiKey)}, {":ts", now}, {":ts2", now}})) {
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool McpDb::deletePolicy(int policyId) {
    ensureApiKeysHashed();
    return DbUtil::exec(db(), "DELETE FROM mcp_access_policies WHERE id = :id", {{":id", policyId}},
                        DbUtil::RequireRows::Yes);
}

bool McpDb::renamePolicy(int policyId, const QString& name) {
    ensureApiKeysHashed();
    return DbUtil::exec(
        db(), "UPDATE mcp_access_policies SET name = :name, updated_at = :ts WHERE id = :id",
        {{":name", name}, {":ts", QDateTime::currentSecsSinceEpoch()}, {":id", policyId}},
        DbUtil::RequireRows::Yes);
}

bool McpDb::regenerateApiKey(int policyId, const QString& newKey) {
    ensureApiKeysHashed();
    return DbUtil::exec(
        db(), "UPDATE mcp_access_policies SET api_key = :key, updated_at = :ts WHERE id = :id",
        {{":key", hashApiKey(newKey)},
         {":ts", QDateTime::currentSecsSinceEpoch()},
         {":id", policyId}},
        DbUtil::RequireRows::Yes);
}

std::optional<McpPolicyRecord> McpDb::getPolicyByApiKeyRecord(const QString& apiKey) {
    ensureApiKeysHashed();
    return DbUtil::one<McpPolicyRecord>(
        db(), "SELECT * FROM mcp_access_policies WHERE api_key = :key LIMIT 1",
        {{":key", hashApiKey(apiKey)}}, policyFromQuery);
}

std::optional<McpPolicyRecord> McpDb::getPolicyRecord(int policyId) {
    ensureApiKeysHashed();
    return DbUtil::one<McpPolicyRecord>(db(),
                                        "SELECT * FROM mcp_access_policies WHERE id = :id LIMIT 1",
                                        {{":id", policyId}}, policyFromQuery);
}

QList<McpPolicyRecord> McpDb::getAllPoliciesRecords() {
    ensureApiKeysHashed();
    return DbUtil::many<McpPolicyRecord>(
        db(), "SELECT * FROM mcp_access_policies ORDER BY created_at ASC", {}, policyFromQuery);
}

int McpDb::policyCount() {
    ensureApiKeysHashed();
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM mcp_access_policies").value_or(0);
}

QString McpDb::policyNameById(int policyId) {
    ensureApiKeysHashed();
    if (policyId < 0) {
        return {};
    }

    return DbUtil::scalarString(db(), "SELECT name FROM mcp_access_policies WHERE id = :id LIMIT 1",
                                {{":id", policyId}})
        .value_or(QString{});
}

// ── Policy-wallet bindings ──────────────────────────────────────

bool McpDb::addPolicyWallet(int policyId, const QString& address) {
    return DbUtil::exec(
        db(),
        "INSERT OR IGNORE INTO mcp_policy_wallets (policy_id, wallet_address) VALUES (:pid, :addr)",
        {{":pid", policyId}, {":addr", address}});
}

bool McpDb::removePolicyWallet(int policyId, const QString& address) {
    return DbUtil::exec(
        db(), "DELETE FROM mcp_policy_wallets WHERE policy_id = :pid AND wallet_address = :addr",
        {{":pid", policyId}, {":addr", address}}, DbUtil::RequireRows::Yes);
}

QStringList McpDb::getPolicyWallets(int policyId) {
    QList<QString> values = DbUtil::many<QString>(
        db(),
        "SELECT wallet_address FROM mcp_policy_wallets WHERE policy_id = :pid ORDER BY "
        "wallet_address",
        {{":pid", policyId}}, [](const QSqlQuery& q) { return q.value(0).toString(); });
    return QStringList(values.begin(), values.end());
}

// ── Policy tool access ──────────────────────────────────────

bool McpDb::setPolicyToolAccess(int policyId, const QString& toolName, int access) {
    static const QString kSql = R"(
        INSERT INTO mcp_policy_tool_access (policy_id, tool_name, access, updated_at)
        VALUES (:pid, :name, :access, :ts)
        ON CONFLICT(policy_id, tool_name) DO UPDATE SET
            access = :access2,
            updated_at = :ts2
    )";

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return DbUtil::exec(db(), kSql,
                        {{":pid", policyId},
                         {":name", toolName},
                         {":access", access},
                         {":ts", now},
                         {":access2", access},
                         {":ts2", now}});
}

int McpDb::policyToolAccess(int policyId, const QString& toolName, int defaultAccess) {
    return DbUtil::scalarInt(db(),
                             "SELECT access FROM mcp_policy_tool_access "
                             "WHERE policy_id = :pid AND tool_name = :name LIMIT 1",
                             {{":pid", policyId}, {":name", toolName}})
        .value_or(defaultAccess);
}

QList<McpToolAccessRecord> McpDb::getPolicyToolConfigRecords(int policyId) {
    return DbUtil::many<McpToolAccessRecord>(
        db(),
        "SELECT tool_name, access FROM mcp_policy_tool_access "
        "WHERE policy_id = :pid ORDER BY tool_name",
        {{":pid", policyId}}, [](const QSqlQuery& q) { return toolAccessFromQuery(q, "access"); });
}

// ── Activity log ─────────────────────────────────────────────────

bool McpDb::insertActivity(const QString& toolName, const QString& arguments, const QString& result,
                           int durationMs, bool success, const QString& walletAddress,
                           int policyId) {
    static const QString kSql = R"(
        INSERT INTO mcp_activity_log (
            tool_name,
            arguments,
            result,
            duration_ms,
            success,
            wallet_address,
            policy_id,
            created_at
        )
        VALUES (:res, :args, :result, :dur, :ok, :wallet, :policy, :ts)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":res", toolName},
                         {":args", arguments},
                         {":result", result},
                         {":dur", durationMs},
                         {":ok", success ? 1 : 0},
                         {":wallet", walletAddress.isEmpty() ? QVariant() : walletAddress},
                         {":policy", policyId >= 0 ? QVariant(policyId) : QVariant()},
                         {":ts", QDateTime::currentSecsSinceEpoch()}});
}

QList<McpActivityRecord> McpDb::getRecentActivityRecords(int limit) {
    return DbUtil::many<McpActivityRecord>(
        db(), "SELECT * FROM mcp_activity_log ORDER BY created_at DESC LIMIT :lim",
        {{":lim", limit}}, activityFromQuery);
}

int McpDb::activityCount() {
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM mcp_activity_log").value_or(0);
}

QList<McpActivityRecord> McpDb::getActivityPageRecords(qint64 maxId, int limit) {
    QSqlQuery q(db());
    if (maxId < 0) {
        if (!DbUtil::prepareBindExec(q,
                                     "SELECT * FROM mcp_activity_log ORDER BY id DESC LIMIT :lim",
                                     {{":lim", limit}})) {
            return {};
        }
    } else {
        if (!DbUtil::prepareBindExec(
                q, "SELECT * FROM mcp_activity_log WHERE id <= :maxId ORDER BY id DESC LIMIT :lim",
                {{":maxId", maxId}, {":lim", limit}})) {
            return {};
        }
    }

    QList<McpActivityRecord> results;
    while (q.next()) {
        results.append(activityFromQuery(q));
    }
    return results;
}

int McpDb::activityCountForWallet(const QString& walletAddress) {
    return DbUtil::scalarInt(db(),
                             "SELECT COUNT(*) FROM mcp_activity_log WHERE wallet_address = :addr",
                             {{":addr", walletAddress}})
        .value_or(0);
}

int McpDb::activityCountForPolicy(int policyId) {
    return DbUtil::scalarInt(db(), "SELECT COUNT(*) FROM mcp_activity_log WHERE policy_id = :pid",
                             {{":pid", policyId}})
        .value_or(0);
}

bool McpDb::clearActivityLog() { return DbUtil::exec(db(), "DELETE FROM mcp_activity_log"); }

// ── Pending approvals ────────────────────────────────────────────

bool McpDb::insertPendingApproval(const QString& id, const QString& toolName,
                                  const QString& arguments, const QString& description,
                                  const QString& walletAddress, int policyId) {
    static const QString kSql = R"(
        INSERT INTO mcp_pending_approval (
            id,
            tool_name,
            arguments,
            description,
            status,
            wallet_address,
            policy_id,
            created_at
        )
        VALUES (:id, :res, :args, :desc, 'pending', :wallet, :policy, :ts)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":id", id},
                         {":res", toolName},
                         {":args", arguments},
                         {":desc", description},
                         {":wallet", walletAddress.isEmpty() ? QVariant() : walletAddress},
                         {":policy", policyId >= 0 ? QVariant(policyId) : QVariant()},
                         {":ts", QDateTime::currentSecsSinceEpoch()}});
}

int McpDb::pendingApprovalCountForWallet(const QString& walletAddress) {
    return DbUtil::scalarInt(db(),
                             "SELECT COUNT(*) FROM mcp_pending_approval "
                             "WHERE wallet_address = :addr AND status = 'pending'",
                             {{":addr", walletAddress}})
        .value_or(0);
}

QList<McpApprovalRecord> McpDb::getPendingApprovalsRecords() {
    return DbUtil::many<McpApprovalRecord>(
        db(), "SELECT * FROM mcp_pending_approval WHERE status = 'pending' ORDER BY created_at ASC",
        {}, approvalFromQuery);
}

int McpDb::pendingApprovalCountForPolicy(int policyId) {
    return DbUtil::scalarInt(db(),
                             "SELECT COUNT(*) FROM mcp_pending_approval "
                             "WHERE policy_id = :pid AND status = 'pending'",
                             {{":pid", policyId}})
        .value_or(0);
}

bool McpDb::resolveApproval(const QString& id, const QString& status, const QString& result,
                            const QString& errorMsg) {
    static const QString kSql = R"(
        UPDATE mcp_pending_approval
        SET
            status = :status,
            result = :result,
            error_msg = :err,
            resolved_at = :ts
        WHERE id = :id
    )";

    return DbUtil::exec(db(), kSql,
                        {{":status", status},
                         {":result", result},
                         {":err", errorMsg},
                         {":ts", QDateTime::currentSecsSinceEpoch()},
                         {":id", id}},
                        DbUtil::RequireRows::Yes);
}

std::optional<McpApprovalRecord> McpDb::getApprovalRecord(const QString& id) {
    return DbUtil::one<McpApprovalRecord>(
        db(), "SELECT * FROM mcp_pending_approval WHERE id = :id LIMIT 1", {{":id", id}},
        approvalFromQuery);
}

QList<McpApprovalRecord> McpDb::getApprovedUnexecutedRecords() {
    return DbUtil::many<McpApprovalRecord>(
        db(),
        "SELECT * FROM mcp_pending_approval WHERE status = 'completed' AND executed = 0 "
        "ORDER BY created_at ASC",
        {}, approvalFromQuery);
}

bool McpDb::markExecuted(const QString& id, const QString& result) {
    static const QString kSql = R"(
        UPDATE mcp_pending_approval
        SET executed = 1, result = :result
        WHERE id = :id
    )";
    return DbUtil::exec(db(), kSql, {{":result", result}, {":id", id}}, DbUtil::RequireRows::Yes);
}
