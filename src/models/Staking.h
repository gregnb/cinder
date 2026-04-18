#ifndef STAKING_H
#define STAKING_H

#include <QString>
#include <QtGlobal>

struct StakeRequest {
    QString walletAddress;
    QString voteAccount;
    quint64 lamports = 0;
    quint64 rentExemptLamports = 0;
};

struct DeactivateRequest {
    QString walletAddress;
    QString stakeAccount;
};

struct WithdrawRequest {
    QString walletAddress;
    QString stakeAccount;
    quint64 lamports = 0;
};

struct StakingRefreshRequest {
    bool refreshStakeAccounts = false;
    bool refreshBalance = false;
    int delayMs = 0;
};

enum class StakingAction { Stake, Deactivate, Withdraw };

enum class StakingActionPhase { Building, Sending, Submitted, Failed };

struct StakingActionUpdate {
    StakingAction action = StakingAction::Stake;
    StakingActionPhase phase = StakingActionPhase::Building;
    QString txSignature;
    QString stakeAccount;
    QString errorCode;
    QString errorMessage;
};

#endif // STAKING_H
