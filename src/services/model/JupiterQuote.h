#ifndef JUPITERQUOTE_H
#define JUPITERQUOTE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

struct JupiterQuote {
    QString inputMint;
    QString outputMint;
    quint64 inAmount = 0;
    quint64 outAmount = 0;
    quint64 otherAmountThreshold = 0;
    double priceImpactPct = 0;
    int slippageBps = 0;
    QString swapMode;
    QJsonObject rawResponse; // Full response to pass back to /swap

    struct RouteStep {
        QString dexLabel;
        QString inputMint;
        QString outputMint;
        int percent = 100;
    };
    QList<RouteStep> routePlan;

    static JupiterQuote fromJson(const QJsonObject& json) {
        JupiterQuote q;
        q.inputMint = json["inputMint"].toString();
        q.outputMint = json["outputMint"].toString();
        q.inAmount = static_cast<quint64>(json["inAmount"].toString().toULongLong());
        q.outAmount = static_cast<quint64>(json["outAmount"].toString().toULongLong());
        q.otherAmountThreshold =
            static_cast<quint64>(json["otherAmountThreshold"].toString().toULongLong());
        q.priceImpactPct = json["priceImpactPct"].toString().toDouble();
        q.slippageBps = json["slippageBps"].toInt();
        q.swapMode = json["swapMode"].toString();
        q.rawResponse = json;

        QJsonArray plan = json["routePlan"].toArray();
        for (const auto& step : plan) {
            QJsonObject s = step.toObject();
            QJsonObject info = s["swapInfo"].toObject();
            RouteStep rs;
            rs.dexLabel = info["label"].toString();
            rs.inputMint = info["inputMint"].toString();
            rs.outputMint = info["outputMint"].toString();
            rs.percent = s["percent"].toInt();
            q.routePlan.append(rs);
        }
        return q;
    }
};

Q_DECLARE_METATYPE(JupiterQuote)

#endif // JUPITERQUOTE_H
