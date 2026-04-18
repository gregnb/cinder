#ifndef STAKINGHANDLER_H
#define STAKINGHANDLER_H

#include "models/Staking.h"
#include <QByteArray>
#include <QObject>
#include <functional>

class SolanaApi;
class Signer;

class StakingHandler : public QObject {
    Q_OBJECT

  public:
    explicit StakingHandler(QObject* parent = nullptr);

    void setSolanaApi(SolanaApi* api);
    void setSigner(Signer* signer);

  public slots:
    void createStake(const StakeRequest& req);
    void deactivateStake(const DeactivateRequest& req);
    void withdrawStake(const WithdrawRequest& req);

  signals:
    void actionUpdated(const StakingActionUpdate& update);
    void refreshRequested(const StakingRefreshRequest& request);

  private:
    bool validateDependencies(StakingAction action);
    void emitActionFailed(StakingAction action, const QString& code, const QString& message = {});

    void requestLatestBlockhash(const std::function<void(const QString&)>& onReady,
                                StakingAction action);
    void signAndSend(const QByteArray& message,
                     const std::function<QByteArray(const QByteArray&)>& buildSignedTx,
                     StakingAction action, const QString& signFailedCode,
                     const QString& signFailedMessage,
                     const std::function<void(const QString&)>& onSubmitted);

    SolanaApi* m_solanaApi = nullptr;
    Signer* m_signer = nullptr;
};

#endif // STAKINGHANDLER_H
