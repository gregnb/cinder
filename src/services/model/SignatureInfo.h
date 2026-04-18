#ifndef SIGNATUREINFO_H
#define SIGNATUREINFO_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

struct SignatureInfo {
    QString signature;
    qint64 slot = 0;
    qint64 blockTime = 0;
    bool hasError = false;
    QString memo;
    QString confirmationStatus;

    static SignatureInfo fromJson(const QJsonObject& json) {
        SignatureInfo s;
        s.signature = json["signature"].toString();
        s.slot = json["slot"].toInteger();
        s.blockTime = json["blockTime"].toInteger();
        s.hasError = !json["err"].isNull();
        s.memo = json["memo"].toString();
        s.confirmationStatus = json["confirmationStatus"].toString();
        return s;
    }

    static QList<SignatureInfo> fromJsonArray(const QJsonArray& arr) {
        QList<SignatureInfo> list;
        list.reserve(arr.size());
        for (const auto& val : arr)
            list.append(fromJson(val.toObject()));
        return list;
    }
};

Q_DECLARE_METATYPE(SignatureInfo)

#endif // SIGNATUREINFO_H
