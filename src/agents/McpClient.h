#ifndef MCPCLIENT_H
#define MCPCLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QProcess>

class McpClient : public QObject {
    Q_OBJECT
  public:
    explicit McpClient(QObject* parent = nullptr);
    ~McpClient() override;

    void runSelfTest(const QString& serverCommand);

  signals:
    void testComplete(bool success, const QString& summary);

  private:
    void sendRequest(const QString& method, const QJsonObject& params = {});
    void sendNotification(const QString& method, const QJsonObject& params = {});
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void processResponse(const QJsonObject& msg);
    void cleanup();

    QProcess* m_process = nullptr;
    QByteArray m_readBuffer;
    int m_nextId = 1;
    int m_initializeId = 0;
    int m_toolsListId = 0;
    int m_toolsCallId = 0;

    enum class TestPhase { Idle, Initialize, ToolsList, ToolsCall, Done };
    TestPhase m_phase = TestPhase::Idle;
    QJsonObject m_serverInfo;
    QJsonArray m_tools;
};

#endif // MCPCLIENT_H
