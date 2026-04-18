#include "McpServer.h"
#include "Constants.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

static constexpr const char* PROTOCOL_VERSION = "2025-03-26";
static constexpr const char* SERVER_NAME = "CinderWallet";
static constexpr const char* SERVER_VERSION = AppVersion::string;

McpServer::McpServer(QObject* parent) : QObject(parent) {}

void McpServer::registerTool(const QString& name, const QString& description,
                             const QJsonObject& inputSchema, ToolCallback callback) {
    m_tools.append({name, description, inputSchema, std::move(callback)});
}

void McpServer::setPostCallHook(PostCallHook hook) { m_postCallHook = std::move(hook); }

void McpServer::run() {
    QFile stdinFile;
    stdinFile.open(stdin, QIODevice::ReadOnly | QIODevice::Text);

    QTextStream stream(&stdinFile);
    stream.setEncoding(QStringConverter::Utf8);

    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.isEmpty()) {
            continue;
        }
        processMessage(line.toUtf8());
    }
}

void McpServer::processMessage(const QByteArray& line) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject msg = doc.object();
    QString method = msg[QLatin1String("method")].toString();
    QJsonValue id = msg[QLatin1String("id")];
    QJsonObject params = msg[QLatin1String("params")].toObject();

    if (method == QLatin1String("initialize")) {
        handleInitialize(id, params);
    } else if (method == QLatin1String("notifications/initialized")) {
        m_initialized = true;
    } else if (method == QLatin1String("tools/list")) {
        handleToolsList(id);
    } else if (method == QLatin1String("tools/call")) {
        handleToolsCall(id, params);
    } else if (!id.isNull() && !id.isUndefined()) {
        sendError(id, -32601, QStringLiteral("Method not found: %1").arg(method));
    }
}

void McpServer::handleInitialize(const QJsonValue& id, const QJsonObject& params) {
    Q_UNUSED(params);

    QJsonObject result;
    result[QLatin1String("protocolVersion")] = QLatin1String(PROTOCOL_VERSION);

    QJsonObject capabilities;
    QJsonObject toolsCap;
    capabilities[QLatin1String("tools")] = toolsCap;
    result[QLatin1String("capabilities")] = capabilities;

    QJsonObject serverInfo;
    serverInfo[QLatin1String("name")] = QLatin1String(SERVER_NAME);
    serverInfo[QLatin1String("version")] = QLatin1String(SERVER_VERSION);
    result[QLatin1String("serverInfo")] = serverInfo;

    sendResponse(id, result);
}

void McpServer::handleToolsList(const QJsonValue& id) {
    QJsonArray toolsArray;
    for (const auto& tool : m_tools) {
        QJsonObject t;
        t[QLatin1String("name")] = tool.name;
        t[QLatin1String("description")] = tool.description;
        t[QLatin1String("inputSchema")] = tool.inputSchema;
        toolsArray.append(t);
    }

    QJsonObject result;
    result[QLatin1String("tools")] = toolsArray;
    sendResponse(id, result);
}

void McpServer::handleToolsCall(const QJsonValue& id, const QJsonObject& params) {
    QString toolName = params[QLatin1String("name")].toString();
    QJsonObject arguments = params[QLatin1String("arguments")].toObject();

    for (const auto& tool : m_tools) {
        if (tool.name == toolName) {
            QElapsedTimer timer;
            timer.start();

            QJsonObject toolResult = tool.callback(arguments);

            int durationMs = static_cast<int>(timer.elapsed());

            QString resultJson =
                QString::fromUtf8(QJsonDocument(toolResult).toJson(QJsonDocument::Compact));

            QJsonObject result;
            QJsonArray content;
            QJsonObject textContent;
            textContent[QLatin1String("type")] = QStringLiteral("text");
            textContent[QLatin1String("text")] = resultJson;
            content.append(textContent);
            result[QLatin1String("content")] = content;

            sendResponse(id, result);

            if (m_postCallHook) {
                QString argsJson =
                    QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact));
                m_postCallHook(toolName, argsJson, resultJson, durationMs, true);
            }
            return;
        }
    }

    if (m_postCallHook) {
        m_postCallHook(toolName, {}, {}, 0, false);
    }
    sendError(id, -32602, QStringLiteral("Unknown tool: %1").arg(toolName));
}

void McpServer::sendResponse(const QJsonValue& id, const QJsonObject& result) {
    QJsonObject msg;
    msg[QLatin1String("jsonrpc")] = QStringLiteral("2.0");
    msg[QLatin1String("id")] = id;
    msg[QLatin1String("result")] = result;
    writeLine(msg);
}

void McpServer::sendError(const QJsonValue& id, int code, const QString& message) {
    QJsonObject err;
    err[QLatin1String("code")] = code;
    err[QLatin1String("message")] = message;

    QJsonObject msg;
    msg[QLatin1String("jsonrpc")] = QStringLiteral("2.0");
    msg[QLatin1String("id")] = id;
    msg[QLatin1String("error")] = err;
    writeLine(msg);
}

void McpServer::writeLine(const QJsonObject& msg) {
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    data.append('\n');

    QFile stdoutFile;
    stdoutFile.open(stdout, QIODevice::WriteOnly);
    stdoutFile.write(data);
    stdoutFile.flush();
}
