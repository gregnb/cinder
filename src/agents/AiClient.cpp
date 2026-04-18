#include "AiClient.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

static const QUrl ANTHROPIC_API_URL(QStringLiteral("https://api.anthropic.com/v1/messages"));

AiClient::AiClient(QObject* parent) : QObject(parent) {}

void AiClient::sendMessage(const QString& token, const QString& model, const QString& userMessage,
                           int maxTokens) {
    qDebug() << "[AiClient] sendMessage called, model:" << model
             << "token length:" << token.length() << "prompt length:" << userMessage.length();

    if (m_activeReply) {
        qDebug() << "[AiClient] Already busy, rejecting request";
        emit requestFailed(QStringLiteral("A request is already in progress"));
        return;
    }

    QJsonObject msg;
    msg[QStringLiteral("role")] = QStringLiteral("user");
    msg[QStringLiteral("content")] = userMessage;

    QJsonArray messages;
    messages.append(msg);

    QJsonObject body;
    body[QStringLiteral("model")] = model;
    body[QStringLiteral("max_tokens")] = maxTokens;
    body[QStringLiteral("messages")] = messages;

    QNetworkRequest request(ANTHROPIC_API_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    request.setRawHeader("anthropic-beta", "oauth-2025-04-20");
    request.setRawHeader("anthropic-version", "2023-06-01");

    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    qDebug() << "[AiClient] POST" << ANTHROPIC_API_URL << "payload size:" << payload.size();

    m_activeReply = m_nam.post(request, payload);

    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* reply = m_activeReply;
        m_activeReply = nullptr;
        reply->deleteLater();

        QByteArray data = reply->readAll();
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        qDebug() << "[AiClient] Response received, HTTP status:" << httpStatus
                 << "body size:" << data.size() << "error:" << reply->error();

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "[AiClient] Error response body:" << data.left(500);
            QString detail;
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                detail = doc.object()[QStringLiteral("error")]
                             .toObject()[QStringLiteral("message")]
                             .toString();
            }
            if (detail.isEmpty()) {
                detail = reply->errorString();
            }

            qDebug() << "[AiClient] Emitting requestFailed:" << detail;
            emit requestFailed(QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(detail));
            return;
        }

        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            qDebug() << "[AiClient] JSON parse error:" << parseErr.errorString();
            emit requestFailed(QStringLiteral("JSON parse error: ") + parseErr.errorString());
            return;
        }

        QJsonArray content = doc.object()[QStringLiteral("content")].toArray();
        if (content.isEmpty()) {
            qDebug() << "[AiClient] Empty content array, full response:" << data.left(500);
            emit requestFailed(QStringLiteral("Empty response from API"));
            return;
        }

        QString text = content[0].toObject()[QStringLiteral("text")].toString();
        qDebug() << "[AiClient] Success, response text length:" << text.length();
        emit responseReady(text);
    });
}

void AiClient::cancel() {
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

bool AiClient::isBusy() const { return m_activeReply != nullptr; }
