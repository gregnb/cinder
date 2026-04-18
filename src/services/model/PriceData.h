#ifndef PRICEDATA_H
#define PRICEDATA_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

struct TokenPrice {
    QString coingeckoId;
    QString symbol;
    double priceUsd = 0.0;
    double change24h = 0.0;

    static TokenPrice fromJson(const QString& id, const QJsonObject& json) {
        TokenPrice tp;
        tp.coingeckoId = id;
        tp.priceUsd = json["usd"].toDouble();
        tp.change24h = json["usd_24h_change"].toDouble();
        return tp;
    }
};

Q_DECLARE_METATYPE(TokenPrice)

struct HistoricalPrice {
    qint64 timestamp = 0; // millisecond epoch
    double priceUsd = 0.0;

    static QList<HistoricalPrice> fromMarketChartJson(const QJsonObject& json) {
        QList<HistoricalPrice> list;
        const QJsonArray prices = json["prices"].toArray();
        list.reserve(prices.size());
        for (const auto& entry : prices) {
            const QJsonArray pair = entry.toArray();
            if (pair.size() >= 2) {
                HistoricalPrice hp;
                hp.timestamp = static_cast<qint64>(pair[0].toDouble());
                hp.priceUsd = pair[1].toDouble();
                list.append(hp);
            }
        }
        return list;
    }
};

Q_DECLARE_METATYPE(HistoricalPrice)

#endif // PRICEDATA_H
