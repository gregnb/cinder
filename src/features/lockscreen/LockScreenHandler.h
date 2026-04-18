#ifndef LOCKSCREENHANDLER_H
#define LOCKSCREENHANDLER_H

#include "crypto/UnlockResult.h"
#include <QObject>
#include <QString>

class LockScreenHandler : public QObject {
    Q_OBJECT

  public:
    enum class UnlockError {
        EmptyPassword,
        NoWallet,
        WalletNotFound,
        IncorrectPassword,
    };

    explicit LockScreenHandler(QObject* parent = nullptr);

    void refreshBiometricState();
    void attemptUnlock(const QString& password);
    void attemptBiometricUnlock();

  signals:
    void biometricStateChanged(bool available);
    void unlockStarted();
    void unlockFailed(LockScreenHandler::UnlockError error);
    void unlockSucceeded(const UnlockResult& result);
    void unlockFlowFinished();

  private:
    QString m_walletAddress;
    bool m_biometricAvailable = false;
};

#endif // LOCKSCREENHANDLER_H
