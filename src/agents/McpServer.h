#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>

class McpServer : public QObject {
    Q_OBJECT
  public:
    explicit McpServer(QObject* parent = nullptr);

    using ToolCallback = std::function<QJsonObject(const QJsonObject& arguments)>;
    using PostCallHook = std::function<void(const QString& toolName, const QString& arguments,
                                            const QString& result, int durationMs, bool success)>;

    void registerTool(const QString& name, const QString& description,
                      const QJsonObject& inputSchema, ToolCallback callback);

    void setPostCallHook(PostCallHook hook);

    void run();

  private:
    void processMessage(const QByteArray& line);
    void handleInitialize(const QJsonValue& id, const QJsonObject& params);
    void handleToolsList(const QJsonValue& id);
    void handleToolsCall(const QJsonValue& id, const QJsonObject& params);
    void sendResponse(const QJsonValue& id, const QJsonObject& result);
    void sendError(const QJsonValue& id, int code, const QString& message);
    void writeLine(const QJsonObject& msg);

    struct ToolEntry {
        QString name;
        QString description;
        QJsonObject inputSchema;
        ToolCallback callback;
    };
    QList<ToolEntry> m_tools;
    PostCallHook m_postCallHook;
    bool m_initialized = false;
};

#endif // MCPSERVER_H
