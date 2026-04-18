#include "NetworkStatsDb.h"
#include "Database.h"
#include "DbUtil.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>

QSqlDatabase NetworkStatsDb::db() { return Database::connection(); }

bool NetworkStatsDb::save(const NetworkStats& stats) {
    // Serialize TPS samples to compact JSON
    QJsonArray samplesArr;
    for (const auto& s : stats.tpsSamples) {
        QJsonObject obj;
        obj["t"] = s.totalTps;
        obj["v"] = s.voteTps;
        obj["n"] = s.nonVoteTps;
        obj["ts"] = static_cast<double>(s.timestamp);
        samplesArr.append(obj);
    }
    QString samplesJson =
        QString::fromUtf8(QJsonDocument(samplesArr).toJson(QJsonDocument::Compact));

    static const QString kSql = R"(
        INSERT OR REPLACE INTO network_stats_cache
            (id, epoch, slot_index, slots_in_epoch, epoch_progress_pct, current_tps,
             tps_samples, total_supply, circulating_supply, active_stake,
             delinquent_pct, validator_count, absolute_slot, block_height,
             inflation_rate, solana_version, updated_at)
        VALUES
            (1, :epoch, :slot_index, :slots_in_epoch, :epoch_progress_pct, :current_tps,
             :tps_samples, :total_supply, :circulating_supply, :active_stake,
             :delinquent_pct, :validator_count, :absolute_slot, :block_height,
             :inflation_rate, :solana_version, :updated_at)
    )";

    return DbUtil::exec(db(), kSql,
                        {{":epoch", static_cast<qint64>(stats.epoch)},
                         {":slot_index", static_cast<qint64>(stats.slotIndex)},
                         {":slots_in_epoch", static_cast<qint64>(stats.slotsInEpoch)},
                         {":epoch_progress_pct", stats.epochProgressPct},
                         {":current_tps", stats.currentTps},
                         {":tps_samples", samplesJson},
                         {":total_supply", static_cast<qint64>(stats.totalSupply)},
                         {":circulating_supply", static_cast<qint64>(stats.circulatingSupply)},
                         {":active_stake", static_cast<qint64>(stats.activeStake)},
                         {":delinquent_pct", stats.delinquentPct},
                         {":validator_count", stats.validatorCount},
                         {":absolute_slot", static_cast<qint64>(stats.absoluteSlot)},
                         {":block_height", static_cast<qint64>(stats.blockHeight)},
                         {":inflation_rate", stats.inflationRate},
                         {":solana_version", stats.solanaVersion},
                         {":updated_at", QDateTime::currentSecsSinceEpoch()}});
}

bool NetworkStatsDb::load(NetworkStats& stats) {
    auto row = DbUtil::one<QSqlRecord>(db(), "SELECT * FROM network_stats_cache WHERE id = 1", {},
                                       [](const QSqlQuery& q) { return q.record(); });
    if (!row.has_value()) {
        return false;
    }

    const QSqlRecord& r = row.value();

    stats.epoch = r.value("epoch").toULongLong();
    stats.slotIndex = r.value("slot_index").toULongLong();
    stats.slotsInEpoch = r.value("slots_in_epoch").toULongLong();
    stats.epochProgressPct = r.value("epoch_progress_pct").toDouble();
    stats.currentTps = r.value("current_tps").toDouble();
    stats.totalSupply = r.value("total_supply").toULongLong();
    stats.circulatingSupply = r.value("circulating_supply").toULongLong();
    stats.activeStake = r.value("active_stake").toULongLong();
    stats.delinquentPct = r.value("delinquent_pct").toDouble();
    stats.validatorCount = r.value("validator_count").toInt();
    stats.absoluteSlot = r.value("absolute_slot").toULongLong();
    stats.blockHeight = r.value("block_height").toULongLong();
    stats.inflationRate = r.value("inflation_rate").toDouble();
    stats.solanaVersion = r.value("solana_version").toString();

    // Deserialize TPS samples from JSON
    QJsonDocument doc = QJsonDocument::fromJson(r.value("tps_samples").toString().toUtf8());
    if (doc.isArray()) {
        for (const auto& val : doc.array()) {
            QJsonObject obj = val.toObject();
            TpsSample s;
            s.totalTps = obj["t"].toDouble();
            s.voteTps = obj["v"].toDouble();
            s.nonVoteTps = obj["n"].toDouble();
            s.timestamp = static_cast<qint64>(obj["ts"].toDouble());
            stats.tpsSamples.append(s);
        }
    }

    return true;
}
