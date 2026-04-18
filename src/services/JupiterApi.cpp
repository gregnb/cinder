#include "JupiterApi.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

const QString JupiterApi::BASE_URL = "https://lite-api.jup.ag";

JupiterApi::JupiterApi(QObject* parent) : QObject(parent) {}

// ── HTTP helpers ─────────────────────────────────────────────

void JupiterApi::httpGet(const QUrl& url, const std::function<void(const QJsonObject&)>& onSuccess,
                         const QString& tag) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, tag]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(
                tag, QString("HTTP %1: %2")
                         .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                         .arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);

        if (parseErr.error != QJsonParseError::NoError) {
            emit requestFailed(tag, "JSON parse error: " + parseErr.errorString());
            return;
        }

        if (doc.isObject() && doc.object().contains("error")) {
            emit requestFailed(tag, doc.object()["error"].toString());
            return;
        }

        onSuccess(doc.object());
    });
}

void JupiterApi::httpPost(const QUrl& url, const QJsonObject& body,
                          const std::function<void(const QJsonObject&)>& onSuccess,
                          const QString& tag) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, tag]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(
                tag, QString("HTTP %1: %2")
                         .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                         .arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);

        if (parseErr.error != QJsonParseError::NoError) {
            emit requestFailed(tag, "JSON parse error: " + parseErr.errorString());
            return;
        }

        if (doc.isObject() && doc.object().contains("error")) {
            emit requestFailed(tag, doc.object()["error"].toString());
            return;
        }

        onSuccess(doc.object());
    });
}

// ── Public API ───────────────────────────────────────────────

void JupiterApi::fetchQuote(const QString& inputMint, const QString& outputMint, quint64 amount,
                            int slippageBps) {
    QUrl url(BASE_URL + "/swap/v1/quote");
    QUrlQuery query;
    query.addQueryItem("inputMint", inputMint);
    query.addQueryItem("outputMint", outputMint);
    query.addQueryItem("amount", QString::number(amount));
    query.addQueryItem("slippageBps", QString::number(slippageBps));
    url.setQuery(query);

    httpGet(
        url,
        [this](const QJsonObject& root) {
            JupiterQuote quote = JupiterQuote::fromJson(root);
            emit quoteReady(quote);
        },
        "quote");
}

void JupiterApi::fetchSwapTransaction(const QJsonObject& quoteResponse,
                                      const QString& userPublicKey) {
    QUrl url(BASE_URL + "/swap/v1/swap");

    QJsonObject body;
    body["quoteResponse"] = quoteResponse;
    body["userPublicKey"] = userPublicKey;
    body["wrapAndUnwrapSol"] = true;
    body["dynamicComputeUnitLimit"] = true;

    httpPost(
        url, body,
        [this](const QJsonObject& root) {
            QString txBase64 = root["swapTransaction"].toString();
            QByteArray serializedTx = QByteArray::fromBase64(txBase64.toUtf8());
            quint64 lastValidBlockHeight =
                static_cast<quint64>(root["lastValidBlockHeight"].toInteger());
            emit swapTransactionReady(serializedTx, lastValidBlockHeight);
        },
        "swap");
}
