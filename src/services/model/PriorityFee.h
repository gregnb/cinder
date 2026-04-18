#ifndef PRIORITYFEE_H
#define PRIORITYFEE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>

struct PriorityFee {
    quint64 slot = 0;
    quint64 prioritizationFee = 0; // micro-lamports per compute unit

    static PriorityFee fromJson(const QJsonObject& json) {
        return {static_cast<quint64>(json["slot"].toInteger()),
                static_cast<quint64>(json["prioritizationFee"].toInteger())};
    }

    static QList<PriorityFee> fromJsonArray(const QJsonArray& arr) {
        QList<PriorityFee> list;
        list.reserve(arr.size());
        for (const auto& v : arr)
            list.append(fromJson(v.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(PriorityFee)

#endif // PRIORITYFEE_H
