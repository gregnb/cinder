#include "CinderWalletApp.h"
#include "StyleLoader.h"
#include "Theme.h"
#include "app/AppSession.h"
#include "app/HardwareWalletCoordinator.h"
#include "crypto/Signer.h"
#include "crypto/SignerManager.h"
#include "crypto/plugin/lattice/LatticePlugin.h"
#include "crypto/plugin/ledger/LedgerPlugin.h"
#include "crypto/plugin/trezor/TrezorPlugin.h"
#include "db/ContactDb.h"
#include "db/NotificationDb.h"
#include "db/TokenAccountDb.h"
#include "db/TransactionDb.h"
#include "db/WalletDb.h"
#include "features/activity/ActivityPage.h"
#include "features/addressbook/AddressBookPage.h"
#include "features/agents/AgentsPage.h"
#include "features/assets/AssetsPage.h"
#include "features/dashboard/DashboardPage.h"
#include "features/lockscreen/LockScreen.h"
#include "features/sendreceive/SendReceivePage.h"
#include "features/settings/SettingsHandler.h"
#include "features/settings/SettingsPage.h"
#include "features/setup/SetupPage.h"
#include "features/staking/StakingPage.h"
#include "features/swap/SwapPage.h"
#include "features/terminal/TerminalPage.h"
#include "features/txlookup/TxLookupPage.h"
#include "features/wallets/WalletsPage.h"
#include "services/AvatarCache.h"
#include "services/BackfillService.h"
#include "services/IdlRegistry.h"
#include "services/NetworkStatsService.h"
#include "services/PortfolioService.h"
#include "services/PriceService.h"
#include "services/SolanaApi.h"
#include "services/SyncService.h"
#include "services/TokenMetadataService.h"
#include "tx/KnownTokens.h"
#include "util/FrameRecorder.h"
#include "widgets/AccountSelector.h"
#include "widgets/ContentBorderWidget.h"
#include "widgets/NotificationFlyout.h"
#include <QApplication>
#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLibraryInfo>
#include <QPixmap>
#include <QScreen>
#include <QStyle>
#include <QTimer>

#ifdef Q_OS_MAC
#include "util/MacUtils.h"
#endif

CinderWalletApp::CinderWalletApp(QWidget* parent) : QMainWindow(parent) {
    setMinimumSize(1100, 720);

    // Size dynamically based on available screen space
    if (auto* screen = QGuiApplication::primaryScreen()) {
        QRect avail = screen->availableGeometry();
        int w = qBound(1100, static_cast<int>(avail.width() * 0.90), 1400);
        int h = qBound(720, static_cast<int>(avail.height() * 0.85), avail.height() - 40);
        resize(w, h);
    } else {
        resize(1200, 750);
    }

    // Center on screen after first show (see showEvent override)
    m_needsInitialCenter = true;

    loadTranslator(SettingsHandler::savedLanguageCode());
    initServices();
    buildUI();

    // Route to the appropriate initial screen
    bool hasWallet = WalletDb::hasAnyWallet();
    int walletCount = WalletDb::countAll();
    qDebug() << "[CinderWalletApp] hasAnyWallet:" << hasWallet << "countAll:" << walletCount;
    if (hasWallet) {
        m_lockScreen->refreshBiometricState();
        showRootView(RootView::LockScreen);
#ifdef Q_OS_MAC
        setToolbarItemsVisible(false);
#endif
    } else {
        showRootView(RootView::WalletOnboarding);
#ifdef Q_OS_MAC
        setToolbarItemsVisible(false);
#endif
    }

#ifdef Q_OS_MAC
    setSidebarToggleCallback([this]() { toggleSidebar(); });
    setBellClickCallback([this]() { toggleNotificationFlyout(); });
    registerSleepWakeCallbacks([this]() { onMacSleep(); }, [this]() { onMacWake(); });
#endif

    // Defer badge refresh until after toolbar is created (setupTransparentTitleBar in main.cpp)
    QTimer::singleShot(0, this, [this]() { refreshNotificationBadge(); });
}

CinderWalletApp::~CinderWalletApp() {
#ifdef Q_OS_MAC
    unregisterSleepWakeCallbacks();
#endif
}

// ── Service initialization (called once) ─────────────────────

void CinderWalletApp::initServices() {
    m_signerManager = new SignerManager(this);

    auto* ledgerPlugin = new LedgerPlugin(this);
    m_signerManager->registerPlugin(ledgerPlugin);
    ledgerPlugin->startPolling(2500);

    auto* trezorPlugin = new TrezorPlugin(this);
    m_signerManager->registerPlugin(trezorPlugin);
    trezorPlugin->startPolling(2500);

    auto* latticePlugin = new LatticePlugin(this);
    m_signerManager->registerPlugin(latticePlugin);

    m_solanaApi = new SolanaApi(this);
    m_solanaApi->setRpcUrls(SettingsHandler::loadRpcEndpoints());
    m_idlRegistry = new IdlRegistry(m_solanaApi, this);
    m_avatarCache = new AvatarCache(this);
    m_priceService = new PriceService(this);
    m_portfolioService = new PortfolioService(m_priceService, this);
    m_syncService = new SyncService(m_solanaApi, m_portfolioService, this);
    m_backfillService = new BackfillService(m_solanaApi, this);
    m_tokenMetadata = new TokenMetadataService(m_solanaApi, m_avatarCache, this);
    m_networkStats = new NetworkStatsService(m_solanaApi, this);
    m_networkStats->start();
    m_hardwareWalletCoordinator = new HardwareWalletCoordinator(
        this, m_signerManager,
        [this](Signer* signer) {
            m_signerManager->setActiveSigner(signer);
            if (m_appSession) {
                m_appSession->rebindUiState();
            }
        },
        this);
    m_appSession = new AppSession(m_signerManager, m_syncService, m_backfillService,
                                  m_hardwareWalletCoordinator, this);
}

// ── Session handling ──────────────────────────────────────────

void CinderWalletApp::handleUnlockResult(const UnlockResult& result) {
    m_appSession->handleUnlockResult(result);
}

// ── Notification handling ─────────────────────────────────────

void CinderWalletApp::onNewActivity(const QString& signature, const Activity& activity) {
    if (NotificationDb::existsForSignature(signature)) {
        return;
    }

    // Resolve token symbol
    QString tokenSymbol = activity.token;
    if (tokenSymbol != "SOL") {
        KnownToken kt = resolveKnownToken(tokenSymbol);
        if (!kt.symbol.isEmpty()) {
            tokenSymbol = kt.symbol;
        } else {
            auto tok = TokenAccountDb::getTokenRecord(activity.token);
            if (tok.has_value() && !tok->symbol.isEmpty()) {
                tokenSymbol = tok->symbol;
            } else {
                tokenSymbol = activity.token.left(6) + "...";
            }
        }
    }

    // Format amount
    QString amountStr;
    if (activity.amount >= 1000.0) {
        amountStr = QLocale(QLocale::English).toString(activity.amount, 'f', 2);
    } else if (activity.amount >= 1.0) {
        amountStr = QString::number(activity.amount, 'f', 4);
    } else if (activity.amount > 0) {
        amountStr = QString::number(activity.amount, 'f', 6);
        while (amountStr.endsWith('0') && !amountStr.endsWith(".0")) {
            amountStr.chop(1);
        }
    } else {
        amountStr = "0";
    }

    QString title = tr("Received %1 %2").arg(amountStr, tokenSymbol);

    // Resolve sender display name — contact name or truncated address
    QString senderDisplay;
    QString contactName = ContactDb::getNameByAddress(activity.fromAddress);
    if (!contactName.isEmpty()) {
        senderDisplay = contactName;
    } else if (activity.fromAddress.length() > 20) {
        senderDisplay = activity.fromAddress.left(8) + "..." + activity.fromAddress.right(8);
    } else {
        senderDisplay = activity.fromAddress;
    }

    QString body = tr("From %1").arg(senderDisplay);

    NotificationDb::insertNotification("receive", title, body, signature, activity.token, amountStr,
                                       activity.fromAddress);

    refreshNotificationBadge();
}

void CinderWalletApp::refreshNotificationBadge() {
    int unread = NotificationDb::countUnread();
#ifdef Q_OS_MAC
    updateNotificationBadge(unread);
#endif
    Q_UNUSED(unread);
}

// ── Multi-wallet switching ───────────────────────────────────

void CinderWalletApp::switchToWallet(const QString& address) {
    m_appSession->switchToWallet(address);
}

void CinderWalletApp::addAccount(int parentWalletId) { m_appSession->addAccount(parentWalletId); }

void CinderWalletApp::importWallet() {
    if (m_appSession->sessionPassword().isEmpty()) {
        qWarning() << "importWallet: no session password cached";
        return;
    }
    m_setupPage->startImportFlow(m_appSession->sessionPassword());
    showRootView(RootView::WalletOnboarding);
}

void CinderWalletApp::onDashboardClicked() { switchToPage(MainPage::Dashboard); }
void CinderWalletApp::onSendReceiveClicked() { switchToPage(MainPage::SendReceive); }
void CinderWalletApp::onStakingClicked() { switchToPage(MainPage::Staking); }
void CinderWalletApp::onSwapClicked() { switchToPage(MainPage::Swap); }
void CinderWalletApp::onSettingsClicked() { switchToPage(MainPage::Settings); }

// ── Sleep/Wake suspension ────────────────────────────────────

void CinderWalletApp::onMacSleep() {
    qDebug() << "[CinderWalletApp] System sleep — pausing services";
    m_solanaApi->flushQueues();
    m_syncService->pause();
    m_backfillService->pause();
    m_networkStats->pause();
    m_tokenMetadata->pause();
    m_priceService->pause();
    m_priceService->flushQueue();
}

void CinderWalletApp::onMacWake() {
    qDebug() << "[CinderWalletApp] System wake — resuming services";
    m_syncService->resume();
    m_backfillService->resume();
    m_networkStats->resume();
    m_tokenMetadata->resume();
    m_priceService->resume();

    // Delayed catch-up: refresh network stats after connections re-establish
    QTimer::singleShot(2000, this, [this]() { m_networkStats->fetchNow(); });
}
