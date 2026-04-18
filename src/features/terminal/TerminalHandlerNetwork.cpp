#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "services/NetworkStatsService.h"
#include "services/SolanaApi.h"
#include "services/model/NetworkStats.h"

using namespace terminal;

void TerminalHandler::cmdNetwork(const QStringList& args) {
    Q_UNUSED(args);
    const NetworkStats& s = m_networkStats->stats();
    if (s.epoch == 0 && s.blockHeight == 0) {
        // No cached data — fetch fresh
        emitOutput("Fetching network stats...", kDimColor);
        auto op = asyncOp();
        op.watch(connect(
            m_api, &SolanaApi::epochInfoReady, this,
            [this](quint64 epoch, quint64 slotIndex, quint64 slotsInEpoch, quint64 absoluteSlot) {
                cancelPending();
                double progress = (slotsInEpoch > 0)
                                      ? (static_cast<double>(slotIndex) / slotsInEpoch * 100.0)
                                      : 0;
                emitOutput("  NETWORK", kAccentColor);
                emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
                emitOutput("  Epoch:           " + QString::number(epoch));
                emitOutput("  Epoch Progress:  " + QString::number(progress, 'f', 1) + "%",
                           kDimColor);
                emitOutput("  Slot:            " + QString::number(absoluteSlot), kDimColor);
                emitOutput("  Slot Index:      " + QString::number(slotIndex) + " / " +
                               QString::number(slotsInEpoch),
                           kDimColor);
            }));
        op.run([this]() { m_api->fetchEpochInfo(); });
        return;
    }

    emitOutput("  NETWORK", kAccentColor);
    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
    emitOutput("  Epoch:           " + QString::number(s.epoch));
    emitOutput("  Epoch Progress:  " + QString::number(s.epochProgressPct, 'f', 1) + "%",
               kDimColor);
    emitOutput("  Slot:            " + QString::number(s.absoluteSlot), kDimColor);
    emitOutput("  Block Height:    " + QString::number(s.blockHeight), kDimColor);
    emitOutput("  TPS:             " + QString::number(s.currentTps, 'f', 0), kDimColor);

    if (s.totalSupply > 0) {
        emitOutput("");
        emitOutput("  SUPPLY", kAccentColor);
        emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
        double totalM = s.totalSupply / 1e15; // lamports → millions SOL
        double circM = s.circulatingSupply / 1e15;
        emitOutput("  Total:           " + QString::number(totalM, 'f', 2) + "M SOL", kDimColor);
        emitOutput("  Circulating:     " + QString::number(circM, 'f', 2) + "M SOL", kDimColor);
    }

    emitOutput("");
    emitOutput("  VALIDATORS", kAccentColor);
    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
    emitOutput("  Active:          " + QString::number(s.validatorCount), kDimColor);
    emitOutput("  Delinquent:      " + QString::number(s.delinquentPct, 'f', 1) + "%", kDimColor);
    double stakeM = s.activeStake / 1e15;
    emitOutput("  Active Stake:    " + QString::number(stakeM, 'f', 2) + "M SOL", kDimColor);

    emitOutput("");
    emitOutput("  MISC", kAccentColor);
    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
    emitOutput("  Inflation:       " + QString::number(s.inflationRate, 'f', 2) + "%", kDimColor);
    if (!s.solanaVersion.isEmpty()) {
        emitOutput("  Solana Version:  " + s.solanaVersion, kDimColor);
    }
}
