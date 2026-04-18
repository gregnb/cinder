#ifndef NETWORKSTATSSERVICE_H
#define NETWORKSTATSSERVICE_H

#include "services/model/NetworkStats.h"
#include <QObject>
#include <QTimer>

class SolanaApi;

class NetworkStatsService : public QObject {
    Q_OBJECT

  public:
    explicit NetworkStatsService(SolanaApi* api, QObject* parent = nullptr);

    void start();
    void stop();
    void pause();
    void resume();
    void fetchNow();
    bool isPaused() const;

    const NetworkStats& stats() const { return m_stats; }

  signals:
    void statsUpdated();
    void connectionStatusChanged(bool connected);

  private:
    void fetchFast();
    void fetchSlow();
    void scheduleEmit();
    void loadFromCache();
    void saveToCache();

    SolanaApi* m_api;
    NetworkStats m_stats;

    QTimer m_fastTimer; // 30s — epoch, TPS, block height
    QTimer m_slowTimer; // 5min — supply, inflation, validators, version

    bool m_emitPending = false;
    bool m_connected = false;
    bool m_paused = false;
};

#endif // NETWORKSTATSSERVICE_H
