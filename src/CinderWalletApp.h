#ifndef CINDERWALLETAPP_H
#define CINDERWALLETAPP_H

#include "crypto/Keypair.h"
#include "crypto/UnlockResult.h"
#include <QEvent>

#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>
#include <QTranslator>
#include <QWindow>

class SetupPage;
class SendReceivePage;
class AssetsPage;
class ActivityPage;
class DashboardPage;
class StakingPage;
class SolanaApi;
class PriceService;
class PortfolioService;
class SyncService;
class BackfillService;
class TxLookupPage;
class TerminalPage;
class WalletsPage;
class AgentsPage;
class AvatarCache;
class IdlRegistry;
class LockScreen;
class NetworkStatsService;
class NotificationFlyout;
class SettingsPage;
class HardwareWalletPlugin;
class HardwareWalletCoordinator;
class AppSession;
class Signer;
class SignerManager;
class SoftwareSigner;
class SwapPage;
class TokenMetadataService;
class AccountSelector;

struct Activity;

class CinderWalletApp : public QMainWindow {
    Q_OBJECT

  public:
    explicit CinderWalletApp(QWidget* parent = nullptr);
    ~CinderWalletApp();

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    enum class MainPage {
        Dashboard = 0,
        Wallets = 1,
        Activity = 2,
        Assets = 3,
        SendReceive = 4,
        AddressBook = 5,
        TxLookup = 6,
        Staking = 7,
        Swap = 8,
        Terminal = 9,
        Agents = 10,
        Settings = 11
    };

    enum class RootView { Normal = 0, WalletOnboarding = 1, LockScreen = 2 };

    void initServices();
    void buildUI();
    void rebuildUI();
    void loadTranslator(const QString& localeCode);
    void createSidebar(QWidget* sidebar);
    QPushButton* createNavButton(const QString& text, const QString& iconName);
    void switchToPage(MainPage page);
    void navigateToPage(MainPage page);
    void goBack();
    void activatePage(MainPage page);
    void showRootView(RootView view);
    static int toIndex(MainPage page);
    static int toIndex(RootView view);
    void toggleSidebar();
    void toggleNotificationFlyout();
    void setDropShadowsEnabled(bool enabled);
    void showSetupPage();
    void showNormalView();
    void onNewActivity(const QString& signature, const Activity& activity);
    void refreshNotificationBadge();
    void handleUnlockResult(const UnlockResult& result);
    void switchToWallet(const QString& address);
    void addAccount(int parentWalletId);
    void importWallet();
    void onMacSleep();
    void onMacWake();

    QString m_stylesheet;
    QStackedWidget* m_rootStack = nullptr;
    QStackedWidget* m_pages = nullptr;
    SetupPage* m_setupPage = nullptr;
    SendReceivePage* m_sendReceivePage = nullptr;
    AssetsPage* m_assetsPage = nullptr;
    ActivityPage* m_activityPage = nullptr;
    DashboardPage* m_dashboardPage = nullptr;
    StakingPage* m_stakingPage = nullptr;
    TxLookupPage* m_txLookupPage = nullptr;
    TerminalPage* m_terminalPage = nullptr;
    WalletsPage* m_walletsPage = nullptr;
    AgentsPage* m_agentsPage = nullptr;
    LockScreen* m_lockScreen = nullptr;
    QList<QPushButton*> m_navButtons;
    QList<QLabel*> m_navTextLabels;
    QStringList m_navTexts;
    MainPage m_currentPage = MainPage::Dashboard;
    bool m_sidebarCollapsed = false;
    bool m_needsInitialCenter = false;
    QList<MainPage> m_navHistory;

    QWidget* m_sidebar = nullptr;
    QWidget* m_sidebarInner = nullptr;
    QLabel* m_logo = nullptr;

    // Extra sidebar buttons (Wallet, Lock) that need collapse handling
    QList<QPushButton*> m_extraButtons;
    QList<QLabel*> m_extraTextLabels;

    SignerManager* m_signerManager = nullptr;
    SolanaApi* m_solanaApi = nullptr;
    IdlRegistry* m_idlRegistry = nullptr;
    PriceService* m_priceService = nullptr;
    PortfolioService* m_portfolioService = nullptr;
    SyncService* m_syncService = nullptr;
    BackfillService* m_backfillService = nullptr;
    AvatarCache* m_avatarCache = nullptr;
    NetworkStatsService* m_networkStats = nullptr;
    TokenMetadataService* m_tokenMetadata = nullptr;
    NotificationFlyout* m_notificationFlyout = nullptr;
    SettingsPage* m_settingsPage = nullptr;
    SwapPage* m_swapPage = nullptr;
    HardwareWalletCoordinator* m_hardwareWalletCoordinator = nullptr;
    AppSession* m_appSession = nullptr;
    QWidget* m_contentArea = nullptr;
    AccountSelector* m_accountSelector = nullptr;
    QTranslator* m_appTranslator = nullptr;
    QTranslator* m_qtTranslator = nullptr;

  private slots:
    void onDashboardClicked();
    void onSendReceiveClicked();
    void onStakingClicked();
    void onSwapClicked();
    void onSettingsClicked();
};

#endif // CINDERWALLETAPP_H
