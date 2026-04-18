#include "ValidatorService.h"
#include "SolanaApi.h"
#include "db/ValidatorCacheDb.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <algorithm>

static const QString MARINADE_API_URL =
    "https://validators-api.marinade.finance/validators?limit=10000";

ValidatorService::ValidatorService(SolanaApi* api, QObject* parent) : QObject(parent), m_api(api) {}

void ValidatorService::refresh() {
    if (m_refreshing) {
        return;
    }
    m_refreshing = true;

    // Reset state
    m_validators.clear();
    m_appData.clear();
    m_gotVoteAccounts = false;
    m_gotInflation = false;
    m_gotEpochInfo = false;
    m_gotSupply = false;
    m_gotAppData = false;
    m_inflationRate = 0.0;
    m_totalStaked = 0;
    m_totalSupply = 0;

    // Pre-load cached names from DB for instant display
    QList<ValidatorCacheRecord> cached = ValidatorCacheDb::getAllRecords();
    for (const auto& row : cached) {
        QJsonObject obj;
        obj["info_name"] = row.name;
        obj["info_icon_url"] = row.avatarUrl;
        obj["score"] = row.score;
        obj["version"] = row.version;
        obj["dc_city"] = row.city;
        obj["dc_country"] = row.country;
        m_appData[row.voteAccount] = obj;
    }

    // Connect RPC signals (one-shot via shared_ptr flag or context)
    auto voteConn = std::make_shared<QMetaObject::Connection>();
    *voteConn = connect(m_api, &SolanaApi::voteAccountsReady, this,
                        [this, voteConn](const QJsonArray& current, const QJsonArray& delinquent) {
                            disconnect(*voteConn);

                            m_totalStaked = 0;
                            for (const auto& v : current) {
                                ValidatorInfo info = ValidatorInfo::fromRpcJson(v.toObject());
                                m_totalStaked += info.activatedStake;
                                m_validators.append(info);
                            }
                            for (const auto& v : delinquent) {
                                ValidatorInfo info = ValidatorInfo::fromRpcJson(v.toObject());
                                info.delinquent = true;
                                m_totalStaked += info.activatedStake;
                                m_validators.append(info);
                            }

                            m_gotVoteAccounts = true;
                            tryFinalize();
                        });

    auto inflConn = std::make_shared<QMetaObject::Connection>();
    *inflConn = connect(m_api, &SolanaApi::inflationRateReady, this,
                        [this, inflConn](double total, double /*validator*/, double /*foundation*/,
                                         double /*epoch*/) {
                            disconnect(*inflConn);
                            m_inflationRate = total;
                            m_gotInflation = true;
                            tryFinalize();
                        });

    auto epochConn = std::make_shared<QMetaObject::Connection>();
    *epochConn = connect(m_api, &SolanaApi::epochInfoReady, this,
                         [this, epochConn](quint64 epoch, quint64 /*slotIndex*/,
                                           quint64 /*slotsInEpoch*/, quint64 /*absoluteSlot*/) {
                             disconnect(*epochConn);
                             m_currentEpoch = epoch;
                             m_gotEpochInfo = true;
                             tryFinalize();
                         });

    auto supplyConn = std::make_shared<QMetaObject::Connection>();
    *supplyConn = connect(m_api, &SolanaApi::supplyReady, this,
                          [this, supplyConn](quint64 total, quint64 /*circulating*/) {
                              disconnect(*supplyConn);
                              m_totalSupply = total;
                              m_gotSupply = true;
                              tryFinalize();
                          });

    disconnect(m_failConn);
    m_failConn = connect(m_api, &SolanaApi::requestFailed, this,
                         [this](const QString& method, const QString& err) {
                             if (method == "getVoteAccounts" || method == "getInflationRate" ||
                                 method == "getEpochInfo" || method == "getSupply") {
                                 disconnect(m_failConn);
                                 m_refreshing = false;
                                 emit error("RPC error (" + method + "): " + err);
                             }
                         });

    // Fire all 4 RPC calls in parallel
    m_api->fetchVoteAccounts();
    m_api->fetchInflationRate();
    m_api->fetchEpochInfo();
    m_api->fetchSupply();

    // Fire Marinade API request (optional — failure is non-fatal)
    fetchMarinadeData();
}

void ValidatorService::fetchMarinadeData() {
    QUrl url(MARINADE_API_URL);
    QNetworkRequest request{url};
    request.setRawHeader("User-Agent", "Cinder/0.1.0");

    QNetworkReply* reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[ValidatorService] Marinade API fetch failed:" << reply->errorString();
            m_gotAppData = true;
            tryFinalize();
            return;
        }

        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            qWarning() << "[ValidatorService] Marinade API parse failed:" << parseErr.errorString();
            m_gotAppData = true;
            tryFinalize();
            return;
        }

        QJsonObject root = doc.object();
        QJsonArray arr = root["validators"].toArray();

        // Overwrite cached data with fresh results
        m_appData.clear();
        for (const auto& v : arr) {
            QJsonObject obj = v.toObject();
            QString voteAccount = obj["vote_account"].toString();
            if (!voteAccount.isEmpty()) {
                m_appData[voteAccount] = obj;

                // Update DB cache
                ValidatorCacheDb::upsert(
                    voteAccount, obj["info_name"].toString(), obj["info_icon_url"].toString(),
                    obj["score"].isNull() ? 0 : obj["score"].toInt(), obj["version"].toString(),
                    obj["dc_city"].toString(), obj["dc_country"].toString());
            }
        }

        qDebug() << "[ValidatorService] Cached" << m_appData.size()
                 << "validators from Marinade API";
        m_gotAppData = true;
        tryFinalize();
    });
}

void ValidatorService::tryFinalize() {
    if (!m_gotVoteAccounts || !m_gotInflation || !m_gotEpochInfo || !m_gotSupply || !m_gotAppData) {
        return;
    }

    // Merge Marinade metadata
    for (auto& v : m_validators) {
        auto it = m_appData.constFind(v.voteAccount);
        if (it != m_appData.constEnd()) {
            v.mergeMarinade(it.value());
        }
        // Fallback: use truncated vote account as name
        if (v.name.isEmpty()) {
            v.name = v.voteAccount.left(8) + "...";
        }
    }

    computeApys();

    // Sort by activated stake descending
    std::sort(m_validators.begin(), m_validators.end(),
              [](const ValidatorInfo& a, const ValidatorInfo& b) {
                  return a.activatedStake > b.activatedStake;
              });

    // Free the raw Marinade JSON — all needed fields are now in m_validators
    m_appData.clear();

    disconnect(m_failConn);
    m_refreshing = false;
    emit validatorsReady(m_validators);
}

void ValidatorService::computeApys() {
    if (m_totalSupply == 0 || m_totalStaked == 0 || m_inflationRate <= 0.0) {
        return;
    }

    double stakingRatio = static_cast<double>(m_totalStaked) / static_cast<double>(m_totalSupply);
    double baseApy = m_inflationRate / stakingRatio;

    // Compute average credits across all non-delinquent validators
    quint64 totalCredits = 0;
    int creditCount = 0;
    for (const auto& v : std::as_const(m_validators)) {
        if (!v.delinquent && v.epochCredits > 0) {
            totalCredits += v.epochCredits;
            creditCount++;
        }
    }

    double avgCredits = creditCount > 0 ? static_cast<double>(totalCredits) / creditCount : 1.0;

    for (auto& v : m_validators) {
        double commissionFactor = 1.0 - (static_cast<double>(v.commission) / 100.0);
        double creditFactor =
            avgCredits > 0 ? static_cast<double>(v.epochCredits) / avgCredits : 0.0;
        v.apy = baseApy * commissionFactor * creditFactor * 100.0;

        // Clamp delinquent validators to 0 APY
        if (v.delinquent) {
            v.apy = 0.0;
        }
    }
}

QString ValidatorService::validatorName(const QString& voteAccount) const {
    // Check in-memory list first
    for (const auto& v : m_validators) {
        if (v.voteAccount == voteAccount) {
            return v.name;
        }
    }
    // Fall back to DB cache
    return ValidatorCacheDb::getName(voteAccount);
}
