#include "StakeAccountRewardDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QSqlDatabase>
#include <QSqlQuery>

QSqlDatabase StakeAccountRewardDb::db() { return Database::connection(); }

static StakeRewardInfo rewardFromQuery(const QSqlQuery& q) {
    StakeRewardInfo reward;
    reward.epoch = static_cast<quint64>(q.value("epoch").toLongLong());
    reward.lamports = q.value("lamports").toLongLong();
    reward.postBalance = static_cast<quint64>(q.value("post_balance").toLongLong());
    reward.effectiveSlot = static_cast<quint64>(q.value("effective_slot").toLongLong());
    reward.commission = q.value("commission").isNull() ? -1 : q.value("commission").toInt();
    return reward;
}

QList<StakeRewardInfo> StakeAccountRewardDb::load(const QString& stakeAddress) {
    return DbUtil::many<StakeRewardInfo>(
        db(),
        "SELECT epoch, lamports, post_balance, effective_slot, commission "
        "FROM stake_account_rewards WHERE stake_address = :addr ORDER BY epoch DESC",
        {{":addr", stakeAddress}}, rewardFromQuery);
}

void StakeAccountRewardDb::upsert(const QString& stakeAddress, const StakeRewardInfo& reward) {
    static const QString kUpsert = R"(
        INSERT INTO stake_account_rewards
            (stake_address, epoch, lamports, post_balance, effective_slot, commission, updated_at)
        VALUES
            (:addr, :epoch, :lamports, :post_balance, :effective_slot, :commission,
             strftime('%s', 'now'))
        ON CONFLICT(stake_address, epoch) DO UPDATE SET
            lamports = excluded.lamports,
            post_balance = excluded.post_balance,
            effective_slot = excluded.effective_slot,
            commission = excluded.commission,
            updated_at = strftime('%s', 'now')
    )";

    DbUtil::exec(
        db(), kUpsert,
        {{":addr", stakeAddress},
         {":epoch", static_cast<qlonglong>(reward.epoch)},
         {":lamports", static_cast<qlonglong>(reward.lamports)},
         {":post_balance", static_cast<qlonglong>(reward.postBalance)},
         {":effective_slot", static_cast<qlonglong>(reward.effectiveSlot)},
         {":commission", reward.commission >= 0 ? QVariant(reward.commission) : QVariant()}});
}

std::optional<quint64> StakeAccountRewardDb::maxEpoch(const QString& stakeAddress) {
    auto value = DbUtil::scalarInt(
        db(), "SELECT MAX(epoch) FROM stake_account_rewards WHERE stake_address = :addr",
        {{":addr", stakeAddress}});
    if (!value.has_value()) {
        return std::nullopt;
    }
    return static_cast<quint64>(*value);
}

quint64 StakeAccountRewardDb::totalLamports(const QString& stakeAddress) {
    auto value = DbUtil::scalarString(
        db(),
        "SELECT COALESCE(SUM(lamports), 0) FROM stake_account_rewards WHERE stake_address = :addr",
        {{":addr", stakeAddress}});
    if (!value.has_value()) {
        return 0;
    }
    return static_cast<quint64>(value->toULongLong());
}
