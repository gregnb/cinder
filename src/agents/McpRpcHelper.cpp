#include "McpRpcHelper.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>

static constexpr int TIMEOUT_MS = 15000;
static const QString DEFAULT_RPC = QStringLiteral("https://api.mainnet-beta.solana.com");

QString McpRpcHelper::rpcUrl() {
    QSettings settings;
    QStringList endpoints = settings.value(QStringLiteral("rpcEndpoints")).toStringList();
    if (endpoints.isEmpty()) {
        return DEFAULT_RPC;
    }
    return endpoints.first();
}

QJsonObject McpRpcHelper::rpcCall(const QString& method, const QJsonArray& params) {
    QJsonObject body;
    body[QLatin1String("jsonrpc")] = QStringLiteral("2.0");
    body[QLatin1String("id")] = 1;
    body[QLatin1String("method")] = method;
    body[QLatin1String("params")] = params;
    return httpPost(QUrl(rpcUrl()), body);
}

QJsonObject McpRpcHelper::httpGet(const QUrl& url) {
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CinderWallet/1.0"));

    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(TIMEOUT_MS);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        QJsonObject err;
        err[QLatin1String("error")] = QStringLiteral("Request timed out");
        return err;
    }

    QByteArray data = reply->readAll();
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (status < 200 || status >= 300) {
        QJsonObject err;
        err[QLatin1String("error")] =
            QStringLiteral("HTTP %1: %2").arg(status).arg(QString::fromUtf8(data.left(200)));
        return err;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        return doc.object();
    }
    if (doc.isArray()) {
        QJsonObject wrapper;
        wrapper[QLatin1String("result")] = doc.array();
        return wrapper;
    }

    QJsonObject err;
    err[QLatin1String("error")] = QStringLiteral("Invalid JSON response");
    return err;
}

QJsonObject McpRpcHelper::httpPost(const QUrl& url, const QJsonObject& body) {
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CinderWallet/1.0"));

    QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(TIMEOUT_MS);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        QJsonObject err;
        err[QLatin1String("error")] = QStringLiteral("Request timed out");
        return err;
    }

    QByteArray data = reply->readAll();
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (status < 200 || status >= 300) {
        QJsonObject err;
        err[QLatin1String("error")] =
            QStringLiteral("HTTP %1: %2").arg(status).arg(QString::fromUtf8(data.left(200)));
        return err;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        return doc.object();
    }

    QJsonObject err;
    err[QLatin1String("error")] = QStringLiteral("Invalid JSON response");
    return err;
}
