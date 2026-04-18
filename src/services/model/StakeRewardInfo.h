#ifndef STAKEREWARDINFO_H
#define STAKEREWARDINFO_H

#include <QtGlobal>

struct StakeRewardInfo {
    quint64 epoch = 0;
    qint64 lamports = 0;
    quint64 postBalance = 0;
    quint64 effectiveSlot = 0;
    int commission = -1;
};

#endif // STAKEREWARDINFO_H
