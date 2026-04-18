#include "StakeAccountDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <limits>

QSqlDatabase StakeAccountDb::db() { return Database::connection(); }

void StakeAccountDb::save(const QString& walletAddress, const QList<StakeAccountInfo>& accounts) {
    auto conn = db();

    QHash<QString, quint64> existingRewardTotals;
    QSqlQuery existing(conn);
    if (existing.prepare("SELECT address, total_rewards_lamports FROM stake_accounts WHERE "
                         "wallet_address = :wa")) {
        existing.bindValue(":wa", walletAddress);
        if (existing.exec()) {
            while (existing.next()) {
                existingRewardTotals.insert(
                    existing.value("address").toString(),
                    static_cast<quint64>(existing.value("total_rewards_lamports").toLongLong()));
            }
        }
    }

    // Remove old entries for this wallet, then insert fresh
    DbUtil::exec(conn, "DELETE FROM stake_accounts WHERE wallet_address = :wa",
                 {{":wa", walletAddress}});

    static const QString kInsert = R"(
        INSERT INTO stake_accounts
            (address, wallet_address, lamports, vote_account, stake, state,
             activation_epoch, deactivation_epoch, total_rewards_lamports, updated_at)
        VALUES
            (:addr, :wa, :lam, :va, :stake, :state, :ae, :de, :rewards, strftime('%s', 'now'))
    )";

    for (const auto& sa : accounts) {
        QString stateStr;
        switch (sa.state) {
            case StakeAccountInfo::State::Active:
                stateStr = "Active";
                break;
            case StakeAccountInfo::State::Activating:
                stateStr = "Activating";
                break;
            case StakeAccountInfo::State::Deactivating:
                stateStr = "Deactivating";
                break;
            case StakeAccountInfo::State::Inactive:
                stateStr = "Inactive";
                break;
            case StakeAccountInfo::State::Initialized:
                stateStr = "Initialized";
                break;
            default:
                stateStr = "Unknown";
                break;
        }

        const quint64 rewardTotal = sa.totalRewardsLamports > 0
                                        ? sa.totalRewardsLamports
                                        : existingRewardTotals.value(sa.address, 0);

        DbUtil::exec(conn, kInsert,
                     {{":addr", sa.address},
                      {":wa", walletAddress},
                      {":lam", static_cast<qlonglong>(sa.lamports)},
                      {":va", sa.voteAccount},
                      {":stake", static_cast<qlonglong>(sa.stake)},
                      {":state", stateStr},
                      {":ae", static_cast<qlonglong>(sa.activationEpoch)},
                      {":de", static_cast<qlonglong>(sa.deactivationEpoch)},
                      {":rewards", static_cast<qlonglong>(rewardTotal)}});
    }
}

static StakeAccountInfo::State parseState(const QString& s) {
    if (s == "Active") {
        return StakeAccountInfo::State::Active;
    }
    if (s == "Activating") {
        return StakeAccountInfo::State::Activating;
    }
    if (s == "Deactivating") {
        return StakeAccountInfo::State::Deactivating;
    }
    if (s == "Inactive") {
        return StakeAccountInfo::State::Inactive;
    }
    if (s == "Initialized") {
        return StakeAccountInfo::State::Initialized;
    }
    return StakeAccountInfo::State::Uninitialized;
}

static StakeAccountInfo recordFromQuery(const QSqlQuery& q) {
    StakeAccountInfo sa;
    sa.address = q.value("address").toString();
    sa.lamports = static_cast<quint64>(q.value("lamports").toLongLong());
    sa.voteAccount = q.value("vote_account").toString();
    sa.stake = static_cast<quint64>(q.value("stake").toLongLong());
    sa.state = parseState(q.value("state").toString());
    sa.activationEpoch = static_cast<quint64>(q.value("activation_epoch").toLongLong());
    sa.deactivationEpoch = static_cast<quint64>(q.value("deactivation_epoch").toLongLong());
    sa.totalRewardsLamports = static_cast<quint64>(q.value("total_rewards_lamports").toLongLong());
    return sa;
}

QList<StakeAccountInfo> StakeAccountDb::load(const QString& walletAddress) {
    return DbUtil::many<StakeAccountInfo>(db(),
                                          "SELECT * FROM stake_accounts WHERE wallet_address = :wa",
                                          {{":wa", walletAddress}}, recordFromQuery);
}

void StakeAccountDb::remove(const QString& walletAddress) {
    DbUtil::exec(db(), "DELETE FROM stake_accounts WHERE wallet_address = :wa",
                 {{":wa", walletAddress}});
}

void StakeAccountDb::setTotalRewardsLamports(const QString& stakeAddress, quint64 lamports) {
    DbUtil::exec(db(),
                 "UPDATE stake_accounts "
                 "SET total_rewards_lamports = :rewards, updated_at = strftime('%s', 'now') "
                 "WHERE address = :addr",
                 {{":rewards", static_cast<qlonglong>(lamports)}, {":addr", stakeAddress}});
}
