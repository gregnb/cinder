#include "McpClient.h"
#include "Constants.h"
#include <QJsonDocument>
#include <QTimer>

McpClient::McpClient(QObject* parent) : QObject(parent) {}

McpClient::~McpClient() { cleanup(); }

void McpClient::runSelfTest(const QString& serverCommand) {
    cleanup();

    m_phase = TestPhase::Initialize;
    m_nextId = 1;
    m_readBuffer.clear();
    m_serverInfo = {};
    m_tools = {};

    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &McpClient::onReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, &McpClient::onProcessError);
    connect(m_process, &QProcess::started, this, [this]() {
        // Send initialize request once process is started.
        QJsonObject params;
        params[QLatin1String("protocolVersion")] = QStringLiteral("2025-03-26");
        params[QLatin1String("capabilities")] = QJsonObject();
        QJsonObject clientInfo;
        clientInfo[QLatin1String("name")] = QStringLiteral("CinderSelfTest");
        clientInfo[QLatin1String("version")] = AppVersion::qstring;
        params[QLatin1String("clientInfo")] = clientInfo;

        m_initializeId = m_nextId;
        sendRequest(QStringLiteral("initialize"), params);
    });

    m_process->start(serverCommand, QStringList());

    // Startup timeout after 3 seconds.
    QTimer::singleShot(3000, this, [this, serverCommand]() {
        if (!m_process || m_process->state() != QProcess::Starting) {
            return;
        }
        emit testComplete(false,
                          QStringLiteral("Failed to start MCP server: %1").arg(serverCommand));
        cleanup();
    });

    // Timeout after 5 seconds
    QTimer::singleShot(5000, this, [this]() {
        if (m_phase != TestPhase::Done && m_phase != TestPhase::Idle) {
            emit testComplete(false, QStringLiteral("Test timed out after 5 seconds"));
            cleanup();
        }
    });
}

void McpClient::sendRequest(const QString& method, const QJsonObject& params) {
    QJsonObject msg;
    msg[QLatin1String("jsonrpc")] = QStringLiteral("2.0");
    msg[QLatin1String("id")] = m_nextId++;
    msg[QLatin1String("method")] = method;
    msg[QLatin1String("params")] = params;

    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_process->write(data);
}

void McpClient::sendNotification(const QString& method, const QJsonObject& params) {
    QJsonObject msg;
    msg[QLatin1String("jsonrpc")] = QStringLiteral("2.0");
    msg[QLatin1String("method")] = method;
    if (!params.isEmpty()) {
        msg[QLatin1String("params")] = params;
    }

    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_process->write(data);
}

void McpClient::onReadyRead() {
    m_readBuffer.append(m_process->readAllStandardOutput());

    while (true) {
        int newline = m_readBuffer.indexOf('\n');
        if (newline < 0) {
            break;
        }

        QByteArray line = m_readBuffer.left(newline);
        m_readBuffer.remove(0, newline + 1);

        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        processResponse(doc.object());
    }
}

void McpClient::processResponse(const QJsonObject& msg) {
    int id = msg[QLatin1String("id")].toInt(-1);

    // Check for error response
    if (msg.contains(QLatin1String("error"))) {
        QJsonObject err = msg[QLatin1String("error")].toObject();
        emit testComplete(
            false, QStringLiteral("MCP error: %1").arg(err[QLatin1String("message")].toString()));
        m_phase = TestPhase::Done;
        cleanup();
        return;
    }

    QJsonObject result = msg[QLatin1String("result")].toObject();

    switch (m_phase) {
        case TestPhase::Initialize: {
            // Verify initialize response
            m_serverInfo = result[QLatin1String("serverInfo")].toObject();
            QString version = result[QLatin1String("protocolVersion")].toString();
            if (version.isEmpty()) {
                emit testComplete(
                    false, QStringLiteral("Invalid initialize response — no protocolVersion"));
                m_phase = TestPhase::Done;
                cleanup();
                return;
            }

            // Send initialized notification
            sendNotification(QStringLiteral("notifications/initialized"));

            // Send tools/list
            m_phase = TestPhase::ToolsList;
            m_toolsListId = m_nextId;
            sendRequest(QStringLiteral("tools/list"));
            break;
        }
        case TestPhase::ToolsList: {
            m_tools = result[QLatin1String("tools")].toArray();

            // Send tools/call with wallet_ping
            m_phase = TestPhase::ToolsCall;
            m_toolsCallId = m_nextId;
            QJsonObject callParams;
            callParams[QLatin1String("name")] = QStringLiteral("wallet_ping");
            callParams[QLatin1String("arguments")] = QJsonObject();
            sendRequest(QStringLiteral("tools/call"), callParams);
            break;
        }
        case TestPhase::ToolsCall: {
            m_phase = TestPhase::Done;

            // Build summary
            QString serverName = m_serverInfo[QLatin1String("name")].toString();
            QString serverVersion = m_serverInfo[QLatin1String("version")].toString();

            QStringList toolNames;
            for (const auto& t : m_tools) {
                toolNames.append(t.toObject()[QLatin1String("name")].toString());
            }

            // Extract ping result
            QJsonArray content = result[QLatin1String("content")].toArray();
            QString pingText;
            if (!content.isEmpty()) {
                pingText = content[0].toObject()[QLatin1String("text")].toString();
            }

            QString summary =
                QStringLiteral("Server: %1 v%2\nTools: %3\nPing: %4")
                    .arg(serverName, serverVersion, toolNames.join(QStringLiteral(", ")), pingText);

            emit testComplete(true, summary);
            cleanup();
            break;
        }
        default:
            break;
    }
}

void McpClient::onProcessError(QProcess::ProcessError error) {
    Q_UNUSED(error);
    if (m_phase != TestPhase::Done && m_phase != TestPhase::Idle) {
        emit testComplete(
            false, QStringLiteral("Process error: %1")
                       .arg(m_process ? m_process->errorString() : QStringLiteral("unknown")));
        m_phase = TestPhase::Done;
    }
}

void McpClient::cleanup() {
    m_phase = TestPhase::Idle;
    if (m_process) {
        QProcess* proc = m_process;
        m_process = nullptr;

        proc->closeWriteChannel();
        if (proc->state() != QProcess::NotRunning) {
            QObject::connect(proc, &QProcess::finished, proc, &QObject::deleteLater);
            proc->terminate();
            QTimer::singleShot(2000, proc, [proc]() {
                if (proc->state() != QProcess::NotRunning) {
                    proc->kill();
                }
            });
            return;
        }
        proc->deleteLater();
    }
}
