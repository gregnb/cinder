#include "Constants.h"
#include "agents/McpRpcHelper.h"
#include "agents/McpServer.h"
#include "agents/McpToolDefs.h"
#include "db/ContactDb.h"
#include "db/Database.h"
#include "db/McpDb.h"
#include "db/NonceDb.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "db/ValidatorCacheDb.h"
#include "db/WalletDb.h"
#include "services/model/StakeAccountInfo.h"
#include "services/model/TransactionResponse.h"
#include "tx/Base58.h"
#include "tx/KnownTokens.h"
#include "tx/ProgramIds.h"
#include "tx/TxClassifier.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QUrlQuery>
#include <QUuid>

// ── DB initialization ────────────────────────────────────────────

static bool openWalletDb() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dbPath = dataDir + "/wallet.db";

    if (!QFile::exists(dbPath)) {
        return false;
    }

    // Open as default connection so WalletDb, TokenAccountDb, etc. all work
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA busy_timeout=3000");
    pragma.exec("PRAGMA foreign_keys=ON");

    return true;
}

// ── Schema helpers ───────────────────────────────────────────────

static QJsonObject emptySchema() {
    QJsonObject s;
    s[QLatin1String("type")] = QStringLiteral("object");
    s[QLatin1String("properties")] = QJsonObject();
    return s;
}

static QJsonObject prop(const QString& type, const QString& desc) {
    QJsonObject p;
    p[QLatin1String("type")] = type;
    p[QLatin1String("description")] = desc;
    return p;
}

// ── Approval polling (for write tools) ───────────────────────────

static QJsonObject taskStatusResponse(const QString& approvalId) {
    auto row = McpDb::getApprovalRecord(approvalId);
    if (!row.has_value()) {
        QJsonObject err;
        err[QLatin1String("error")] = QStringLiteral("Task not found");
        return err;
    }

    QJsonObject result;
    result[QLatin1String("task_id")] = approvalId;

    if (row->status == "pending") {
        result[QLatin1String("status")] = QStringLiteral("pending_approval");
        result[QLatin1String("message")] =
            QStringLiteral("Waiting for wallet owner to approve this request");
        result[QLatin1String("retry_after_seconds")] = 3;
    } else if (row->status == "rejected") {
        result[QLatin1String("status")] = QStringLiteral("rejected");
        result[QLatin1String("message")] = row->errorMsg.isEmpty()
                                               ? QStringLiteral("Request rejected by wallet owner")
                                               : row->errorMsg;
    } else if (row->status == "completed" && !row->executed) {
        result[QLatin1String("status")] = QStringLiteral("executing");
        result[QLatin1String("message")] = QStringLiteral("Approved. Transaction is executing.");
        result[QLatin1String("retry_after_seconds")] = 2;
    } else if (row->status == "completed" && row->executed) {
        QJsonDocument doc = QJsonDocument::fromJson(row->result.toUtf8());
        QJsonObject resObj = doc.isObject() ? doc.object() : QJsonObject();
        bool isFailed = !row->errorMsg.isEmpty() ||
                        resObj.value(QLatin1String("status")).toString() == "failed";
        if (isFailed) {
            result[QLatin1String("status")] = QStringLiteral("failed");
            result[QLatin1String("error")] = !row->errorMsg.isEmpty()
                                                 ? row->errorMsg
                                                 : resObj.value(QLatin1String("error")).toString();
        } else {
            result[QLatin1String("status")] = QStringLiteral("completed");
            result[QLatin1String("result")] =
                resObj.isEmpty() ? QJsonValue(row->result) : QJsonValue(resObj);
        }
    }

    return result;
}

static QJsonObject submitTask(const QString& toolName, const QJsonObject& args,
                              const QString& description, const QString& walletAddress = {},
                              int policyId = -1, bool autoApprove = false) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString argsJson = QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));

    if (!McpDb::insertPendingApproval(id, toolName, argsJson, description, walletAddress,
                                      policyId)) {
        QJsonObject err;
        err[QLatin1String("error")] = QStringLiteral("Failed to submit task");
        return err;
    }

    if (autoApprove) {
        McpDb::resolveApproval(id, QStringLiteral("completed"));
    }

    QJsonObject result;
    result[QLatin1String("task_id")] = id;
    result[QLatin1String("status")] =
        autoApprove ? QStringLiteral("executing") : QStringLiteral("pending_approval");
    result[QLatin1String("message")] =
        autoApprove
            ? QStringLiteral("Auto-approved. Poll wallet_task_status for the result.")
            : QStringLiteral(
                  "Waiting for wallet owner approval. Poll wallet_task_status for updates.");
    result[QLatin1String("retry_after_seconds")] = autoApprove ? 2 : 3;
    return result;
}

// ── Main ─────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Cinder"));
    app.setApplicationName(QStringLiteral("Cinder"));
    app.setApplicationVersion(AppVersion::qstring);

    bool dbOpen = openWalletDb();

    McpServer server;

    // ── wallet_ping — always available, no auth required ─────────
    server.registerTool(QStringLiteral("wallet_ping"),
                        QStringLiteral("Check if the Cinder wallet MCP server is running"),
                        emptySchema(), [](const QJsonObject&) -> QJsonObject {
                            QJsonObject r;
                            r[QLatin1String("status")] = QStringLiteral("ok");
                            r[QLatin1String("timestamp")] =
                                QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                            r[QLatin1String("server")] =
                                QStringLiteral("CinderWallet MCP v") + AppVersion::qstring;
                            return r;
                        });

    // ── API key authentication ───────────────────────────────────

    QString apiKey = qEnvironmentVariable("CINDER_API_KEY");

    if (apiKey.isEmpty()) {
        // No API key — register error tool alongside ping and run
        server.registerTool(
            QStringLiteral("wallet_error"),
            QStringLiteral("Authentication error — no API key provided"), emptySchema(),
            [](const QJsonObject&) -> QJsonObject {
                QJsonObject r;
                r[QLatin1String("error")] = QStringLiteral(
                    "No API key provided. Set the CINDER_API_KEY environment variable. "
                    "Create an access policy in the wallet's Access Policies tab to get an API "
                    "key.");
                return r;
            });
        server.run();
        return 0;
    }

    if (!dbOpen) {
        server.registerTool(
            QStringLiteral("wallet_error"),
            QStringLiteral("Database error — wallet database not found"), emptySchema(),
            [](const QJsonObject&) -> QJsonObject {
                QJsonObject r;
                r[QLatin1String("error")] = QStringLiteral(
                    "Wallet database not found. Ensure the Cinder wallet app has been launched "
                    "at least once to create the database.");
                return r;
            });
        server.run();
        return 0;
    }

    // Look up policy by API key
    auto policyRow = McpDb::getPolicyByApiKeyRecord(apiKey);
    if (!policyRow.has_value()) {
        server.registerTool(
            QStringLiteral("wallet_error"),
            QStringLiteral("Authentication error — invalid API key"), emptySchema(),
            [](const QJsonObject&) -> QJsonObject {
                QJsonObject r;
                r[QLatin1String("error")] = QStringLiteral(
                    "Invalid API key. Check the key in your configuration, or create "
                    "a new access policy in the wallet's Access Policies tab.");
                return r;
            });
        server.run();
        return 0;
    }

    int policyId = policyRow->id;
    QString policyName = policyRow->name;

    // Load policy's bound wallets
    QStringList policyWallets = McpDb::getPolicyWallets(policyId);
    QString singleWallet = (policyWallets.size() == 1) ? policyWallets.first() : QString();

    // Load global tool config
    QMap<QString, bool> toolConfig;
    auto configs = McpDb::getAllToolConfigRecords();
    for (const auto& row : configs) {
        toolConfig[row.toolName] = row.access != 0;
    }

    // Load policy tool access: { tool -> access level }
    QMap<QString, int> policyToolAccess;
    auto accessRows = McpDb::getPolicyToolConfigRecords(policyId);
    for (const auto& row : accessRows) {
        policyToolAccess[row.toolName] = row.access;
    }

    // Helper to check if a tool is globally enabled (default: true)
    auto isEnabled = [&](const char* name) -> bool {
        auto it = toolConfig.find(QString::fromLatin1(name));
        if (it != toolConfig.end()) {
            return it.value();
        }
        return true;
    };

    // Helper to get policy's access level for a tool
    auto getPolicyAccess = [&](const QString& toolName) -> int {
        auto it = policyToolAccess.find(toolName);
        if (it != policyToolAccess.end()) {
            return it.value();
        }
        return McpDb::AccessBlocked;
    };

    // Helper to check if wallet is bound to this policy
    auto isPolicyWallet = [&](const QString& address) -> bool {
        return policyWallets.contains(address);
    };

    // Helper to resolve wallet address from args.
    // When the policy has exactly one bound wallet, defaults to it.
    // When multiple wallets are bound, requires the caller to specify via 'from' or paramName.
    // Returns empty string if ambiguous (caller must check and return an error).
    auto resolveWallet = [&](const QJsonObject& args,
                             const QString& paramName = QStringLiteral("address")) -> QString {
        QString addr = args[paramName].toString();
        if (addr.isEmpty()) {
            addr = args["from"].toString();
        }
        if (addr.isEmpty()) {
            if (policyWallets.size() == 1) {
                return policyWallets.first();
            }
            return {};
        }
        return addr;
    };

    auto walletError = [&]() -> QString {
        if (policyWallets.isEmpty()) {
            return QStringLiteral("No wallets assigned to this policy.");
        }
        return QStringLiteral(
            "Multiple wallets are assigned to this policy. "
            "Specify an 'address' parameter. Use wallet_address to see available wallets.");
    };

    // ── Read Tools ─────────────────────────────────────────────────
    // (wallet_ping is registered before auth — always available)

    // Wrapper for read tools: checks policy access before calling handler.
    // Blocked = error, Approval/Allow = execute (no approval flow for reads).
    auto registerReadTool = [&](const char* name, const char* desc, const QJsonObject& schema,
                                const std::function<QJsonObject(const QJsonObject&)>& handler) {
        if (!isEnabled(name)) {
            return;
        }
        server.registerTool(
            QString::fromLatin1(name), QString::fromLatin1(desc), schema,
            [name, handler, &getPolicyAccess](const QJsonObject& args) -> QJsonObject {
                QString toolName = QString::fromLatin1(name);
                int access = getPolicyAccess(toolName);
                if (access == McpDb::AccessBlocked) {
                    QJsonObject err;
                    err[QLatin1String("error")] =
                        QStringLiteral(
                            "Tool '%1' is blocked for this policy. "
                            "The wallet owner can change this in the Access Policies tab.")
                            .arg(toolName);
                    return err;
                }
                return handler(args);
            });
    };

    // wallet_address
    registerReadTool(
        "wallet_address", "Get the wallet address(es) assigned to this policy", emptySchema(),
        [&policyWallets, policyName](const QJsonObject&) -> QJsonObject {
            QJsonObject r;
            if (policyWallets.isEmpty()) {
                r[QLatin1String("error")] = QStringLiteral("No wallets assigned to this policy.");
            } else if (policyWallets.size() == 1) {
                r[QLatin1String("address")] = policyWallets.first();
            } else {
                QJsonArray arr;
                for (const auto& w : policyWallets) {
                    arr.append(w);
                }
                r[QLatin1String("addresses")] = arr;
                r[QLatin1String("note")] =
                    QStringLiteral("Multiple wallets are bound to this policy. "
                                   "You must specify a 'from' address in write tools.");
            }
            r[QLatin1String("policy")] = policyName;
            return r;
        });

    // wallet_info
    registerReadTool("wallet_info", "Get wallet details and RPC endpoint", emptySchema(),
                     [&policyWallets, policyName](const QJsonObject&) -> QJsonObject {
                         QJsonObject r;
                         if (policyWallets.size() == 1) {
                             r[QLatin1String("address")] = policyWallets.first();
                         } else {
                             QJsonArray arr;
                             for (const auto& w : policyWallets) {
                                 arr.append(w);
                             }
                             r[QLatin1String("addresses")] = arr;
                         }
                         r[QLatin1String("rpc_endpoint")] =
                             QStringLiteral("https://api.mainnet-beta.solana.com");
                         r[QLatin1String("network")] = QStringLiteral("mainnet-beta");
                         r[QLatin1String("policy")] = policyName;
                         return r;
                     });

    // wallet_list_wallets — only returns policy's bound wallets
    registerReadTool(
        "wallet_list_wallets", "List wallets assigned to this policy", emptySchema(),
        [&policyWallets, &policyToolAccess, policyName](const QJsonObject&) -> QJsonObject {
            auto allWallets = WalletDb::getAllRecords();
            QJsonArray arr;
            for (const auto& w : allWallets) {
                QString addr = w.address;
                if (!policyWallets.contains(addr)) {
                    continue;
                }

                QJsonObject wo;
                wo[QLatin1String("label")] = w.label;
                wo[QLatin1String("address")] = addr;
                wo[QLatin1String("key_type")] = w.keyType;

                // Include per-tool access levels
                QJsonObject accessObj;
                for (auto it = policyToolAccess.begin(); it != policyToolAccess.end(); ++it) {
                    QString level;
                    switch (it.value()) {
                        case McpDb::AccessAllow:
                            level = QStringLiteral("allow");
                            break;
                        case McpDb::AccessApproval:
                            level = QStringLiteral("approval");
                            break;
                        default:
                            level = QStringLiteral("blocked");
                            break;
                    }
                    accessObj[it.key()] = level;
                }
                wo[QLatin1String("tool_access")] = accessObj;
                arr.append(wo);
            }
            QJsonObject r;
            r[QLatin1String("wallets")] = arr;
            r[QLatin1String("count")] = arr.size();
            r[QLatin1String("policy")] = policyName;
            return r;
        });

    // wallet_get_balance (DB-based — reads cached balance)
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        registerReadTool("wallet_get_balance", "Get SOL balance from the database cache", s,
                         [&resolveWallet, &isPolicyWallet,
                          &walletError](const QJsonObject& args) -> QJsonObject {
                             QString wallet = resolveWallet(args);
                             QJsonObject r;
                             r[QLatin1String("address")] = wallet;
                             if (wallet.isEmpty() || !isPolicyWallet(wallet)) {
                                 r[QLatin1String("error")] = walletError();
                                 return r;
                             }
                             auto snapshot = PortfolioDb::getLatestSnapshotRecord(wallet);
                             if (snapshot.has_value()) {
                                 auto tokens = PortfolioDb::getTokenSnapshotsRecords(snapshot->id);
                                 for (const auto& t : tokens) {
                                     if (t.symbol == "SOL") {
                                         r[QLatin1String("balance_sol")] = t.balance;
                                         r[QLatin1String("price_usd")] = t.priceUsd;
                                         r[QLatin1String("value_usd")] = t.valueUsd;
                                         return r;
                                     }
                                 }
                             }
                             r[QLatin1String("balance_sol")] = 0.0;
                             r[QLatin1String("note")] =
                                 QStringLiteral("No cached balance available");
                             return r;
                         });
    }

    // wallet_get_balances
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        registerReadTool("wallet_get_balances", "Get all token balances from the database", s,
                         [&resolveWallet, &isPolicyWallet,
                          &walletError](const QJsonObject& args) -> QJsonObject {
                             QString wallet = resolveWallet(args);
                             if (wallet.isEmpty() || !isPolicyWallet(wallet)) {
                                 QJsonObject r;
                                 r[QLatin1String("error")] = walletError();
                                 return r;
                             }
                             auto accounts = TokenAccountDb::getAccountsByOwnerRecords(wallet);
                             QJsonArray arr;
                             for (const auto& a : accounts) {
                                 QJsonObject ao;
                                 ao[QLatin1String("mint")] = a.tokenAddress;
                                 ao[QLatin1String("symbol")] = a.symbol;
                                 ao[QLatin1String("name")] = a.name;
                                 ao[QLatin1String("balance")] = a.balance;
                                 ao[QLatin1String("decimals")] = a.decimals;
                                 ao[QLatin1String("usd_price")] = a.usdPrice;
                                 arr.append(ao);
                             }
                             QJsonObject r;
                             r[QLatin1String("address")] = wallet;
                             r[QLatin1String("balances")] = arr;
                             r[QLatin1String("count")] = arr.size();
                             return r;
                         });
    }

    // wallet_get_portfolio
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        registerReadTool("wallet_get_portfolio", "Get the latest portfolio snapshot", s,
                         [&resolveWallet](const QJsonObject& args) -> QJsonObject {
                             QString wallet = resolveWallet(args);
                             auto snapshot = PortfolioDb::getLatestSnapshotRecord(wallet);
                             if (!snapshot.has_value()) {
                                 QJsonObject r;
                                 r[QLatin1String("error")] =
                                     QStringLiteral("No portfolio snapshot available");
                                 return r;
                             }
                             QJsonObject r;
                             r[QLatin1String("address")] = wallet;
                             r[QLatin1String("id")] = snapshot->id;
                             r[QLatin1String("timestamp")] = snapshot->timestamp;
                             r[QLatin1String("total_usd")] = snapshot->totalUsd;
                             r[QLatin1String("sol_price")] = snapshot->solPrice;
                             r[QLatin1String("created_at")] = snapshot->createdAt;
                             auto tokens = PortfolioDb::getTokenSnapshotsRecords(snapshot->id);
                             QJsonArray tokenArr;
                             for (const auto& t : tokens) {
                                 QJsonObject to;
                                 to[QLatin1String("mint")] = t.mint;
                                 to[QLatin1String("symbol")] = t.symbol;
                                 to[QLatin1String("balance")] = t.balance;
                                 to[QLatin1String("price_usd")] = t.priceUsd;
                                 to[QLatin1String("value_usd")] = t.valueUsd;
                                 tokenArr.append(to);
                             }
                             r[QLatin1String("tokens")] = tokenArr;
                             return r;
                         });
    }

    // wallet_get_portfolio_history
    {
        QJsonObject histSchema;
        histSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject histProps;
        histProps[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        histProps[QLatin1String("from_timestamp")] =
            prop("integer", "Start timestamp (epoch seconds)");
        histProps[QLatin1String("to_timestamp")] = prop("integer", "End timestamp (epoch seconds)");
        histProps[QLatin1String("max_rows")] =
            prop("integer", "Maximum number of snapshots to return");
        histSchema[QLatin1String("properties")] = histProps;
        registerReadTool("wallet_get_portfolio_history", "Get portfolio snapshots over time",
                         histSchema, [&resolveWallet](const QJsonObject& args) -> QJsonObject {
                             QString wallet = resolveWallet(args);
                             qint64 from = args["from_timestamp"].toInteger(0);
                             qint64 to =
                                 args["to_timestamp"].toInteger(QDateTime::currentSecsSinceEpoch());
                             int maxRows = args["max_rows"].toInt(100);
                             auto snapshots =
                                 PortfolioDb::getSnapshotsRecords(wallet, from, to, maxRows);
                             QJsonArray snapshotArr;
                             for (const auto& s : snapshots) {
                                 QJsonObject so;
                                 so[QLatin1String("id")] = s.id;
                                 so[QLatin1String("timestamp")] = s.timestamp;
                                 so[QLatin1String("total_usd")] = s.totalUsd;
                                 so[QLatin1String("sol_price")] = s.solPrice;
                                 so[QLatin1String("created_at")] = s.createdAt;
                                 snapshotArr.append(so);
                             }
                             QJsonObject r;
                             r[QLatin1String("snapshots")] = snapshotArr;
                             r[QLatin1String("count")] = snapshots.size();
                             return r;
                         });
    }

    // wallet_get_transactions
    {
        QJsonObject txSchema;
        txSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject txProps;
        txProps[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        txProps[QLatin1String("limit")] =
            prop("integer", "Max transactions to return (default 20)");
        txProps[QLatin1String("offset")] = prop("integer", "Offset for pagination (default 0)");
        txProps[QLatin1String("token")] = prop("string", "Filter by token mint or symbol");
        txProps[QLatin1String("activity_type")] =
            prop("string", "Filter by activity type (send, receive, swap)");
        txSchema[QLatin1String("properties")] = txProps;
        registerReadTool(
            "wallet_get_transactions", "Get transaction history for the wallet", txSchema,
            [&resolveWallet, &isPolicyWallet,
             &walletError](const QJsonObject& args) -> QJsonObject {
                QString wallet = resolveWallet(args);
                if (wallet.isEmpty() || !isPolicyWallet(wallet)) {
                    QJsonObject r;
                    r[QLatin1String("error")] = walletError();
                    return r;
                }
                int limit = args["limit"].toInt(20);
                int offset = args["offset"].toInt(0);
                QString token = args["token"].toString();
                QString actType = args["activity_type"].toString();
                auto txs =
                    TransactionDb::getTransactionsRecords(wallet, token, actType, limit, offset);
                QJsonArray txArr;
                for (const auto& t : txs) {
                    QJsonObject to;
                    to[QLatin1String("id")] = t.id;
                    to[QLatin1String("signature")] = t.signature;
                    to[QLatin1String("block_time")] = t.blockTime;
                    to[QLatin1String("activity_type")] = t.activityType;
                    to[QLatin1String("from_address")] = t.fromAddress;
                    to[QLatin1String("to_address")] = t.toAddress;
                    to[QLatin1String("token")] = t.token;
                    to[QLatin1String("amount")] = t.amount;
                    to[QLatin1String("fee")] = t.fee;
                    to[QLatin1String("slot")] = t.slot;
                    to[QLatin1String("err")] = t.err;
                    txArr.append(to);
                }
                QJsonObject r;
                r[QLatin1String("transactions")] = txArr;
                r[QLatin1String("count")] = txs.size();
                r[QLatin1String("total")] = TransactionDb::countTransactions(wallet);
                return r;
            });
    }

    // wallet_get_transaction
    {
        QJsonObject sigSchema;
        sigSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject sigProps;
        sigProps[QLatin1String("signature")] = prop("string", "Transaction signature");
        sigSchema[QLatin1String("properties")] = sigProps;
        sigSchema[QLatin1String("required")] = QJsonArray{QStringLiteral("signature")};
        registerReadTool(
            "wallet_get_transaction", "Get raw JSON for a specific transaction signature",
            sigSchema, [](const QJsonObject& args) -> QJsonObject {
                QString sig = args["signature"].toString();
                QString raw = TransactionDb::getRawJson(sig);
                if (raw.isEmpty()) {
                    QJsonObject r;
                    r[QLatin1String("error")] = QStringLiteral("Transaction not found");
                    return r;
                }
                QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
                return doc.object();
            });
    }

    // wallet_get_token_info
    {
        QJsonObject tiSchema;
        tiSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject tiProps;
        tiProps[QLatin1String("mint")] = prop("string", "Token mint address");
        tiSchema[QLatin1String("properties")] = tiProps;
        tiSchema[QLatin1String("required")] = QJsonArray{QStringLiteral("mint")};
        registerReadTool("wallet_get_token_info", "Get token metadata by mint address", tiSchema,
                         [](const QJsonObject& args) -> QJsonObject {
                             QString mint = args["mint"].toString();
                             auto token = TokenAccountDb::getTokenRecord(mint);
                             if (!token.has_value()) {
                                 QJsonObject r;
                                 r[QLatin1String("error")] = QStringLiteral("Token not found");
                                 return r;
                             }
                             QJsonObject r;
                             r[QLatin1String("address")] = token->address;
                             r[QLatin1String("symbol")] = token->symbol;
                             r[QLatin1String("name")] = token->name;
                             r[QLatin1String("decimals")] = token->decimals;
                             r[QLatin1String("token_program")] = token->tokenProgram;
                             r[QLatin1String("logo_url")] = token->logoUrl;
                             r[QLatin1String("metadata_fetched")] = token->metadataFetched;
                             r[QLatin1String("created_at")] = token->createdAt;
                             r[QLatin1String("updated_at")] = token->updatedAt;
                             return r;
                         });
    }

    // wallet_derive_ata
    {
        QJsonObject ataSchema;
        ataSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject ataProps;
        ataProps[QLatin1String("owner")] = prop("string", "Owner wallet address");
        ataProps[QLatin1String("mint")] = prop("string", "Token mint address");
        ataSchema[QLatin1String("properties")] = ataProps;
        ataSchema[QLatin1String("required")] =
            QJsonArray{QStringLiteral("owner"), QStringLiteral("mint")};
        registerReadTool("wallet_derive_ata",
                         "Derive the associated token account address for a mint", ataSchema,
                         [](const QJsonObject& args) -> QJsonObject {
                             Q_UNUSED(args);
                             QJsonObject r;
                             r[QLatin1String("note")] = QStringLiteral(
                                 "ATA derivation requires libsodium (PDA). Use the main app's "
                                 "terminal command instead.");
                             return r;
                         });
    }

    // wallet_get_contacts
    registerReadTool("wallet_get_contacts", "Get all contacts from the address book", emptySchema(),
                     [](const QJsonObject&) -> QJsonObject {
                         auto contacts = ContactDb::getAllRecords();
                         QJsonArray contactArr;
                         for (const auto& c : contacts) {
                             QJsonObject co;
                             co[QLatin1String("id")] = c.id;
                             co[QLatin1String("name")] = c.name;
                             co[QLatin1String("address")] = c.address;
                             co[QLatin1String("created_at")] = c.createdAt;
                             contactArr.append(co);
                         }
                         QJsonObject r;
                         r[QLatin1String("contacts")] = contactArr;
                         r[QLatin1String("count")] = contacts.size();
                         return r;
                     });

    // wallet_find_contact
    {
        QJsonObject fcSchema;
        fcSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject fcProps;
        fcProps[QLatin1String("query")] = prop("string", "Search by name or address");
        fcSchema[QLatin1String("properties")] = fcProps;
        fcSchema[QLatin1String("required")] = QJsonArray{QStringLiteral("query")};
        registerReadTool("wallet_find_contact", "Search contacts by name or address", fcSchema,
                         [](const QJsonObject& args) -> QJsonObject {
                             QString query = args["query"].toString();
                             auto contacts = ContactDb::getAllRecords(query);
                             QJsonArray contactArr;
                             for (const auto& c : contacts) {
                                 QJsonObject co;
                                 co[QLatin1String("id")] = c.id;
                                 co[QLatin1String("name")] = c.name;
                                 co[QLatin1String("address")] = c.address;
                                 co[QLatin1String("created_at")] = c.createdAt;
                                 contactArr.append(co);
                             }
                             QJsonObject r;
                             r[QLatin1String("contacts")] = contactArr;
                             r[QLatin1String("count")] = contacts.size();
                             return r;
                         });
    }

    // wallet_encode_base58
    {
        QJsonObject encSchema;
        encSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject encProps;
        encProps[QLatin1String("hex")] = prop("string", "Hex-encoded bytes to encode");
        encSchema[QLatin1String("properties")] = encProps;
        encSchema[QLatin1String("required")] = QJsonArray{QStringLiteral("hex")};
        registerReadTool("wallet_encode_base58", "Encode hex bytes to Base58", encSchema,
                         [](const QJsonObject& args) -> QJsonObject {
                             QByteArray bytes =
                                 QByteArray::fromHex(args["hex"].toString().toUtf8());
                             QJsonObject r;
                             r[QLatin1String("base58")] = Base58::encode(bytes);
                             return r;
                         });
    }

    // wallet_decode_base58
    {
        QJsonObject decSchema;
        decSchema[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject decProps;
        decProps[QLatin1String("base58")] = prop("string", "Base58-encoded string to decode");
        decSchema[QLatin1String("properties")] = decProps;
        decSchema[QLatin1String("required")] = QJsonArray{QStringLiteral("base58")};
        registerReadTool("wallet_decode_base58", "Decode a Base58 string to hex bytes", decSchema,
                         [](const QJsonObject& args) -> QJsonObject {
                             QByteArray decoded = Base58::decode(args["base58"].toString());
                             QJsonObject r;
                             r[QLatin1String("hex")] = QString::fromLatin1(decoded.toHex());
                             r[QLatin1String("length")] = decoded.size();
                             return r;
                         });
    }

    // ── wallet_classify_transaction ──────────────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("signature")] = prop("string", "Transaction signature");
        p[QLatin1String("wallet")] = prop(
            "string", "Wallet address for perspective (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("signature")};
        registerReadTool(
            "wallet_classify_transaction", "Classify a transaction by its activities", s,
            [&resolveWallet](const QJsonObject& args) -> QJsonObject {
                QString sig = args["signature"].toString();
                if (sig.isEmpty()) {
                    return {{"error", "signature is required"}};
                }
                QString rawJson = TransactionDb::getRawJson(sig);
                if (rawJson.isEmpty()) {
                    return {
                        {"error", QStringLiteral("Transaction %1 not found in cache").arg(sig)}};
                }
                QJsonObject txObj = QJsonDocument::fromJson(rawJson.toUtf8()).object();
                TransactionResponse tx = TransactionResponse::fromJson(txObj);
                QString wallet = args["wallet"].toString();
                if (wallet.isEmpty()) {
                    wallet = resolveWallet(args);
                }
                auto cls = TxClassifier::classify(tx, wallet);
                QJsonObject r;
                r[QLatin1String("type")] = static_cast<int>(cls.type);
                r[QLatin1String("label")] = cls.label;
                r[QLatin1String("from")] = cls.from;
                r[QLatin1String("to")] = cls.to;
                r[QLatin1String("amount")] = cls.amount;
                r[QLatin1String("mint")] = cls.mint;
                r[QLatin1String("token_symbol")] = cls.tokenSymbol;
                if (cls.type == TxClassifier::TxType::Swap) {
                    r[QLatin1String("amount_in")] = cls.amountIn;
                    r[QLatin1String("mint_in")] = cls.mintIn;
                    r[QLatin1String("symbol_in")] = cls.symbolIn;
                    r[QLatin1String("amount_out")] = cls.amountOut;
                    r[QLatin1String("mint_out")] = cls.mintOut;
                    r[QLatin1String("symbol_out")] = cls.symbolOut;
                    r[QLatin1String("dex")] = TxClassifier::dexName(cls.dexProgramId);
                }
                return r;
            });
    }

    // ── wallet_get_token_accounts ─────────────────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        registerReadTool("wallet_get_token_accounts", "Get token accounts for the wallet", s,
                         [&resolveWallet](const QJsonObject& args) -> QJsonObject {
                             QString wallet = resolveWallet(args);
                             auto accounts = TokenAccountDb::getAccountsByOwnerRecords(wallet);
                             QJsonArray arr;
                             for (const auto& a : accounts) {
                                 QJsonObject obj;
                                 obj[QLatin1String("account")] = a.accountAddress;
                                 obj[QLatin1String("mint")] = a.tokenAddress;
                                 obj[QLatin1String("balance")] = a.balance;
                                 obj[QLatin1String("symbol")] = a.symbol;
                                 obj[QLatin1String("name")] = a.name;
                                 obj[QLatin1String("decimals")] = a.decimals;
                                 obj[QLatin1String("token_program")] = a.tokenProgram;
                                 obj[QLatin1String("state")] = a.state;
                                 arr.append(obj);
                             }
                             QJsonObject r;
                             r[QLatin1String("address")] = wallet;
                             r[QLatin1String("accounts")] = arr;
                             r[QLatin1String("count")] = accounts.size();
                             return r;
                         });
    }

    // ── wallet_inspect_account (live RPC) ─────────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] = prop("string", "Account address to inspect");
        p[QLatin1String("encoding")] =
            prop("string", "Data encoding: base64 (default), base58, jsonParsed");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("address")};
        registerReadTool(
            "wallet_inspect_account", "Inspect any Solana account (live RPC)", s,
            [](const QJsonObject& args) -> QJsonObject {
                QString address = args["address"].toString();
                QString encoding = args["encoding"].toString();
                if (encoding.isEmpty()) {
                    encoding = QStringLiteral("base64");
                }
                QJsonObject config;
                config[QLatin1String("encoding")] = encoding;
                QJsonArray params;
                params.append(address);
                params.append(config);
                QJsonObject rpc = McpRpcHelper::rpcCall(QStringLiteral("getAccountInfo"), params);
                if (rpc.contains(QLatin1String("error"))) {
                    return rpc;
                }
                QJsonObject val = rpc["result"].toObject()["value"].toObject();
                if (val.isEmpty()) {
                    return {{"error", QStringLiteral("Account %1 not found").arg(address)}};
                }
                QJsonObject r;
                r[QLatin1String("address")] = address;
                r[QLatin1String("lamports")] = val["lamports"].toDouble();
                r[QLatin1String("owner")] = val["owner"].toString();
                r[QLatin1String("executable")] = val["executable"].toBool();
                r[QLatin1String("rent_epoch")] = val["rentEpoch"].toDouble();
                r[QLatin1String("data")] = val["data"];
                return r;
            });
    }

    // ── wallet_get_rent_exempt (live RPC) ─────────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("data_size")] = prop("integer", "Account data size in bytes");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("data_size")};
        registerReadTool("wallet_get_rent_exempt",
                         "Get minimum balance for rent exemption (live RPC)", s,
                         [](const QJsonObject& args) -> QJsonObject {
                             int dataSize = args["data_size"].toInt(0);
                             QJsonArray params;
                             params.append(dataSize);
                             QJsonObject rpc = McpRpcHelper::rpcCall(
                                 QStringLiteral("getMinimumBalanceForRentExemption"), params);
                             if (rpc.contains(QLatin1String("error"))) {
                                 return rpc;
                             }
                             quint64 lamports = static_cast<quint64>(rpc["result"].toDouble());
                             QJsonObject r;
                             r[QLatin1String("lamports")] = static_cast<double>(lamports);
                             r[QLatin1String("sol")] = static_cast<double>(lamports) / 1e9;
                             r[QLatin1String("data_size")] = dataSize;
                             return r;
                         });
    }

    // ── wallet_get_stake_accounts (live RPC) ──────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        registerReadTool(
            "wallet_get_stake_accounts", "Get stake accounts for the wallet (live RPC)", s,
            [&resolveWallet](const QJsonObject& args) -> QJsonObject {
                QString wallet = resolveWallet(args);
                // getProgramAccounts with memcmp filter on authorized withdrawer (offset 44)
                QByteArray walletBytes = Base58::decode(wallet);
                if (walletBytes.size() != 32) {
                    return {{"error", "Invalid wallet address"}};
                }
                QJsonObject memcmp;
                memcmp[QLatin1String("offset")] = 44;
                memcmp[QLatin1String("bytes")] = wallet;
                QJsonObject filter;
                filter[QLatin1String("memcmp")] = memcmp;
                QJsonArray filters;
                filters.append(filter);
                QJsonObject config;
                config[QLatin1String("encoding")] = QStringLiteral("jsonParsed");
                config[QLatin1String("filters")] = filters;
                QJsonArray params;
                params.append(SolanaPrograms::StakeProgram);
                params.append(config);
                QJsonObject rpc =
                    McpRpcHelper::rpcCall(QStringLiteral("getProgramAccounts"), params);
                if (rpc.contains(QLatin1String("error"))) {
                    return rpc;
                }
                // Fetch epoch info for state classification
                QJsonObject epochRpc =
                    McpRpcHelper::rpcCall(QStringLiteral("getEpochInfo"), QJsonArray{});
                quint64 currentEpoch = 0;
                if (!epochRpc.contains(QLatin1String("error"))) {
                    currentEpoch =
                        static_cast<quint64>(epochRpc["result"].toObject()["epoch"].toDouble());
                }
                QJsonArray accounts = rpc["result"].toArray();
                QJsonArray arr;
                for (const auto& item : accounts) {
                    QJsonObject obj = item.toObject();
                    QString pubkey = obj["pubkey"].toString();
                    QJsonObject account = obj["account"].toObject();
                    StakeAccountInfo info =
                        StakeAccountInfo::fromJsonParsed(pubkey, account, currentEpoch);
                    QJsonObject entry;
                    entry[QLatin1String("address")] = info.address;
                    entry[QLatin1String("lamports")] = static_cast<double>(info.lamports);
                    entry[QLatin1String("sol")] = info.solAmount();
                    entry[QLatin1String("stake")] = info.stakeAmount();
                    entry[QLatin1String("state")] = info.stateString();
                    entry[QLatin1String("voter")] = info.voteAccount;
                    entry[QLatin1String("staker")] = info.staker;
                    entry[QLatin1String("withdrawer")] = info.withdrawer;
                    entry[QLatin1String("activation_epoch")] =
                        static_cast<double>(info.activationEpoch);
                    entry[QLatin1String("deactivation_epoch")] =
                        static_cast<double>(info.deactivationEpoch);
                    arr.append(entry);
                }
                QJsonObject r;
                r[QLatin1String("address")] = wallet;
                r[QLatin1String("accounts")] = arr;
                r[QLatin1String("count")] = arr.size();
                return r;
            });
    }

    // ── wallet_get_validators (DB cache) ──────────────────────────
    {
        registerReadTool("wallet_get_validators", "Get validator list from cache", emptySchema(),
                         [](const QJsonObject&) -> QJsonObject {
                             auto validators = ValidatorCacheDb::getAllRecords();
                             QJsonArray arr;
                             for (const auto& v : validators) {
                                 QJsonObject obj;
                                 obj[QLatin1String("vote_account")] = v.voteAccount;
                                 obj[QLatin1String("name")] = v.name;
                                 obj[QLatin1String("score")] = v.score;
                                 obj[QLatin1String("version")] = v.version;
                                 obj[QLatin1String("city")] = v.city;
                                 obj[QLatin1String("country")] = v.country;
                                 obj[QLatin1String("avatar_url")] = v.avatarUrl;
                                 arr.append(obj);
                             }
                             QJsonObject r;
                             r[QLatin1String("validators")] = arr;
                             r[QLatin1String("count")] = arr.size();
                             return r;
                         });
    }

    // ── wallet_get_validator_info (DB cache) ──────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("vote_account")] = prop("string", "Validator vote account address");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("vote_account")};
        registerReadTool(
            "wallet_get_validator_info", "Get details for a specific validator", s,
            [](const QJsonObject& args) -> QJsonObject {
                QString voteAccount = args["vote_account"].toString();
                auto v = ValidatorCacheDb::getByVoteAccountRecord(voteAccount);
                if (!v.has_value()) {
                    return {{"error",
                             QStringLiteral("Validator %1 not found in cache").arg(voteAccount)}};
                }
                QJsonObject r;
                r[QLatin1String("vote_account")] = v->voteAccount;
                r[QLatin1String("name")] = v->name;
                r[QLatin1String("score")] = v->score;
                r[QLatin1String("version")] = v->version;
                r[QLatin1String("city")] = v->city;
                r[QLatin1String("country")] = v->country;
                r[QLatin1String("avatar_url")] = v->avatarUrl;
                return r;
            });
    }

    // ── wallet_get_swap_quote (Jupiter API) ───────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("input_mint")] = prop("string", "Input token mint address");
        p[QLatin1String("output_mint")] = prop("string", "Output token mint address");
        p[QLatin1String("amount")] =
            prop("integer", "Amount of input token in smallest unit (e.g. lamports)");
        p[QLatin1String("slippage_bps")] =
            prop("integer", "Slippage tolerance in basis points (default 50)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{
            QStringLiteral("input_mint"), QStringLiteral("output_mint"), QStringLiteral("amount")};
        registerReadTool("wallet_get_swap_quote", "Get a swap quote from Jupiter", s,
                         [](const QJsonObject& args) -> QJsonObject {
                             QString inputMint = args["input_mint"].toString();
                             QString outputMint = args["output_mint"].toString();
                             quint64 amount = static_cast<quint64>(args["amount"].toDouble());
                             int slippageBps = args["slippage_bps"].toInt(50);
                             QUrl url(QStringLiteral("https://lite-api.jup.ag/swap/v1/quote"));
                             QUrlQuery query;
                             query.addQueryItem(QStringLiteral("inputMint"), inputMint);
                             query.addQueryItem(QStringLiteral("outputMint"), outputMint);
                             query.addQueryItem(QStringLiteral("amount"), QString::number(amount));
                             query.addQueryItem(QStringLiteral("slippageBps"),
                                                QString::number(slippageBps));
                             url.setQuery(query);
                             return McpRpcHelper::httpGet(url);
                         });
    }

    // ── wallet_get_price (CoinGecko API) ──────────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("mint")] = prop("string", "Token mint address");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("mint")};
        registerReadTool(
            "wallet_get_price", "Get current price for a token mint", s,
            [](const QJsonObject& args) -> QJsonObject {
                QString mint = args["mint"].toString();
                // Try cached CoinGecko ID first
                QString cgId = TokenAccountDb::getCoingeckoId(mint);
                if (cgId.isEmpty()) {
                    // Try resolving via contract endpoint
                    QUrl resolveUrl(
                        QStringLiteral("https://api.coingecko.com/api/v3/coins/solana/contract/%1")
                            .arg(mint));
                    QJsonObject resolved = McpRpcHelper::httpGet(resolveUrl);
                    if (!resolved.contains(QLatin1String("error"))) {
                        cgId = resolved["id"].toString();
                    }
                }
                if (cgId.isEmpty()) {
                    return {{"error",
                             QStringLiteral("Could not resolve CoinGecko ID for %1").arg(mint)}};
                }
                QUrl priceUrl(QStringLiteral("https://api.coingecko.com/api/v3/simple/price"));
                QUrlQuery q;
                q.addQueryItem(QStringLiteral("ids"), cgId);
                q.addQueryItem(QStringLiteral("vs_currencies"), QStringLiteral("usd"));
                priceUrl.setQuery(q);
                QJsonObject priceResp = McpRpcHelper::httpGet(priceUrl);
                if (priceResp.contains(QLatin1String("error"))) {
                    return priceResp;
                }
                double usd = priceResp[cgId].toObject()["usd"].toDouble();
                QJsonObject r;
                r[QLatin1String("mint")] = mint;
                r[QLatin1String("coingecko_id")] = cgId;
                r[QLatin1String("usd")] = usd;
                return r;
            });
    }

    // ── wallet_get_network_stats (live RPC) ───────────────────────
    {
        registerReadTool(
            "wallet_get_network_stats", "Get Solana network statistics", emptySchema(),
            [](const QJsonObject&) -> QJsonObject {
                QJsonObject r;
                // Block height
                QJsonObject bh = McpRpcHelper::rpcCall(QStringLiteral("getBlockHeight"));
                if (!bh.contains(QLatin1String("error"))) {
                    r[QLatin1String("block_height")] = bh["result"].toDouble();
                }
                // Epoch info
                QJsonObject ei = McpRpcHelper::rpcCall(QStringLiteral("getEpochInfo"));
                if (!ei.contains(QLatin1String("error"))) {
                    QJsonObject epoch = ei["result"].toObject();
                    r[QLatin1String("epoch")] = epoch["epoch"].toDouble();
                    r[QLatin1String("slot_index")] = epoch["slotIndex"].toDouble();
                    r[QLatin1String("slots_in_epoch")] = epoch["slotsInEpoch"].toDouble();
                    r[QLatin1String("absolute_slot")] = epoch["absoluteSlot"].toDouble();
                    r[QLatin1String("transaction_count")] = epoch["transactionCount"].toDouble();
                }
                // Supply
                QJsonObject sup = McpRpcHelper::rpcCall(QStringLiteral("getSupply"));
                if (!sup.contains(QLatin1String("error"))) {
                    QJsonObject val = sup["result"].toObject()["value"].toObject();
                    r[QLatin1String("total_supply_sol")] = val["total"].toDouble() / 1e9;
                    r[QLatin1String("circulating_supply_sol")] =
                        val["circulating"].toDouble() / 1e9;
                }
                // Version
                QJsonObject ver = McpRpcHelper::rpcCall(QStringLiteral("getVersion"));
                if (!ver.contains(QLatin1String("error"))) {
                    r[QLatin1String("solana_version")] =
                        ver["result"].toObject()["solana-core"].toString();
                }
                return r;
            });
    }

    // ── wallet_get_blockhash (live RPC) ───────────────────────────
    {
        registerReadTool("wallet_get_blockhash", "Get the latest blockhash (live RPC)",
                         emptySchema(), [](const QJsonObject&) -> QJsonObject {
                             QJsonObject rpc =
                                 McpRpcHelper::rpcCall(QStringLiteral("getLatestBlockhash"));
                             if (rpc.contains(QLatin1String("error"))) {
                                 return rpc;
                             }
                             QJsonObject val = rpc["result"].toObject()["value"].toObject();
                             QJsonObject r;
                             r[QLatin1String("blockhash")] = val["blockhash"].toString();
                             r[QLatin1String("last_valid_block_height")] =
                                 val["lastValidBlockHeight"].toDouble();
                             return r;
                         });
    }

    // ── wallet_get_priority_fees (live RPC) ───────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("accounts")] = prop("array", "Account addresses to check fees for");
        s[QLatin1String("properties")] = p;
        registerReadTool("wallet_get_priority_fees", "Get recent prioritization fees (live RPC)", s,
                         [](const QJsonObject& args) -> QJsonObject {
                             QJsonArray accounts;
                             QJsonArray inputAccounts = args["accounts"].toArray();
                             for (const auto& a : inputAccounts) {
                                 accounts.append(a.toString());
                             }
                             QJsonArray params;
                             if (!accounts.isEmpty()) {
                                 params.append(accounts);
                             }
                             QJsonObject rpc = McpRpcHelper::rpcCall(
                                 QStringLiteral("getRecentPrioritizationFees"), params);
                             if (rpc.contains(QLatin1String("error"))) {
                                 return rpc;
                             }
                             QJsonObject r;
                             r[QLatin1String("fees")] = rpc["result"].toArray();
                             return r;
                         });
    }

    // ── wallet_simulate_transaction (live RPC) ────────────────────
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("transaction")] = prop("string", "Base64-encoded serialized transaction");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("transaction")};
        registerReadTool(
            "wallet_simulate_transaction", "Simulate a base64-encoded transaction (live RPC)", s,
            [](const QJsonObject& args) -> QJsonObject {
                QString tx = args["transaction"].toString();
                QJsonObject config;
                config[QLatin1String("sigVerify")] = false;
                config[QLatin1String("replaceRecentBlockhash")] = true;
                config[QLatin1String("encoding")] = QStringLiteral("base64");
                QJsonArray params;
                params.append(tx);
                params.append(config);
                QJsonObject rpc =
                    McpRpcHelper::rpcCall(QStringLiteral("simulateTransaction"), params);
                if (rpc.contains(QLatin1String("error"))) {
                    return rpc;
                }
                QJsonObject val = rpc["result"].toObject()["value"].toObject();
                QJsonObject r;
                r[QLatin1String("err")] = val["err"];
                r[QLatin1String("logs")] = val["logs"];
                r[QLatin1String("units_consumed")] = val["unitsConsumed"].toDouble();
                return r;
            });
    }

    // ── Task status — always available (like wallet_ping), no policy check ──

    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("task_id")] = prop("string", "Task ID returned by a write operation");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("task_id")};
        server.registerTool(
            QStringLiteral("wallet_task_status"),
            QStringLiteral(
                "Poll the status of an async write task. All write operations (send, swap, stake, "
                "etc.) return a task_id. Call this tool to check if the task is pending_approval, "
                "executing, completed, rejected, or failed. Poll every few seconds until you get a "
                "terminal status (completed, rejected, or failed)."),
            s, [](const QJsonObject& args) -> QJsonObject {
                QString taskId = args["task_id"].toString();
                if (taskId.isEmpty()) {
                    QJsonObject err;
                    err[QLatin1String("error")] = QStringLiteral("task_id is required");
                    return err;
                }
                return taskStatusResponse(taskId);
            });
    }

    // ── Write Tools (submit for approval with policy access check) ────

    auto registerWriteTool = [&](const char* name, const char* desc, const QJsonObject& schema,
                                 const std::function<QString(const QJsonObject&)>& descriptionFn) {
        if (!isEnabled(name)) {
            return;
        }
        QString fullDesc =
            QStringLiteral("%1. Returns a task_id immediately. Poll wallet_task_status with the "
                           "task_id to check progress (pending_approval → executing → completed).")
                .arg(QString::fromLatin1(desc));
        server.registerTool(
            QString::fromLatin1(name), fullDesc, schema,
            [name, descriptionFn, singleWallet, policyId, &getPolicyAccess,
             &isPolicyWallet](const QJsonObject& args) -> QJsonObject {
                // Resolve which wallet address this write targets
                QString targetWallet = args["from"].toString();
                if (targetWallet.isEmpty()) {
                    targetWallet = singleWallet; // empty if multiple wallets bound
                }
                if (targetWallet.isEmpty()) {
                    QJsonObject err;
                    err[QLatin1String("error")] = QStringLiteral(
                        "Multiple wallets are assigned to this policy. "
                        "You must specify a 'from' address. "
                        "Use wallet_address or wallet_list_wallets to see available wallets.");
                    return err;
                }

                // Verify wallet is bound to this policy
                if (!isPolicyWallet(targetWallet)) {
                    QJsonObject err;
                    err[QLatin1String("error")] =
                        QStringLiteral(
                            "Wallet %1 is not assigned to this policy. "
                            "The wallet administrator can assign it in the Access Policies tab.")
                            .arg(targetWallet);
                    return err;
                }

                // Check policy's tool access level
                QString toolName = QString::fromLatin1(name);
                int access = getPolicyAccess(toolName);

                if (access == McpDb::AccessBlocked) {
                    QJsonObject err;
                    err[QLatin1String("error")] =
                        QStringLiteral(
                            "Tool '%1' is blocked for this policy. "
                            "The wallet owner can change this in the Access Policies tab.")
                            .arg(toolName);
                    return err;
                }

                QString humanDesc = descriptionFn(args);
                bool autoApprove = (access == McpDb::AccessAllow);
                return submitTask(toolName, args, humanDesc, targetWallet, policyId, autoApprove);
            });
    };

    // wallet_send_sol
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("to")] = prop("string", "Recipient address");
        p[QLatin1String("amount")] = prop("number", "Amount of SOL to send");
        p[QLatin1String("from")] =
            prop("string", "Sender wallet address (required if policy has multiple wallets)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("to"), QStringLiteral("amount")};
        registerWriteTool("wallet_send_sol", "Transfer SOL to an address", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Send %1 SOL to %2")
                                  .arg(a["amount"].toDouble(), 0, 'f', 9)
                                  .arg(a["to"].toString());
                          });
    }

    // wallet_send_token
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("to")] = prop("string", "Recipient address");
        p[QLatin1String("mint")] = prop("string", "Token mint address");
        p[QLatin1String("amount")] = prop("number", "Amount of tokens to send");
        p[QLatin1String("from")] =
            prop("string", "Sender wallet address (required if policy has multiple wallets)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] =
            QJsonArray{QStringLiteral("to"), QStringLiteral("mint"), QStringLiteral("amount")};
        registerWriteTool("wallet_send_token", "Transfer SPL tokens to an address", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Send %1 tokens (%2) to %3")
                                  .arg(a["amount"].toDouble(), 0, 'f', 6)
                                  .arg(a["mint"].toString().left(8))
                                  .arg(a["to"].toString());
                          });
    }

    // wallet_swap
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("input_mint")] = prop("string", "Input token mint address");
        p[QLatin1String("output_mint")] = prop("string", "Output token mint address");
        p[QLatin1String("amount")] = prop("number", "Amount of input token to swap");
        p[QLatin1String("slippage_bps")] =
            prop("integer", "Slippage tolerance in basis points (default 50)");
        p[QLatin1String("from")] =
            prop("string", "Wallet address to swap from (required if policy has multiple wallets)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{
            QStringLiteral("input_mint"), QStringLiteral("output_mint"), QStringLiteral("amount")};
        registerWriteTool("wallet_swap", "Swap tokens via Jupiter", s, [](const QJsonObject& a) {
            return QStringLiteral("Swap %1 of %2 for %3")
                .arg(a["amount"].toDouble(), 0, 'f', 6)
                .arg(a["input_mint"].toString().left(8))
                .arg(a["output_mint"].toString().left(8));
        });
    }

    // wallet_stake_create
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("validator")] = prop("string", "Validator vote account address");
        p[QLatin1String("amount")] = prop("number", "Amount of SOL to stake");
        p[QLatin1String("from")] = prop(
            "string", "Wallet address to stake from (required if policy has multiple wallets)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] =
            QJsonArray{QStringLiteral("validator"), QStringLiteral("amount")};
        registerWriteTool("wallet_stake_create", "Create and delegate a stake account", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Stake %1 SOL with validator %2")
                                  .arg(a["amount"].toDouble(), 0, 'f', 9)
                                  .arg(a["validator"].toString().left(8));
                          });
    }

    // Simple write tools with just an address/account arg
    auto registerSimpleWriteTool = [&](const char* name, const char* desc, const char* argName,
                                       const char* argDesc, const QString& descTemplate) {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QString::fromLatin1(argName)] = prop("string", QString::fromLatin1(argDesc));
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QString::fromLatin1(argName)};
        registerWriteTool(name, desc, s, [argName, descTemplate](const QJsonObject& a) {
            return descTemplate.arg(a[QString::fromLatin1(argName)].toString());
        });
    };

    registerSimpleWriteTool("wallet_stake_deactivate", "Deactivate a stake account",
                            "stake_account", "Stake account address",
                            QStringLiteral("Deactivate stake account %1"));
    registerSimpleWriteTool("wallet_token_close", "Close an empty token account", "token_account",
                            "Token account address", QStringLiteral("Close token account %1"));
    registerSimpleWriteTool("wallet_nonce_advance", "Advance a nonce value", "nonce_account",
                            "Nonce account address", QStringLiteral("Advance nonce %1"));
    registerSimpleWriteTool("wallet_nonce_close", "Close a nonce account", "nonce_account",
                            "Nonce account address", QStringLiteral("Close nonce account %1"));

    // wallet_stake_withdraw
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("stake_account")] = prop("string", "Stake account address");
        p[QLatin1String("amount")] = prop("number", "Amount of SOL to withdraw");
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] =
            QJsonArray{QStringLiteral("stake_account"), QStringLiteral("amount")};
        registerWriteTool("wallet_stake_withdraw", "Withdraw from a stake account", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Withdraw %1 SOL from stake account %2")
                                  .arg(a["amount"].toDouble(), 0, 'f', 9)
                                  .arg(a["stake_account"].toString().left(8));
                          });
    }

    // wallet_token_burn
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("mint")] = prop("string", "Token mint address");
        p[QLatin1String("amount")] = prop("number", "Amount of tokens to burn");
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("mint"), QStringLiteral("amount")};
        registerWriteTool("wallet_token_burn", "Burn tokens", s, [](const QJsonObject& a) {
            return QStringLiteral("Burn %1 tokens (%2)")
                .arg(a["amount"].toDouble(), 0, 'f', 6)
                .arg(a["mint"].toString().left(8));
        });
    }

    // wallet_nonce_create
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("lamports")] = prop("integer", "Lamports to fund the nonce account with");
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        registerWriteTool("wallet_nonce_create", "Create a durable nonce account", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Create nonce account with %1 lamports")
                                  .arg(a["lamports"].toInteger(0));
                          });
    }

    // wallet_nonce_withdraw
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("nonce_account")] = prop("string", "Nonce account address");
        p[QLatin1String("amount")] = prop("number", "Amount of SOL to withdraw");
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] =
            QJsonArray{QStringLiteral("nonce_account"), QStringLiteral("amount")};
        registerWriteTool("wallet_nonce_withdraw", "Withdraw from a nonce account", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Withdraw %1 SOL from nonce %2")
                                  .arg(a["amount"].toDouble(), 0, 'f', 9)
                                  .arg(a["nonce_account"].toString().left(8));
                          });
    }

    // wallet_add_contact
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("name")] = prop("string", "Contact name");
        p[QLatin1String("address")] = prop("string", "Solana address");
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] =
            QJsonArray{QStringLiteral("name"), QStringLiteral("address")};
        registerWriteTool("wallet_add_contact", "Add a contact to the address book", s,
                          [](const QJsonObject& a) {
                              return QStringLiteral("Add contact '%1' (%2)")
                                  .arg(a["name"].toString(), a["address"].toString().left(8));
                          });
    }

    // wallet_remove_contact
    {
        QJsonObject s;
        s[QLatin1String("type")] = QStringLiteral("object");
        QJsonObject p;
        p[QLatin1String("address")] = prop("string", "Contact address to remove");
        p[QLatin1String("from")] =
            prop("string", "Wallet address (auto-selected if policy has one wallet)");
        s[QLatin1String("properties")] = p;
        s[QLatin1String("required")] = QJsonArray{QStringLiteral("address")};
        registerWriteTool(
            "wallet_remove_contact", "Remove a contact from the address book", s,
            [](const QJsonObject& a) {
                return QStringLiteral("Remove contact %1").arg(a["address"].toString().left(8));
            });
    }

    // ── Post-call hook: log activity ─────────────────────────────

    server.setPostCallHook([policyId](const QString& toolName, const QString& arguments,
                                      const QString& result, int durationMs, bool success) {
        // Extract wallet_address from arguments if present
        QString wallet;
        QJsonDocument doc = QJsonDocument::fromJson(arguments.toUtf8());
        if (doc.isObject()) {
            wallet = doc.object()["from"].toString();
        }
        McpDb::insertActivity(toolName, arguments, result, durationMs, success, wallet, policyId);
    });

    server.run();
    return 0;
}
