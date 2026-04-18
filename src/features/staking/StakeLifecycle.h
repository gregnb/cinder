#ifndef STAKELIFECYCLE_H
#define STAKELIFECYCLE_H

#include "services/model/StakeAccountInfo.h"
#include <QDateTime>
#include <QString>
#include <algorithm>
#include <limits>

struct StakeLifecycle {
    enum class Phase { Uninitialized, Initialized, Activating, Active, Deactivating, Inactive };

    Phase phase = Phase::Uninitialized;
    bool hasDelegation = false;
    bool lockupActive = false;
    bool canUnstake = false;
    bool canWithdraw = false;
    bool actionEnabled = false;

    quint64 delegatedLamports = 0;
    quint64 activeLamports = 0;
    quint64 inactiveLamports = 0;
    quint64 withdrawableLamports = 0;

    QString statusLabel;
    QString actionLabel;
    QString description;
    int currentStepIndex = 0;

    static Phase derivePhase(const StakeAccountInfo& info, quint64 currentEpoch) {
        constexpr quint64 maxU64 = std::numeric_limits<quint64>::max();

        if (info.voteAccount.isEmpty()) {
            return Phase::Initialized;
        }

        if (info.deactivationEpoch < maxU64) {
            if (info.deactivationEpoch < currentEpoch) {
                return Phase::Inactive;
            }
            if (info.deactivationEpoch == currentEpoch) {
                return Phase::Deactivating;
            }
            return Phase::Active;
        }

        if (info.activationEpoch >= currentEpoch) {
            return Phase::Activating;
        }

        return Phase::Active;
    }

    static QString phaseLabel(Phase phase) {
        switch (phase) {
            case Phase::Activating:
                return QStringLiteral("Activating");
            case Phase::Active:
                return QStringLiteral("Active");
            case Phase::Deactivating:
                return QStringLiteral("Deactivating");
            case Phase::Inactive:
                return QStringLiteral("Inactive");
            case Phase::Initialized:
                return QStringLiteral("Initialized");
            default:
                return QStringLiteral("Unknown");
        }
    }

    static StakeLifecycle derive(const StakeAccountInfo& info, quint64 currentEpoch,
                                 qint64 currentUnixTimestamp = QDateTime::currentSecsSinceEpoch()) {
        StakeLifecycle lifecycle;
        lifecycle.phase = derivePhase(info, currentEpoch);
        lifecycle.hasDelegation = !info.voteAccount.isEmpty();
        lifecycle.lockupActive = (info.lockupUnixTimestamp > currentUnixTimestamp) ||
                                 (info.lockupEpoch > 0 && info.lockupEpoch > currentEpoch);
        lifecycle.delegatedLamports = info.stake;

        const quint64 totalLamports = info.lamports;
        lifecycle.inactiveLamports = totalLamports > lifecycle.delegatedLamports
                                         ? totalLamports - lifecycle.delegatedLamports
                                         : 0;

        switch (lifecycle.phase) {
            case Phase::Active:
                lifecycle.activeLamports = lifecycle.delegatedLamports;
                lifecycle.canUnstake = true;
                lifecycle.actionEnabled = true;
                lifecycle.actionLabel = QStringLiteral("Unstake");
                lifecycle.currentStepIndex = 2;
                lifecycle.description =
                    QStringLiteral("Currently staked and eligible for rewards. If you unstake, "
                                   "this position enters cooldown before it becomes withdrawable.");
                break;
            case Phase::Inactive:
                lifecycle.withdrawableLamports =
                    lifecycle.lockupActive ? 0 : std::max<quint64>(totalLamports, 0);
                lifecycle.canWithdraw = !lifecycle.lockupActive;
                lifecycle.actionEnabled = lifecycle.canWithdraw;
                lifecycle.actionLabel = lifecycle.lockupActive ? QStringLiteral("Locked")
                                                               : QStringLiteral("Withdraw All");
                lifecycle.currentStepIndex = 2;
                lifecycle.description =
                    lifecycle.lockupActive
                        ? QStringLiteral(
                              "Stake cooldown is complete, but lockup still prevents withdrawal.")
                        : QStringLiteral("Stake cooldown is complete. Funds are now withdrawable.");
                break;
            case Phase::Activating:
                lifecycle.actionLabel = QStringLiteral("Activating");
                lifecycle.currentStepIndex = 1;
                lifecycle.description =
                    QStringLiteral("Stake activation is in progress. This position will become "
                                   "fully staked once activation completes.");
                break;
            case Phase::Deactivating:
                lifecycle.activeLamports = lifecycle.delegatedLamports;
                lifecycle.actionLabel = QStringLiteral("Deactivating");
                lifecycle.currentStepIndex = 1;
                lifecycle.description =
                    QStringLiteral("Unstake submitted in epoch %1. Cooldown is in progress and "
                                   "funds become withdrawable after this epoch completes.")
                        .arg(QString::number(currentEpoch));
                break;
            case Phase::Initialized:
                lifecycle.actionLabel = QStringLiteral("Unavailable");
                lifecycle.currentStepIndex = 0;
                lifecycle.description = QStringLiteral(
                    "This stake account is initialized but not currently delegated.");
                break;
            case Phase::Uninitialized:
                lifecycle.actionLabel = QStringLiteral("Unavailable");
                lifecycle.currentStepIndex = 0;
                lifecycle.description = QStringLiteral("Stake account data is not available.");
                break;
        }

        lifecycle.statusLabel = phaseLabel(lifecycle.phase);
        return lifecycle;
    }
};

#endif // STAKELIFECYCLE_H
