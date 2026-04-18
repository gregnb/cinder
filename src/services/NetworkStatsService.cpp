#include "NetworkStatsService.h"
#include "SolanaApi.h"
#include "db/NetworkStatsDb.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

static const int FAST_INTERVAL_MS = 30000;  // 30 seconds
static const int SLOW_INTERVAL_MS = 300000; // 5 minutes

NetworkStatsService::NetworkStatsService(SolanaApi* api, QObject* parent)
    : QObject(parent), m_api(api) {

    m_fastTimer.setInterval(FAST_INTERVAL_MS);
    m_slowTimer.setInterval(SLOW_INTERVAL_MS);
    connect(&m_fastTimer, &QTimer::timeout, this, &NetworkStatsService::fetchFast);
    connect(&m_slowTimer, &QTimer::timeout, this, &NetworkStatsService::fetchSlow);

    // Epoch info
    connect(m_api, &SolanaApi::epochInfoReady, this,
            [this](quint64 epoch, quint64 slotIndex, quint64 slotsInEpoch, quint64 absoluteSlot) {
                m_stats.epoch = epoch;
                m_stats.slotIndex = slotIndex;
                m_stats.slotsInEpoch = slotsInEpoch;
                m_stats.absoluteSlot = absoluteSlot;
                m_stats.epochProgressPct =
                    slotsInEpoch > 0 ? static_cast<double>(slotIndex) / slotsInEpoch * 100.0 : 0;
                scheduleEmit();
            });

    // Performance samples → TPS with vote/non-vote breakdown
    connect(m_api, &SolanaApi::performanceSamplesReady, this, [this](const QJsonArray& samples) {
        m_stats.tpsSamples.clear();
        qint64 now = QDateTime::currentSecsSinceEpoch();
        // Samples are newest-first from the RPC; reverse for chronological order
        for (int i = samples.size() - 1; i >= 0; --i) {
            QJsonObject s = samples[i].toObject();
            double period = s["samplePeriodSecs"].toDouble();
            double totalTx = s["numTransactions"].toDouble();
            double nonVoteTx = s["numNonVoteTransactions"].toDouble();

            TpsSample sample;
            sample.totalTps = period > 0 ? totalTx / period : 0;
            sample.nonVoteTps = period > 0 ? nonVoteTx / period : 0;
            sample.voteTps = sample.totalTps - sample.nonVoteTps;
            // Approximate timestamp: i counts down from newest, output is oldest-first
            int outputIdx = samples.size() - 1 - i;
            sample.timestamp = now - static_cast<qint64>((samples.size() - 1 - outputIdx) * period);
            m_stats.tpsSamples.append(sample);
        }
        if (!m_stats.tpsSamples.isEmpty()) {
            m_stats.currentTps = m_stats.tpsSamples.last().totalTps;
        }
        scheduleEmit();
    });

    // Block height
    connect(m_api, &SolanaApi::blockHeightReady, this, [this](quint64 height) {
        m_stats.blockHeight = height;
        scheduleEmit();
    });

    // Supply
    connect(m_api, &SolanaApi::supplyReady, this, [this](quint64 total, quint64 circulating) {
        m_stats.totalSupply = total;
        m_stats.circulatingSupply = circulating;
        scheduleEmit();
    });

    // Inflation
    connect(m_api, &SolanaApi::inflationRateReady, this,
            [this](double total, double, double, double) {
                m_stats.inflationRate = total * 100.0; // fraction → percentage
                scheduleEmit();
            });

    // Vote accounts → validator count, active stake, delinquent %
    connect(m_api, &SolanaApi::voteAccountsReady, this,
            [this](const QJsonArray& current, const QJsonArray& delinquent) {
                m_stats.validatorCount = current.size();
                quint64 activeStake = 0;
                for (const auto& v : current) {
                    activeStake += static_cast<quint64>(v.toObject()["activatedStake"].toDouble());
                }
                quint64 delinquentStake = 0;
                for (const auto& v : delinquent) {
                    delinquentStake +=
                        static_cast<quint64>(v.toObject()["activatedStake"].toDouble());
                }
                m_stats.activeStake = activeStake;
                quint64 totalStake = activeStake + delinquentStake;
                m_stats.delinquentPct =
                    totalStake > 0 ? static_cast<double>(delinquentStake) / totalStake * 100.0 : 0;
                scheduleEmit();
            });

    // Version
    connect(m_api, &SolanaApi::versionReady, this, [this](const QString& version) {
        m_stats.solanaVersion = version;
        scheduleEmit();
    });

    // Connection status: mark disconnected on fast-poll failures
    connect(m_api, &SolanaApi::requestFailed, this, [this](const QString& method, const QString&) {
        if (m_connected && (method == "getEpochInfo" || method == "getRecentPerformanceSamples" ||
                            method == "getBlockHeight")) {
            m_connected = false;
            emit connectionStatusChanged(false);
        }
    });
}

void NetworkStatsService::start() {
    loadFromCache();

    QTimer::singleShot(0, this, &NetworkStatsService::fetchFast);
    QTimer::singleShot(0, this, &NetworkStatsService::fetchSlow);
    m_fastTimer.start();
    m_slowTimer.start();
}

void NetworkStatsService::stop() {
    m_fastTimer.stop();
    m_slowTimer.stop();
    m_paused = false;
}

void NetworkStatsService::pause() {
    if (m_paused) {
        return;
    }
    m_paused = true;
    m_fastTimer.stop();
    m_slowTimer.stop();
    qDebug() << "[NetworkStatsService] Paused";
}

void NetworkStatsService::resume() {
    if (!m_paused) {
        return;
    }
    m_paused = false;
    m_fastTimer.start();
    m_slowTimer.start();
    qDebug() << "[NetworkStatsService] Resumed";
}

void NetworkStatsService::fetchNow() {
    fetchFast();
    fetchSlow();
}

bool NetworkStatsService::isPaused() const { return m_paused; }

void NetworkStatsService::fetchFast() {
    m_api->fetchEpochInfo();
    m_api->fetchRecentPerformanceSamples(30);
    m_api->fetchBlockHeight();
}

void NetworkStatsService::fetchSlow() {
    m_api->fetchSupply();
    m_api->fetchInflationRate();
    m_api->fetchVoteAccounts();
    m_api->fetchVersion();
}

void NetworkStatsService::scheduleEmit() {
    if (!m_connected) {
        m_connected = true;
        emit connectionStatusChanged(true);
    }
    if (!m_emitPending) {
        m_emitPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_emitPending = false;
            emit statsUpdated();
            saveToCache();
        });
    }
}

// ── Cache persistence ────────────────────────────────────────

void NetworkStatsService::loadFromCache() {
    if (NetworkStatsDb::load(m_stats)) {
        emit statsUpdated();
    }
}

void NetworkStatsService::saveToCache() { NetworkStatsDb::save(m_stats); }
