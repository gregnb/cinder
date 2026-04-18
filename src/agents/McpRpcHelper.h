#ifndef MCPRPCHELPER_H
#define MCPRPCHELPER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

class McpRpcHelper {
  public:
    // Blocking Solana JSON-RPC call. Reads RPC URL from QSettings.
    static QJsonObject rpcCall(const QString& method, const QJsonArray& params = {});

    // Blocking HTTP GET, returns parsed JSON object
    static QJsonObject httpGet(const QUrl& url);

    // Blocking HTTP POST with JSON body
    static QJsonObject httpPost(const QUrl& url, const QJsonObject& body);

  private:
    static QString rpcUrl();
};

#endif // MCPRPCHELPER_H
