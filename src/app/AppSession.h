#ifndef APPSESSION_H
#define APPSESSION_H

#include "crypto/Keypair.h"
#include "crypto/UnlockResult.h"
#include <QObject>

class Signer;
class SignerManager;
class HardwareWalletCoordinator;
class SyncService;
class BackfillService;
class SendReceivePage;
class TerminalPage;
class StakingPage;
class SwapPage;
class TxLookupPage;
class AgentsPage;
class SettingsPage;
class WalletsPage;
class ActivityPage;
class DashboardPage;
class AssetsPage;
class AccountSelector;

class AppSession : public QObject {
    Q_OBJECT

  public:
    AppSession(SignerManager* signerManager, SyncService* syncService,
               BackfillService* backfillService,
               HardwareWalletCoordinator* hardwareWalletCoordinator, QObject* parent = nullptr);

    void attachUi(SendReceivePage* sendReceivePage, TerminalPage* terminalPage,
                  StakingPage* stakingPage, SwapPage* swapPage, TxLookupPage* txLookupPage,
                  AgentsPage* agentsPage, SettingsPage* settingsPage, WalletsPage* walletsPage,
                  ActivityPage* activityPage, DashboardPage* dashboardPage, AssetsPage* assetsPage,
                  AccountSelector* accountSelector);

    void handleUnlockResult(const UnlockResult& result);
    void switchToWallet(const QString& address);
    void addAccount(int parentWalletId);
    void rebindUiState();
    void clearSession();

    const QString& sessionPassword() const;
    const Keypair& currentKeypair() const;

  private:
    void distributeSigner(Signer* signer);
    void applySoftwareKeypair(const Keypair& keypair, Signer* signer);
    void applyActiveAddress(const QString& address, bool refreshAccountList);
    void updateSignerFactory();

    SignerManager* m_signerManager = nullptr;
    SyncService* m_syncService = nullptr;
    BackfillService* m_backfillService = nullptr;
    HardwareWalletCoordinator* m_hardwareWalletCoordinator = nullptr;

    SendReceivePage* m_sendReceivePage = nullptr;
    TerminalPage* m_terminalPage = nullptr;
    StakingPage* m_stakingPage = nullptr;
    SwapPage* m_swapPage = nullptr;
    TxLookupPage* m_txLookupPage = nullptr;
    AgentsPage* m_agentsPage = nullptr;
    SettingsPage* m_settingsPage = nullptr;
    WalletsPage* m_walletsPage = nullptr;
    ActivityPage* m_activityPage = nullptr;
    DashboardPage* m_dashboardPage = nullptr;
    AssetsPage* m_assetsPage = nullptr;
    AccountSelector* m_accountSelector = nullptr;

    Keypair m_keypair;
    QString m_sessionPassword;
};

#endif // APPSESSION_H
