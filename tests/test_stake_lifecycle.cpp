#include "features/staking/StakeLifecycle.h"
#include <QJsonObject>
#include <gtest/gtest.h>
#include <limits>

namespace {

    StakeAccountInfo makeDelegatedStake() {
        StakeAccountInfo info;
        info.address = "stake-address";
        info.lamports = 7'286'000;
        info.rentExemptReserve = 2'283'000;
        info.voteAccount = "vote-account";
        info.stake = 5'003'000;
        info.activationEpoch = 946;
        info.deactivationEpoch = std::numeric_limits<quint64>::max();
        return info;
    }

} // namespace

TEST(StakeLifecycleTest, ActiveStakeCanBeUnstaked) {
    StakeAccountInfo info = makeDelegatedStake();

    const StakeLifecycle lifecycle = StakeLifecycle::derive(info, 949, 0);

    EXPECT_EQ(lifecycle.phase, StakeLifecycle::Phase::Active);
    EXPECT_EQ(lifecycle.statusLabel, "Active");
    EXPECT_TRUE(lifecycle.canUnstake);
    EXPECT_FALSE(lifecycle.canWithdraw);
    EXPECT_TRUE(lifecycle.actionEnabled);
    EXPECT_EQ(lifecycle.actionLabel, "Unstake");
    EXPECT_EQ(lifecycle.activeLamports, 5'003'000u);
    EXPECT_EQ(lifecycle.inactiveLamports, 2'283'000u);
}

TEST(StakeLifecycleTest, ActivatingStakeIsNotYetActionable) {
    StakeAccountInfo info = makeDelegatedStake();
    info.activationEpoch = 950;

    const StakeLifecycle lifecycle = StakeLifecycle::derive(info, 949, 0);

    EXPECT_EQ(lifecycle.phase, StakeLifecycle::Phase::Activating);
    EXPECT_EQ(lifecycle.statusLabel, "Activating");
    EXPECT_FALSE(lifecycle.actionEnabled);
    EXPECT_EQ(lifecycle.actionLabel, "Activating");
    EXPECT_EQ(lifecycle.activeLamports, 0u);
}

TEST(StakeLifecycleTest, DeactivationEpochEqualCurrentEpochMeansDeactivating) {
    StakeAccountInfo info = makeDelegatedStake();
    info.deactivationEpoch = 949;

    const StakeLifecycle lifecycle = StakeLifecycle::derive(info, 949, 0);

    EXPECT_EQ(lifecycle.phase, StakeLifecycle::Phase::Deactivating);
    EXPECT_EQ(lifecycle.statusLabel, "Deactivating");
    EXPECT_FALSE(lifecycle.canWithdraw);
    EXPECT_FALSE(lifecycle.actionEnabled);
    EXPECT_EQ(lifecycle.actionLabel, "Deactivating");
    EXPECT_EQ(lifecycle.activeLamports, 5'003'000u);
}

TEST(StakeLifecycleTest, DeactivationEpochPastCurrentEpochMeansInactive) {
    StakeAccountInfo info = makeDelegatedStake();
    info.deactivationEpoch = 948;

    const StakeLifecycle lifecycle = StakeLifecycle::derive(info, 949, 0);

    EXPECT_EQ(lifecycle.phase, StakeLifecycle::Phase::Inactive);
    EXPECT_EQ(lifecycle.statusLabel, "Inactive");
    EXPECT_TRUE(lifecycle.canWithdraw);
    EXPECT_TRUE(lifecycle.actionEnabled);
    EXPECT_EQ(lifecycle.actionLabel, "Withdraw All");
    EXPECT_EQ(lifecycle.withdrawableLamports, 7'286'000u);
}

TEST(StakeLifecycleTest, LockupBlocksWithdrawalForInactiveStake) {
    StakeAccountInfo info = makeDelegatedStake();
    info.deactivationEpoch = 948;
    info.lockupUnixTimestamp = 100;

    const StakeLifecycle lifecycle = StakeLifecycle::derive(info, 949, 50);

    EXPECT_EQ(lifecycle.phase, StakeLifecycle::Phase::Inactive);
    EXPECT_TRUE(lifecycle.lockupActive);
    EXPECT_FALSE(lifecycle.canWithdraw);
    EXPECT_FALSE(lifecycle.actionEnabled);
    EXPECT_EQ(lifecycle.actionLabel, "Locked");
    EXPECT_EQ(lifecycle.withdrawableLamports, 0u);
}

TEST(StakeLifecycleTest, InitializedStakeHasNoDelegationAction) {
    StakeAccountInfo info;
    info.address = "stake-address";
    info.lamports = 2'283'000;
    info.rentExemptReserve = 2'283'000;

    const StakeLifecycle lifecycle = StakeLifecycle::derive(info, 949, 0);

    EXPECT_EQ(lifecycle.phase, StakeLifecycle::Phase::Initialized);
    EXPECT_EQ(lifecycle.statusLabel, "Initialized");
    EXPECT_FALSE(lifecycle.canUnstake);
    EXPECT_FALSE(lifecycle.canWithdraw);
    EXPECT_FALSE(lifecycle.actionEnabled);
    EXPECT_EQ(lifecycle.actionLabel, "Unavailable");
}

TEST(StakeLifecycleTest, ParsedStakeKeepsBoundaryAsDeactivating) {
    QJsonObject delegation{
        {"voter", "vote-account"},
        {"stake", "5002923"},
        {"activationEpoch", "946"},
        {"deactivationEpoch", "949"},
    };
    QJsonObject stake{
        {"delegation", delegation},
    };
    QJsonObject authorized{
        {"staker", "staker"},
        {"withdrawer", "withdrawer"},
    };
    QJsonObject lockup{
        {"custodian", ""},
        {"epoch", "0"},
        {"unixTimestamp", 0},
    };
    QJsonObject meta{
        {"authorized", authorized},
        {"rentExemptReserve", "2283000"},
        {"lockup", lockup},
    };
    QJsonObject info{
        {"meta", meta},
        {"stake", stake},
    };
    QJsonObject parsed{
        {"type", "delegated"},
        {"info", info},
    };
    QJsonObject data{
        {"parsed", parsed},
    };
    QJsonObject account{
        {"lamports", 7286000.0},
        {"space", 200.0},
        {"data", data},
    };

    const StakeAccountInfo parsedInfo =
        StakeAccountInfo::fromJsonParsed("stake-address", account, 949);

    EXPECT_EQ(parsedInfo.state, StakeAccountInfo::State::Deactivating);
}
