#ifndef VALIDATORSERVICE_H
#define VALIDATORSERVICE_H

#include "services/model/ValidatorInfo.h"
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>

class SolanaApi;

class ValidatorService : public QObject {
    Q_OBJECT

  public:
    explicit ValidatorService(SolanaApi* api, QObject* parent = nullptr);

    void refresh();

    const QList<ValidatorInfo>& validators() const { return m_validators; }
    quint64 currentEpoch() const { return m_currentEpoch; }
    QString validatorName(const QString& voteAccount) const;

  signals:
    void validatorsReady(const QList<ValidatorInfo>& validators);
    void error(const QString& message);

  private:
    void fetchMarinadeData();
    void tryFinalize();
    void computeApys();

    SolanaApi* m_api = nullptr;
    QNetworkAccessManager m_nam;

    // Accumulated RPC data
    QList<ValidatorInfo> m_validators;
    double m_inflationRate = 0.0;
    quint64 m_totalStaked = 0;
    quint64 m_totalSupply = 0;
    quint64 m_currentEpoch = 0;

    // validators.app data keyed by vote account
    QMap<QString, QJsonObject> m_appData;

    // Completion flags
    bool m_gotVoteAccounts = false;
    bool m_gotInflation = false;
    bool m_gotEpochInfo = false;
    bool m_gotSupply = false;
    bool m_gotAppData = false;
    bool m_refreshing = false;

    QMetaObject::Connection m_failConn;
};

#endif // VALIDATORSERVICE_H
