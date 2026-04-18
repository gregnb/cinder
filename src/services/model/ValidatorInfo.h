#ifndef VALIDATORINFO_H
#define VALIDATORINFO_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

struct ValidatorInfo {
    QString voteAccount;
    QString nodePubkey;
    QString name;
    QString avatarUrl;
    QString version;
    QString city;
    QString country;
    int commission = 0;
    quint64 activatedStake = 0;
    quint64 lastVote = 0;
    quint64 epochCredits = 0;
    double apy = 0.0;
    double uptimePct = 0.0;
    int score = 0;
    bool delinquent = false;
    bool superminority = false;

    double stakeInSol() const { return static_cast<double>(activatedStake) / 1e9; }

    static ValidatorInfo fromRpcJson(const QJsonObject& obj) {
        ValidatorInfo v;
        v.voteAccount = obj["votePubkey"].toString();
        v.nodePubkey = obj["nodePubkey"].toString();
        v.commission = obj["commission"].toInt();
        v.activatedStake = static_cast<quint64>(obj["activatedStake"].toDouble());
        v.lastVote = static_cast<quint64>(obj["lastVote"].toDouble());

        QJsonArray epochCreditsArr = obj["epochCredits"].toArray();
        if (!epochCreditsArr.isEmpty()) {
            QJsonArray last = epochCreditsArr.last().toArray();
            if (last.size() >= 3) {
                v.epochCredits = static_cast<quint64>(last[1].toDouble() - last[2].toDouble());
            }
        }
        return v;
    }

    void mergeMarinade(const QJsonObject& obj) {
        QString n = obj["info_name"].toString();
        if (!n.isEmpty()) {
            name = n;
        }
        QString icon = obj["info_icon_url"].toString();
        if (!icon.isEmpty()) {
            avatarUrl = icon;
        }
        version = obj["version"].toString();
        city = obj["dc_city"].toString();
        country = obj["dc_country"].toString();
        superminority = obj["superminority"].toBool();
        uptimePct = obj["avg_uptime_pct"].toDouble();
        if (obj.contains("score") && !obj["score"].isNull()) {
            score = obj["score"].toInt();
        }
    }
};

Q_DECLARE_METATYPE(ValidatorInfo)

#endif // VALIDATORINFO_H
