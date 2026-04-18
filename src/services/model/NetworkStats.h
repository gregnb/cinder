#ifndef NETWORKSTATS_H
#define NETWORKSTATS_H

#include <QList>
#include <QString>

struct TpsSample {
    double totalTps = 0;
    double voteTps = 0;
    double nonVoteTps = 0;
    qint64 timestamp = 0; // approx unix timestamp
};

struct NetworkStats {
    // Epoch
    quint64 epoch = 0;
    quint64 slotIndex = 0;
    quint64 slotsInEpoch = 0;
    double epochProgressPct = 0;

    // TPS (from getRecentPerformanceSamples)
    double currentTps = 0;
    QList<TpsSample> tpsSamples;

    // Supply (in lamports)
    quint64 totalSupply = 0;
    quint64 circulatingSupply = 0;

    // Stake
    quint64 activeStake = 0;
    double delinquentPct = 0;
    int validatorCount = 0;

    // Misc
    quint64 absoluteSlot = 0;
    quint64 blockHeight = 0;
    double inflationRate = 0;
    QString solanaVersion;
};

#endif // NETWORKSTATS_H
