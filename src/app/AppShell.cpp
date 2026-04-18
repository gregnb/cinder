#include "CinderWalletApp.h"
#include "StyleLoader.h"
#include "Theme.h"
#include "app/AppSession.h"
#include "app/HardwareWalletCoordinator.h"
#include "crypto/HardwareWalletPlugin.h"
#include "crypto/Signer.h"
#include "crypto/SignerManager.h"
#include "crypto/WalletCrypto.h"
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
#include <QDialog>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QtConcurrent/QtConcurrent>
#include <sodium.h>

#ifdef Q_OS_MAC
#include "util/MacUtils.h"
#endif

static const int SIDEBAR_EXPANDED = 280;
static const int SIDEBAR_COLLAPSED = 80;

int CinderWalletApp::toIndex(MainPage page) { return static_cast<int>(page); }

int CinderWalletApp::toIndex(RootView view) { return static_cast<int>(view); }

void CinderWalletApp::buildUI() {
    setWindowTitle(tr("Cinder"));

    m_stylesheet = StyleLoader::loadTheme();
    setStyleSheet(m_stylesheet);

    m_rootStack = new QStackedWidget(this);

    QWidget* normalView = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(normalView);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_sidebar = new QWidget();
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(SIDEBAR_EXPANDED);
    m_sidebar->installEventFilter(this);
    createSidebar(m_sidebar);

    ContentBorderWidget* contentArea = new ContentBorderWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_pages = new QStackedWidget();
    m_sendReceivePage = new SendReceivePage();
    m_sendReceivePage->setSolanaApi(m_solanaApi);
    auto* addressBookPage = new AddressBookPage();

    m_assetsPage = new AssetsPage();
    m_dashboardPage = new DashboardPage();
    m_dashboardPage->setSignerManager(m_signerManager);
    m_activityPage = new ActivityPage();
    m_walletsPage = new WalletsPage();
    m_pages->addWidget(m_dashboardPage);
    m_pages->addWidget(m_walletsPage);
    m_pages->addWidget(m_activityPage);
    m_pages->addWidget(m_assetsPage);
    m_pages->addWidget(m_sendReceivePage);
    m_pages->addWidget(addressBookPage);
    m_txLookupPage = new TxLookupPage(m_solanaApi, m_idlRegistry);
    m_pages->addWidget(m_txLookupPage);
    m_activityPage->setAvatarCache(m_avatarCache);
    m_sendReceivePage->setAvatarCache(m_avatarCache);
    m_assetsPage->setAvatarCache(m_avatarCache);
    m_txLookupPage->setAvatarCache(m_avatarCache);
    m_dashboardPage->setAvatarCache(m_avatarCache);
    m_stakingPage = new StakingPage();
    m_stakingPage->setSolanaApi(m_solanaApi);
    m_stakingPage->setAvatarCache(m_avatarCache);
    connect(m_stakingPage, &StakingPage::stakingSummaryChanged, m_dashboardPage,
            &DashboardPage::updateStakingSummary);
    m_pages->addWidget(m_stakingPage);
    m_swapPage = new SwapPage();
    m_swapPage->setSolanaApi(m_solanaApi);
    m_pages->addWidget(m_swapPage);
    m_terminalPage = new TerminalPage(m_solanaApi, m_idlRegistry, m_networkStats, m_priceService);
    m_pages->addWidget(m_terminalPage);
    m_agentsPage = new AgentsPage();
    m_agentsPage->setSolanaApi(m_solanaApi);
    connect(m_agentsPage, &AgentsPage::notificationAdded, this,
            &CinderWalletApp::refreshNotificationBadge);
    connect(m_agentsPage, &AgentsPage::contactsChanged, addressBookPage,
            &AddressBookPage::refreshList);
    connect(m_agentsPage, &AgentsPage::contactsChanged, m_activityPage,
            &ActivityPage::refreshKeepingFilters);
    connect(m_agentsPage, &AgentsPage::balancesChanged, this, [this]() {
        QString addr = m_syncService->walletAddress();
        if (!addr.isEmpty()) {
            m_dashboardPage->refresh(addr);
            m_assetsPage->refresh(addr);
            m_sendReceivePage->refreshBalances();
            m_swapPage->refreshBalances();
        }
    });
    connect(m_agentsPage, &AgentsPage::stakeChanged, m_stakingPage, &StakingPage::refresh);
    m_pages->addWidget(m_agentsPage);
    m_settingsPage = new SettingsPage(m_solanaApi);
    m_pages->addWidget(m_settingsPage);

    connect(addressBookPage, &AddressBookPage::contactsChanged, m_activityPage,
            &ActivityPage::refreshKeepingFilters);
    connect(addressBookPage, &AddressBookPage::sendToAddress, this, [this](const QString& address) {
        m_sendReceivePage->openWithRecipient(address);
        navigateToPage(MainPage::SendReceive);
    });
    connect(m_assetsPage, &AssetsPage::sendAsset, this, [this](const QString& mint) {
        m_sendReceivePage->setExternalEntry(true);
        m_sendReceivePage->openWithMint(mint);
        navigateToPage(MainPage::SendReceive);
    });
    connect(m_sendReceivePage, &SendReceivePage::backRequested, this, &CinderWalletApp::goBack);
    connect(m_sendReceivePage, &SendReceivePage::transactionSent, this, [this](const QString&) {
        m_sendReceivePage->refreshBalances();
        QString addr = m_syncService->walletAddress();
        if (!addr.isEmpty()) {
            m_dashboardPage->refresh(addr);
            m_assetsPage->refresh(addr);
        }
    });
    connect(m_dashboardPage, &DashboardPage::transactionClicked, this,
            [this](const QString& signature) {
                m_txLookupPage->openWithSignature(signature);
                navigateToPage(MainPage::TxLookup);
            });
    connect(m_dashboardPage, &DashboardPage::stakeClicked, this,
            [this](const QString& stakeAddress) {
                m_stakingPage->openStakeDetail(stakeAddress);
                navigateToPage(MainPage::Staking);
            });
    connect(m_activityPage, &ActivityPage::transactionClicked, this,
            [this](const QString& signature) {
                m_txLookupPage->openWithSignature(signature);
                navigateToPage(MainPage::TxLookup);
            });
    connect(m_stakingPage, &StakingPage::transactionClicked, this,
            [this](const QString& signature) {
                m_txLookupPage->openWithSignature(signature);
                navigateToPage(MainPage::TxLookup);
            });
    connect(m_txLookupPage, &TxLookupPage::backRequested, this, &CinderWalletApp::goBack);
    connect(m_walletsPage, &WalletsPage::walletSwitched, this, &CinderWalletApp::switchToWallet);
    connect(m_walletsPage, &WalletsPage::addWalletRequested, this, &CinderWalletApp::importWallet);
    connect(m_walletsPage, &WalletsPage::addAccountRequested, this, &CinderWalletApp::addAccount);
    connect(m_walletsPage, &WalletsPage::walletRemoved, this, [this](const QString&) {
        auto wallets = WalletDb::getAllRecords();
        m_accountSelector->setAccounts(wallets);
        if (!wallets.isEmpty()) {
            switchToWallet(wallets.first().address);
        }
        m_walletsPage->refresh();
    });
    connect(m_walletsPage, &WalletsPage::walletRenamed, this,
            [this]() { m_accountSelector->setAccounts(WalletDb::getAllRecords()); });
    connect(m_walletsPage, &WalletsPage::avatarChanged, this,
            [this]() { m_accountSelector->setAccounts(WalletDb::getAllRecords()); });

    connect(m_syncService, &SyncService::balanceSynced, this, [this](const QString& address) {
        m_sendReceivePage->refreshBalances();
        m_dashboardPage->refresh(address);
        m_assetsPage->refresh(address);
    });
    connect(m_syncService, &SyncService::portfolioSynced, this, [this](const QString& address) {
        m_dashboardPage->refresh(address);
        m_assetsPage->refresh(address);
    });
    connect(m_syncService, &SyncService::transactionsSynced, this, [this](const QString& address) {
        m_dashboardPage->refresh(address);
        int newCount = TransactionDb::countTransactions(address);
        if (newCount != m_activityPage->totalRows()) {
            m_activityPage->refreshKeepingFilters();
        }
    });
    connect(m_syncService, &SyncService::syncError, this,
            [](const QString& method, const QString& error) {
                qWarning() << "[Sync] RPC error —" << method << ":" << error;
            });
    connect(m_syncService, &SyncService::balanceSynced, this,
            [this](const QString&) { m_tokenMetadata->start(); });
    connect(m_backfillService, &BackfillService::started, this,
            [this]() { m_activityPage->setBackfillRunning(true); });
    connect(m_backfillService, &BackfillService::backfillComplete, this,
            [this]() { m_activityPage->setBackfillRunning(false); });
    connect(m_backfillService, &BackfillService::pageComplete, this, [this](int newInPage) {
        if (newInPage > 0 && !m_syncService->walletAddress().isEmpty()) {
            int newCount = TransactionDb::countTransactions(m_syncService->walletAddress());
            if (newCount != m_activityPage->totalRows()) {
                m_activityPage->refreshKeepingFilters();
            }
            m_tokenMetadata->start();
        }
    });
    connect(m_tokenMetadata, &TokenMetadataService::metadataResolved, this, [this](const QString&) {
        if (!m_syncService->walletAddress().isEmpty()) {
            m_activityPage->refreshKeepingFilters();
        }
    });
    connect(m_networkStats, &NetworkStatsService::statsUpdated, this,
            [this]() { m_dashboardPage->updateNetworkStats(m_networkStats->stats()); });
#ifdef Q_OS_MAC
    connect(m_networkStats, &NetworkStatsService::connectionStatusChanged, this,
            [](bool connected) {
                updateConnectionStatus(connected, connected ? QObject::tr("CONNECTED")
                                                            : QObject::tr("NOT CONNECTED"));
            });
#endif

    connect(m_syncService, &SyncService::newActivityDetected, this,
            &CinderWalletApp::onNewActivity);

    connect(m_settingsPage, &SettingsPage::languageChanged, this, [this](const QString& code) {
        loadTranslator(code);
        QTimer::singleShot(0, this, [this]() { rebuildUI(); });
    });

    connect(m_settingsPage, &SettingsPage::biometricToggled, this, [this](bool enabled) {
        if (!enabled) {
            return;
        }

        QString address = m_syncService->walletAddress();
        if (address.isEmpty()) {
            m_settingsPage->setBiometricChecked(false);
            return;
        }

        auto* dialog = new QDialog(this);
        dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        dialog->setAttribute(Qt::WA_TranslucentBackground);
        dialog->setFixedSize(420, 240);

        auto* outer = new QVBoxLayout(dialog);
        outer->setContentsMargins(0, 0, 0, 0);

        auto* card = new QFrame(dialog);
        card->setObjectName("biometricDialogCard");
        outer->addWidget(card);

        auto* dlgLayout = new QVBoxLayout(card);
        dlgLayout->setContentsMargins(32, 28, 32, 28);
        dlgLayout->setSpacing(16);

        auto* dlgTitle = new QLabel(tr("Enter your wallet password to enable Touch ID"));
        dlgTitle->setWordWrap(true);
        dlgTitle->setProperty("uiClass", "appModalTitle");
        dlgLayout->addWidget(dlgTitle);

        auto* pwInput = new QLineEdit();
        pwInput->setObjectName("biometricPasswordInput");
        pwInput->setEchoMode(QLineEdit::Password);
        pwInput->setPlaceholderText(tr("Wallet password..."));
        dlgLayout->addWidget(pwInput);

        auto* dlgError = new QLabel();
        dlgError->setProperty("uiClass", "appModalError");
        dlgError->setVisible(false);
        dlgLayout->addWidget(dlgError);

        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();

        auto* cancelBtn = new QPushButton(tr("Cancel"));
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setProperty("uiClass", "appModalSecondaryBtn");
        btnRow->addWidget(cancelBtn);

        auto* confirmBtn = new QPushButton(tr("Enable"));
        confirmBtn->setCursor(Qt::PointingHandCursor);
        confirmBtn->setStyleSheet(Theme::primaryBtnStyle);
        btnRow->addWidget(confirmBtn);

        dlgLayout->addLayout(btnRow);

        connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
        connect(confirmBtn, &QPushButton::clicked, dialog,
                [this, pwInput, dlgError, confirmBtn, address, dialog]() {
                    QString password = pwInput->text();
                    if (password.isEmpty()) {
                        dlgError->setText(tr("Please enter your password."));
                        dlgError->setVisible(true);
                        return;
                    }

                    confirmBtn->setEnabled(false);
                    confirmBtn->setText(tr("Verifying..."));

                    auto wallet = WalletDb::getByAddressRecord(address);
                    if (!wallet) {
                        dlgError->setText(tr("Wallet not found."));
                        dlgError->setVisible(true);
                        confirmBtn->setEnabled(true);
                        confirmBtn->setText(tr("Enable"));
                        return;
                    }
                    WalletCrypto::EncryptedBlob blob;
                    blob.salt = wallet->salt;
                    blob.nonce = wallet->nonce;
                    blob.ciphertext = wallet->ciphertext;

                    auto* watcher = new QFutureWatcher<bool>(dialog);
                    connect(watcher, &QFutureWatcher<bool>::finished, dialog,
                            [watcher, dialog, dlgError, confirmBtn, address, password]() mutable {
                                bool decrypted = watcher->result();
                                watcher->deleteLater();
                                if (!decrypted) {
                                    dlgError->setText(tr("Incorrect password."));
                                    dlgError->setVisible(true);
                                    confirmBtn->setEnabled(true);
                                    confirmBtn->setText(tr("Enable"));
                                    return;
                                }
#ifdef Q_OS_MAC
                                bool stored = storeBiometricPassword(address, password);
#else
                                bool stored = false;
#endif
                                sodium_memzero(
                                    password.data(),
                                    static_cast<size_t>(password.size() * sizeof(QChar)));
                                if (stored) {
                                    dialog->accept();
                                } else {
                                    dlgError->setText(tr("Failed to store password in Keychain."));
                                    dlgError->setVisible(true);
                                    confirmBtn->setEnabled(true);
                                    confirmBtn->setText(tr("Enable"));
                                }
                            });

                    watcher->setFuture(QtConcurrent::run([blob, password]() mutable -> bool {
                        QByteArray result = WalletCrypto::decrypt(blob, password);
                        sodium_memzero(password.data(),
                                       static_cast<size_t>(password.size() * sizeof(QChar)));
                        if (result.isEmpty()) {
                            return false;
                        }
                        sodium_memzero(result.data(), result.size());
                        return true;
                    }));
                });

        if (dialog->exec() == QDialog::Accepted) {
            WalletDb::setBiometricEnabled(address, true);
        } else {
            m_settingsPage->setBiometricChecked(false);
        }
        dialog->deleteLater();
    });

    auto* reconnectBanner = new QWidget();
    reconnectBanner->setObjectName("appReconnectBanner");
    reconnectBanner->setFixedHeight(40);
    auto* bannerLayout = new QHBoxLayout(reconnectBanner);
    bannerLayout->setContentsMargins(16, 0, 16, 0);
    auto* reconnectBannerText = new QLabel(tr("Connect your hardware wallet to sign transactions"));
    reconnectBannerText->setObjectName("appReconnectBannerText");
    bannerLayout->addWidget(reconnectBannerText, 1);
    reconnectBanner->hide();
    contentLayout->addWidget(reconnectBanner);
    if (m_hardwareWalletCoordinator) {
        m_hardwareWalletCoordinator->setReconnectBanner(reconnectBanner, reconnectBannerText);
    }

    contentLayout->addWidget(m_pages);
    mainLayout->addWidget(m_sidebar);
    mainLayout->addWidget(contentArea, 1);

    m_notificationFlyout = new NotificationFlyout(normalView);
    m_notificationFlyout->hide();
    connect(m_notificationFlyout, &NotificationFlyout::allMarkedRead, this,
            &CinderWalletApp::refreshNotificationBadge);
    connect(m_notificationFlyout, &NotificationFlyout::badgeChanged, this,
            &CinderWalletApp::refreshNotificationBadge);

    m_setupPage = new SetupPage();
    connect(m_setupPage, &SetupPage::backRequested, this, &CinderWalletApp::showNormalView);
    connect(m_setupPage, &SetupPage::walletCreated, this, [this](const UnlockResult& result) {
        handleUnlockResult(result);
        showNormalView();
        switchToPage(MainPage::Dashboard);
    });

    if (m_appSession) {
        m_appSession->attachUi(m_sendReceivePage, m_terminalPage, m_stakingPage, m_swapPage,
                               m_txLookupPage, m_agentsPage, m_settingsPage, m_walletsPage,
                               m_activityPage, m_dashboardPage, m_assetsPage, m_accountSelector);
    }

    for (auto* plugin : m_signerManager->plugins()) {
        const HardwarePluginId pluginId = parseHardwarePluginId(plugin->pluginId());
        if (pluginId == HardwarePluginId::Ledger) {
            connect(plugin, &HardwareWalletPlugin::devicesChanged, this, [this, plugin]() {
                m_setupPage->setAvailableLedgerDevices(plugin->connectedDevices());
            });
            connect(m_setupPage, &SetupPage::ledgerPageEntered, this, [this, plugin]() {
                m_setupPage->setAvailableLedgerDevices(plugin->connectedDevices());
            });
            connect(m_setupPage, &SetupPage::ledgerConnectionRequested, this,
                    [this, plugin](const QString& deviceId, const QString& derivPath) {
                        m_hardwareWalletCoordinator->requestSetupConnection(
                            plugin, deviceId, derivPath,
                            tr("Failed to connect. Make sure the Solana app is open on your "
                               "Ledger."),
                            [this](Signer* signer) {
                                m_setupPage->onLedgerAddressReceived(signer->address(),
                                                                     signer->publicKey());
                            },
                            [this](const QString& error) {
                                m_setupPage->onLedgerConnectionFailed(error);
                            });
                    });
        } else if (pluginId == HardwarePluginId::Trezor) {
            connect(plugin, &HardwareWalletPlugin::devicesChanged, this, [this, plugin]() {
                m_setupPage->setAvailableTrezorDevices(plugin->connectedDevices());
            });
            connect(m_setupPage, &SetupPage::trezorPageEntered, this, [this, plugin]() {
                m_setupPage->setAvailableTrezorDevices(plugin->connectedDevices());
            });
            connect(m_setupPage, &SetupPage::trezorConnectionRequested, this,
                    [this, plugin](const QString& deviceId, const QString& derivPath) {
                        m_hardwareWalletCoordinator->requestSetupConnection(
                            plugin, deviceId, derivPath,
                            tr("Failed to connect. Make sure your Trezor is unlocked."),
                            [this](Signer* signer) {
                                m_setupPage->onTrezorAddressReceived(signer->address(),
                                                                     signer->publicKey());
                            },
                            [this](const QString& error) {
                                m_setupPage->onTrezorConnectionFailed(error);
                            });
                    });
        }
    }

    connect(m_signerManager, &SignerManager::signerDisconnected, this, [this]() {
        if (m_signerManager->isHardwareWallet()) {
            qDebug() << "Hardware wallet disconnected — signer cleared";
        }
    });

    m_lockScreen = new LockScreen();
    connect(m_lockScreen, &LockScreen::unlocked, this, [this](const UnlockResult& result) {
        QString lastActive = QSettings().value("lastActiveAddress").toString();

        handleUnlockResult(result);
        showNormalView();

        if (!lastActive.isEmpty() && lastActive != result.address) {
            switchToWallet(lastActive);
        } else {
            m_dashboardPage->refresh(result.address);
            m_activityPage->refresh(result.address);
            m_assetsPage->refresh(result.address);
            m_sendReceivePage->refreshBalances();
        }
    });

    m_rootStack->addWidget(normalView);
    m_rootStack->addWidget(m_setupPage);
    m_rootStack->addWidget(m_lockScreen);

    setCentralWidget(m_rootStack);
    switchToPage(MainPage::Dashboard);
}

void CinderWalletApp::rebuildUI() {
    MainPage savedPage = m_currentPage;
    bool savedCollapsed = m_sidebarCollapsed;

    m_syncService->disconnect(this);
    m_backfillService->disconnect(this);
    m_networkStats->disconnect(this);
    m_tokenMetadata->disconnect(this);

    m_navButtons.clear();
    m_navTextLabels.clear();
    m_navTexts.clear();
    m_extraButtons.clear();
    m_extraTextLabels.clear();
    m_navHistory.clear();
    m_currentPage = MainPage::Dashboard;
    m_sidebarCollapsed = false;

    delete takeCentralWidget();
    m_rootStack = nullptr;
    m_pages = nullptr;
    m_sidebar = nullptr;
    m_sidebarInner = nullptr;
    m_logo = nullptr;
    m_dashboardPage = nullptr;
    m_activityPage = nullptr;
    m_assetsPage = nullptr;
    m_sendReceivePage = nullptr;
    m_stakingPage = nullptr;
    m_swapPage = nullptr;
    m_txLookupPage = nullptr;
    m_terminalPage = nullptr;
    m_settingsPage = nullptr;
    m_walletsPage = nullptr;
    m_setupPage = nullptr;
    m_lockScreen = nullptr;
    m_notificationFlyout = nullptr;
    m_accountSelector = nullptr;

    buildUI();

    if (toIndex(savedPage) > 0 && toIndex(savedPage) < m_navButtons.size()) {
        switchToPage(savedPage);
    }
    if (savedCollapsed) {
        toggleSidebar();
    }

    if (m_appSession) {
        m_appSession->rebindUiState();
    }

#ifdef Q_OS_MAC
    updateSidebarToggleTooltip(m_sidebarCollapsed ? tr("Expand") : tr("Collapse"));
    updateNotificationBellTooltip(tr("Notifications"));
#endif
}

void CinderWalletApp::loadTranslator(const QString& localeCode) {
    if (m_appTranslator) {
        qApp->removeTranslator(m_appTranslator);
        delete m_appTranslator;
        m_appTranslator = nullptr;
    }
    if (m_qtTranslator) {
        qApp->removeTranslator(m_qtTranslator);
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }

    if (localeCode == "en") {
        return;
    }

    QLocale locale(localeCode);

    m_appTranslator = new QTranslator(this);
    if (m_appTranslator->load(locale, "Cinder", "_", ":/i18n")) {
        qApp->installTranslator(m_appTranslator);
    } else {
        delete m_appTranslator;
        m_appTranslator = nullptr;
    }

    m_qtTranslator = new QTranslator(this);
    if (m_qtTranslator->load(locale, "qtbase", "_",
                             QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        qApp->installTranslator(m_qtTranslator);
    } else {
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }
}

void CinderWalletApp::createSidebar(QWidget* sidebar) {
    m_sidebarInner = new QWidget(sidebar);
    m_sidebarInner->setFixedWidth(SIDEBAR_EXPANDED);
    m_sidebarInner->move(0, 0);

    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_sidebarInner);
    sidebarLayout->setContentsMargins(18, 20, 18, 28);
    sidebarLayout->setSpacing(6);

    m_logo = new QLabel();
    m_logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QPixmap logoPixmap(":/images/cinder-flame.png");

    if (!logoPixmap.isNull()) {
        qreal dpr = devicePixelRatioF();
        QPixmap scaled = logoPixmap.scaled(static_cast<int>(40 * dpr), static_cast<int>(40 * dpr),
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        m_logo->setPixmap(scaled);
        m_logo->setFixedSize(40, 40);
        m_logo->setObjectName("sidebarLogo");
    } else {
        m_logo->setText("◢◣◥");
        m_logo->setObjectName("logoFallback");
    }

    m_logo->setStyleSheet("background: transparent; border: none;");
    QHBoxLayout* logoRow = new QHBoxLayout();
    logoRow->setContentsMargins(2, 0, 0, 0);
    logoRow->setSpacing(0);
    logoRow->addWidget(m_logo);
    logoRow->addStretch();
    sidebarLayout->addLayout(logoRow);
    sidebarLayout->addSpacing(16);

    m_accountSelector = new AccountSelector();
    connect(m_accountSelector, &AccountSelector::accountSwitched, this,
            &CinderWalletApp::switchToWallet);
    connect(m_accountSelector, &AccountSelector::addAccountRequested, this,
            &CinderWalletApp::addAccount);
    connect(m_accountSelector, &AccountSelector::addWalletRequested, this,
            &CinderWalletApp::importWallet);
    sidebarLayout->addWidget(m_accountSelector);
    sidebarLayout->addSpacing(16);

    sidebarLayout->addWidget(createNavButton(tr("Dashboard"), "dashboard"));
    sidebarLayout->addWidget(createNavButton(tr("Wallets"), "wallets"));
    sidebarLayout->addWidget(createNavButton(tr("Activity"), "activity"));
    sidebarLayout->addWidget(createNavButton(tr("Assets"), "coins"));
    sidebarLayout->addWidget(createNavButton(tr("Send/Receive"), "swap"));
    sidebarLayout->addWidget(createNavButton(tr("Address Book"), "addressbook"));
    sidebarLayout->addWidget(createNavButton(tr("TX Lookup"), "txlookup"));
    sidebarLayout->addWidget(createNavButton(tr("Staking"), "stake"));
    sidebarLayout->addWidget(createNavButton(tr("Swap"), "transactions"));
    sidebarLayout->addWidget(createNavButton(tr("Terminal"), "terminal"));
    sidebarLayout->addWidget(createNavButton(tr("Agents"), "agents"));

    sidebarLayout->addStretch();

    {
        QPushButton* lockBtn = new QPushButton();
        lockBtn->setFixedHeight(44);
        lockBtn->setCursor(Qt::PointingHandCursor);
        lockBtn->setObjectName("navButton");

        QHBoxLayout* lLay = new QHBoxLayout(lockBtn);
        lLay->setContentsMargins(6, 0, 6, 0);
        lLay->setSpacing(10);

        QLabel* lIcon = new QLabel();
        lIcon->setObjectName("navIcon");
        QPixmap lockPx(":/icons/lock.png");
        if (!lockPx.isNull()) {
            qreal dpr = devicePixelRatioF();
            int sz = 32;
            QPixmap sc = lockPx.scaled(static_cast<int>(sz * dpr), static_cast<int>(sz * dpr),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
            sc.setDevicePixelRatio(dpr);
            lIcon->setPixmap(sc);
        }
        lIcon->setFixedSize(32, 32);
        lLay->addWidget(lIcon, 0, Qt::AlignVCenter);

        QLabel* lText = new QLabel(tr("Lock Wallet"));
        lText->setObjectName("navText");
        lLay->addWidget(lText, 1, Qt::AlignVCenter);

        connect(lockBtn, &QPushButton::clicked, this, [this]() {
            if (m_appSession) {
                m_appSession->clearSession();
            }

            m_lockScreen->refreshBiometricState();
            showRootView(RootView::LockScreen);
#ifdef Q_OS_MAC
            setToolbarItemsVisible(false);
#endif
        });
        m_extraButtons.append(lockBtn);
        m_extraTextLabels.append(lText);
        sidebarLayout->addWidget(lockBtn);
    }

    sidebarLayout->addWidget(createNavButton(tr("Settings"), "settings"));
}

QPushButton* CinderWalletApp::createNavButton(const QString& text, const QString& iconName) {
    QPushButton* btn = new QPushButton();
    btn->setFixedHeight(44);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setObjectName("navButton");

    QHBoxLayout* lay = new QHBoxLayout(btn);
    lay->setContentsMargins(6, 0, 6, 0);
    lay->setSpacing(10);

    QLabel* iconLabel = new QLabel();
    iconLabel->setObjectName("navIcon");
    QPixmap iconPixmap(QString(":/icons/%1.png").arg(iconName));
    if (!iconPixmap.isNull()) {
        qreal dpr = devicePixelRatioF();
        int sz = 32;
        QPixmap scaled = iconPixmap.scaled(static_cast<int>(sz * dpr), static_cast<int>(sz * dpr),
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        iconLabel->setPixmap(scaled);
    }
    iconLabel->setFixedSize(32, 32);
    lay->addWidget(iconLabel, 0, Qt::AlignVCenter);

    QLabel* textLabel = new QLabel(text);
    textLabel->setObjectName("navText");
    lay->addWidget(textLabel, 1, Qt::AlignVCenter);

    m_navTexts.append(text);
    m_navButtons.append(btn);
    m_navTextLabels.append(textLabel);
    MainPage pageIndex = static_cast<MainPage>(m_navButtons.size() - 1);

    connect(btn, &QPushButton::clicked, this, [this, pageIndex]() { switchToPage(pageIndex); });

    return btn;
}

void CinderWalletApp::setDropShadowsEnabled(bool enabled) {
    const auto children = findChildren<QWidget*>();
    for (QWidget* child : children) {
        auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(child->graphicsEffect());
        if (effect) {
            effect->setEnabled(enabled);
        }
    }
}

void CinderWalletApp::toggleNotificationFlyout() {
    if (m_notificationFlyout->isVisible()) {
        m_notificationFlyout->hideFlyout();
    } else {
        int flyoutWidth = m_notificationFlyout->width();
        int x = m_notificationFlyout->parentWidget()->width() - flyoutWidth - 16;
        int y = 8;
        m_notificationFlyout->move(x, y);
        m_notificationFlyout->showFlyout();
    }
}

void CinderWalletApp::toggleSidebar() {
    m_sidebarCollapsed = !m_sidebarCollapsed;

#ifdef Q_OS_MAC
    updateSidebarToggleTooltip(m_sidebarCollapsed ? tr("Expand") : tr("Collapse"));
#endif

    int targetWidth = m_sidebarCollapsed ? SIDEBAR_COLLAPSED : SIDEBAR_EXPANDED;

    if (!m_sidebarCollapsed) {
        for (int i = 0; i < m_navButtons.size(); ++i) {
            if (i < m_navTextLabels.size()) {
                m_navTextLabels[i]->show();
            }
            m_navButtons[i]->setFixedHeight(44);
            m_navButtons[i]->setMinimumWidth(0);
            m_navButtons[i]->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        for (int i = 0; i < m_extraButtons.size(); ++i) {
            m_extraTextLabels[i]->show();
            m_extraButtons[i]->setMinimumWidth(0);
            m_extraButtons[i]->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        if (m_accountSelector) {
            m_accountSelector->setCollapsed(false);
        }
        m_sidebarInner->layout()->invalidate();
        m_sidebarInner->layout()->activate();
        activatePage(m_currentPage);
    }

    setDropShadowsEnabled(false);

    QVariantAnimation* anim = new QVariantAnimation(this);
    anim->setDuration(130);
    anim->setStartValue(m_sidebar->width());
    anim->setEndValue(targetWidth);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) { m_sidebar->setFixedWidth(value.toInt()); });

    connect(anim, &QVariantAnimation::finished, this, [this]() {
        if (m_sidebarCollapsed) {
            for (int i = 0; i < m_navButtons.size(); ++i) {
                if (i < m_navTextLabels.size()) {
                    m_navTextLabels[i]->hide();
                }
                m_navButtons[i]->setFixedWidth(44);
            }
            for (int i = 0; i < m_extraButtons.size(); ++i) {
                m_extraTextLabels[i]->hide();
                m_extraButtons[i]->setFixedWidth(44);
            }
            if (m_accountSelector) {
                m_accountSelector->setCollapsed(true);
            }
            activatePage(m_currentPage);
        }
        setDropShadowsEnabled(true);
    });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

bool CinderWalletApp::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_sidebar && event->type() == QEvent::Resize && m_sidebarInner) {
        m_sidebarInner->setFixedHeight(m_sidebar->height());
    }
    return QMainWindow::eventFilter(obj, event);
}

void CinderWalletApp::switchToPage(MainPage page) {
    m_navHistory.clear();
    activatePage(page);
}

void CinderWalletApp::navigateToPage(MainPage page) {
    m_navHistory.append(m_currentPage);
    activatePage(page);
}

void CinderWalletApp::goBack() {
    if (m_navHistory.isEmpty()) {
        activatePage(MainPage::Dashboard);
        return;
    }
    MainPage previousPage = m_navHistory.takeLast();
    activatePage(previousPage);
}

void CinderWalletApp::activatePage(MainPage page) {
    const int index = toIndex(page);
    if (index < 0 || index >= m_navButtons.size()) {
        return;
    }
    const int previousIndex = toIndex(m_currentPage);

    if (previousIndex >= 0 && previousIndex < m_navButtons.size()) {
        QPushButton* prev = m_navButtons[previousIndex];
        prev->setObjectName("navButton");
        prev->setStyleSheet("");
        prev->setGraphicsEffect(nullptr);
        prev->style()->unpolish(prev);
        prev->style()->polish(prev);
        for (auto* child : prev->findChildren<QWidget*>()) {
            child->style()->unpolish(child);
            child->style()->polish(child);
        }
    }

    QPushButton* active = m_navButtons[index];
    active->setObjectName(m_sidebarCollapsed ? "navButtonActiveCompact" : "navButtonActive");
    active->setStyleSheet("");
    active->style()->unpolish(active);
    active->style()->polish(active);
    for (auto* child : active->findChildren<QWidget*>()) {
        child->style()->unpolish(child);
        child->style()->polish(child);
    }

    QGraphicsDropShadowEffect* glow = new QGraphicsDropShadowEffect();
    glow->setBlurRadius(m_sidebarCollapsed ? 20 : 45);
    glow->setColor(Theme::glowColor);
    glow->setOffset(0, 0);
    active->setGraphicsEffect(glow);

    m_currentPage = page;
    m_pages->setCurrentIndex(index);
}

void CinderWalletApp::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (m_needsInitialCenter) {
        m_needsInitialCenter = false;
        if (auto* screen = QGuiApplication::primaryScreen()) {
            QRect avail = screen->availableGeometry();

            int frameW = frameGeometry().width();
            int frameH = frameGeometry().height();

            if (frameH > avail.height()) {
                int newH = avail.height() - 40;
                resize(width(), newH);
                frameH = newH + (frameGeometry().height() - height());
            }

            int x = avail.x() + (avail.width() - frameW) / 2;
            int y = avail.y() + static_cast<int>((avail.height() - frameH) * 0.40);
            move(x, y);
        }
    }
}

void CinderWalletApp::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
#ifdef Q_OS_MAC
        if (windowHandle()) {
            setupTransparentTitleBar(windowHandle());
        }
#endif
        setStyleSheet("");
        setStyleSheet(m_stylesheet);
    }
}

void CinderWalletApp::showRootView(RootView view) { m_rootStack->setCurrentIndex(toIndex(view)); }

void CinderWalletApp::showSetupPage() { showRootView(RootView::WalletOnboarding); }

void CinderWalletApp::showNormalView() {
    showRootView(RootView::Normal);
#ifdef Q_OS_MAC
    setToolbarItemsVisible(true);
#endif
}
