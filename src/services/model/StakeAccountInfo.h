#ifndef STAKEACCOUNTINFO_H
#define STAKEACCOUNTINFO_H

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <limits>

struct StakeAccountInfo {
    enum class State { Uninitialized, Initialized, Activating, Active, Deactivating, Inactive };

    QString address;
    quint64 lamports = 0;
    quint64 rentExemptReserve = 0;
    QString staker;
    QString withdrawer;
    QString voteAccount;
    quint64 activationEpoch = 0;
    quint64 deactivationEpoch = 0;
    quint64 lockupEpoch = 0;
    qint64 lockupUnixTimestamp = 0;
    QString lockupCustodian;
    quint64 allocatedDataSize = 0;
    quint64 stake = 0;
    quint64 totalRewardsLamports = 0;
    State state = State::Uninitialized;

    double solAmount() const { return static_cast<double>(lamports) / 1e9; }
    double stakeAmount() const { return static_cast<double>(stake) / 1e9; }
    double totalRewardsSol() const { return static_cast<double>(totalRewardsLamports) / 1e9; }

    QString stateString() const {
        switch (state) {
            case State::Activating:
                return "Activating";
            case State::Active:
                return "Active";
            case State::Deactivating:
                return "Deactivating";
            case State::Inactive:
                return "Inactive";
            case State::Initialized:
                return "Initialized";
            default:
                return "Unknown";
        }
    }

    static StakeAccountInfo fromJsonParsed(const QString& pubkey, const QJsonObject& accountObj,
                                           quint64 currentEpoch) {
        StakeAccountInfo info;
        info.address = pubkey;
        info.lamports = static_cast<quint64>(accountObj["lamports"].toDouble());
        info.allocatedDataSize = static_cast<quint64>(accountObj["space"].toDouble());

        QJsonObject parsed = accountObj["data"].toObject()["parsed"].toObject();
        QJsonObject stakeInfo = parsed["info"].toObject();
        QString type = parsed["type"].toString();

        QJsonObject meta = stakeInfo["meta"].toObject();
        QJsonObject authorized = meta["authorized"].toObject();
        QJsonObject lockup = meta["lockup"].toObject();
        info.staker = authorized["staker"].toString();
        info.withdrawer = authorized["withdrawer"].toString();
        info.rentExemptReserve =
            static_cast<quint64>(meta["rentExemptReserve"].toString().toDouble());
        info.lockupCustodian = lockup["custodian"].toString();
        info.lockupEpoch = lockup["epoch"].toString().toULongLong();
        info.lockupUnixTimestamp = lockup["unixTimestamp"].toVariant().toLongLong();

        if (type == "delegated") {
            QJsonObject stakeData = stakeInfo["stake"].toObject();
            QJsonObject delegation = stakeData["delegation"].toObject();
            info.voteAccount = delegation["voter"].toString();
            info.stake = static_cast<quint64>(delegation["stake"].toString().toDouble());
            info.activationEpoch = delegation["activationEpoch"].toString().toULongLong();
            info.deactivationEpoch = delegation["deactivationEpoch"].toString().toULongLong();

            constexpr quint64 maxU64 = std::numeric_limits<quint64>::max();
            if (info.deactivationEpoch < maxU64 && info.deactivationEpoch < currentEpoch) {
                info.state = State::Inactive;
            } else if (info.deactivationEpoch < maxU64 && info.deactivationEpoch == currentEpoch) {
                info.state = State::Deactivating;
            } else if (info.deactivationEpoch < maxU64) {
                info.state = State::Active;
            } else if (info.activationEpoch <= currentEpoch) {
                info.state = State::Active;
            } else {
                info.state = State::Activating;
            }
        } else {
            info.state = State::Initialized;
        }

        return info;
    }
};

Q_DECLARE_METATYPE(StakeAccountInfo)

#endif // STAKEACCOUNTINFO_H
