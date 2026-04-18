#include "app/AppSession.h"

#include "app/HardwareWalletCoordinator.h"
#include "crypto/HDDerivation.h"
#include "crypto/Signer.h"
#include "crypto/SignerManager.h"
#include "crypto/SoftwareSigner.h"
#include "crypto/WalletCrypto.h"
#include "db/WalletDb.h"
#include "features/activity/ActivityPage.h"
#include "features/agents/AgentsPage.h"
#include "features/assets/AssetsPage.h"
#include "features/dashboard/DashboardPage.h"
#include "features/sendreceive/SendReceivePage.h"
#include "features/settings/SettingsPage.h"
#include "features/staking/StakingPage.h"
#include "features/swap/SwapPage.h"
#include "features/terminal/TerminalPage.h"
#include "features/txlookup/TxLookupPage.h"
#include "features/wallets/WalletsPage.h"
#include "services/BackfillService.h"
#include "services/SyncService.h"
#include "widgets/AccountSelector.h"
#include <QDebug>
#include <QSettings>
#include <sodium.h>

AppSession::AppSession(SignerManager* signerManager, SyncService* syncService,
                       BackfillService* backfillService,
                       HardwareWalletCoordinator* hardwareWalletCoordinator, QObject* parent)
    : QObject(parent), m_signerManager(signerManager), m_syncService(syncService),
      m_backfillService(backfillService), m_hardwareWalletCoordinator(hardwareWalletCoordinator) {}

void AppSession::attachUi(SendReceivePage* sendReceivePage, TerminalPage* terminalPage,
                          StakingPage* stakingPage, SwapPage* swapPage, TxLookupPage* txLookupPage,
                          AgentsPage* agentsPage, SettingsPage* settingsPage,
                          WalletsPage* walletsPage, ActivityPage* activityPage,
                          DashboardPage* dashboardPage, AssetsPage* assetsPage,
                          AccountSelector* accountSelector) {
    m_sendReceivePage = sendReceivePage;
    m_terminalPage = terminalPage;
    m_stakingPage = stakingPage;
    m_swapPage = swapPage;
    m_txLookupPage = txLookupPage;
    m_agentsPage = agentsPage;
    m_settingsPage = settingsPage;
    m_walletsPage = walletsPage;
    m_activityPage = activityPage;
    m_dashboardPage = dashboardPage;
    m_assetsPage = assetsPage;
    m_accountSelector = accountSelector;

    rebindUiState();
}

void AppSession::handleUnlockResult(const UnlockResult& result) {
    if (result.isHardwareWallet()) {
        m_keypair = Keypair();

        if (Signer* pendingSigner =
                m_hardwareWalletCoordinator->takePendingSignerIfMatches(result.address)) {
            m_signerManager->setActiveSigner(pendingSigner);
            distributeSigner(pendingSigner);
        } else {
            m_hardwareWalletCoordinator->connectHardwareSigner(result.hardwarePlugin,
                                                               result.derivationPath);
        }
    } else {
        auto* signer = new SoftwareSigner(result.keypair, this);
        m_signerManager->setActiveSigner(signer);
        applySoftwareKeypair(result.keypair, signer);
    }

    if (!result.password.isEmpty()) {
        clearSession();
        m_sessionPassword = result.password;
        updateSignerFactory();
    }

    applyActiveAddress(result.address, true);
}

void AppSession::switchToWallet(const QString& address) {
    if (m_syncService && address == m_syncService->walletAddress()) {
        return;
    }

    auto wallet = WalletDb::getByAddressRecord(address);
    if (!wallet) {
        qWarning() << "switchToWallet: wallet not found for" << address;
        return;
    }

    m_hardwareWalletCoordinator->hideReconnectBanner();

    if (wallet->isHardwareWallet()) {
        m_keypair = Keypair();
        m_hardwareWalletCoordinator->connectHardwareSigner(wallet->hardwarePluginId(),
                                                           wallet->derivationPath);
    } else {
        if (m_sessionPassword.isEmpty()) {
            qWarning() << "switchToWallet: no session password cached";
            return;
        }

        WalletCrypto::EncryptedBlob blob;
        blob.salt = wallet->salt;
        blob.nonce = wallet->nonce;
        blob.ciphertext = wallet->ciphertext;

        QByteArray secretKey = WalletCrypto::decrypt(blob, m_sessionPassword);
        if (secretKey.isEmpty()) {
            qWarning() << "switchToWallet: decryption failed";
            return;
        }

        Keypair keypair = Keypair::fromSecretKey(secretKey);
        sodium_memzero(secretKey.data(), secretKey.size());

        auto* signer = new SoftwareSigner(keypair, this);
        m_signerManager->setActiveSigner(signer);
        applySoftwareKeypair(keypair, signer);
    }

    applyActiveAddress(address, false);
}

void AppSession::addAccount(int parentWalletId) {
    if (m_sessionPassword.isEmpty()) {
        qWarning() << "addAccount: no session password cached";
        return;
    }

    auto seedBlob = WalletDb::getSeedBlobRecord(parentWalletId);
    if (!seedBlob) {
        qWarning() << "addAccount: no stored seed for parent" << parentWalletId;
        return;
    }

    WalletCrypto::EncryptedBlob blob;
    blob.salt = seedBlob->salt;
    blob.nonce = seedBlob->nonce;
    blob.ciphertext = seedBlob->ciphertext;
    QByteArray seed = WalletCrypto::decrypt(blob, m_sessionPassword);

    if (seed.isEmpty()) {
        qWarning() << "addAccount: seed decryption failed";
        return;
    }

    int nextIndex = WalletDb::nextAccountIndex(parentWalletId);
    QString derivPath = QString("m/44'/501'/%1'/0'").arg(nextIndex);
    QByteArray edSeed = HDDerivation::derive(seed, derivPath);
    sodium_memzero(seed.data(), seed.size());

    if (edSeed.isEmpty()) {
        qWarning() << "addAccount: HD derivation failed";
        return;
    }

    Keypair keypair = Keypair::fromSeed(edSeed);
    sodium_memzero(edSeed.data(), edSeed.size());

    if (keypair.isNull()) {
        qWarning() << "addAccount: keypair creation failed";
        return;
    }

    QByteArray secretKey = keypair.secretKey();
    auto encrypted = WalletCrypto::encrypt(secretKey, m_sessionPassword);
    sodium_memzero(secretKey.data(), secretKey.size());

    QString label = QString("Wallet %1").arg(nextIndex + 1);
    bool ok = WalletDb::insertDerivedWallet(
        label, keypair.address(), toStorageString(WalletKeyType::Mnemonic), encrypted.salt,
        encrypted.nonce, encrypted.ciphertext, nextIndex, parentWalletId);
    if (!ok) {
        qWarning() << "addAccount: database insert failed";
        return;
    }

    if (m_accountSelector) {
        m_accountSelector->setAccounts(WalletDb::getAllRecords());
    }
    switchToWallet(keypair.address());
}

void AppSession::rebindUiState() {
    QString address = m_syncService ? m_syncService->walletAddress() : QString();
    if (!address.isEmpty()) {
        applyActiveAddress(address, true);
    }

    if (!m_keypair.address().isEmpty()) {
        if (m_sendReceivePage) {
            m_sendReceivePage->setKeypair(m_keypair);
        }
        if (m_terminalPage) {
            m_terminalPage->setKeypair(m_keypair);
        }
        if (m_stakingPage) {
            m_stakingPage->setKeypair(m_keypair);
        }
        if (m_swapPage) {
            m_swapPage->setKeypair(m_keypair);
        }
    }

    if (m_signerManager && m_signerManager->activeSigner()) {
        distributeSigner(m_signerManager->activeSigner());
    }

    updateSignerFactory();
}

void AppSession::clearSession() {
    if (!m_sessionPassword.isEmpty()) {
        sodium_memzero(m_sessionPassword.data(),
                       static_cast<size_t>(m_sessionPassword.size() * sizeof(QChar)));
        m_sessionPassword.clear();
    }
    m_keypair = Keypair();
    if (m_agentsPage) {
        m_agentsPage->setSignerFactory(nullptr);
    }
}

const QString& AppSession::sessionPassword() const { return m_sessionPassword; }

const Keypair& AppSession::currentKeypair() const { return m_keypair; }

void AppSession::distributeSigner(Signer* signer) {
    if (m_sendReceivePage) {
        m_sendReceivePage->setSigner(signer);
    }
    if (m_terminalPage) {
        m_terminalPage->setSigner(signer);
    }
    if (m_stakingPage) {
        m_stakingPage->setSigner(signer);
    }
    if (m_swapPage) {
        m_swapPage->setSigner(signer);
    }
    if (m_agentsPage) {
        m_agentsPage->setSigner(signer);
    }
}

void AppSession::applySoftwareKeypair(const Keypair& keypair, Signer* signer) {
    m_keypair = keypair;
    if (m_sendReceivePage) {
        m_sendReceivePage->setKeypair(keypair);
    }
    if (m_terminalPage) {
        m_terminalPage->setKeypair(keypair);
    }
    if (m_stakingPage) {
        m_stakingPage->setKeypair(keypair);
    }
    if (m_swapPage) {
        m_swapPage->setKeypair(keypair);
    }
    distributeSigner(signer);
}

void AppSession::applyActiveAddress(const QString& address, bool refreshAccountList) {
    if (m_syncService) {
        m_syncService->setWalletAddress(address);
    }
    if (m_backfillService) {
        m_backfillService->setWalletAddress(address);
    }
    if (m_sendReceivePage) {
        m_sendReceivePage->setWalletAddress(address);
        m_sendReceivePage->refreshBalances();
    }
    if (m_terminalPage) {
        m_terminalPage->setWalletAddress(address);
    }
    if (m_stakingPage) {
        m_stakingPage->setWalletAddress(address);
        m_stakingPage->prefetchValidators();
        m_stakingPage->refresh();
    }
    if (m_swapPage) {
        m_swapPage->setWalletAddress(address);
    }
    if (m_txLookupPage) {
        m_txLookupPage->setWalletAddress(address);
    }
    if (m_agentsPage) {
        m_agentsPage->setWalletAddress(address);
    }
    if (m_settingsPage) {
        m_settingsPage->setWalletAddress(address);
    }
    if (m_walletsPage) {
        m_walletsPage->setActiveAddress(address);
        m_walletsPage->refresh();
    }
    if (m_activityPage && m_backfillService) {
        m_activityPage->setBackfillRunning(m_backfillService->isRunning());
        m_activityPage->refresh(address);
    }
    if (m_dashboardPage) {
        m_dashboardPage->refresh(address);
    }
    if (m_assetsPage) {
        m_assetsPage->refresh(address);
    }

    QSettings().setValue("lastActiveAddress", address);

    if (m_accountSelector) {
        if (refreshAccountList) {
            m_accountSelector->setAccounts(WalletDb::getAllRecords());
        }
        m_accountSelector->setActiveAddress(address);
    }
}

void AppSession::updateSignerFactory() {
    if (!m_agentsPage) {
        return;
    }

    if (m_sessionPassword.isEmpty()) {
        m_agentsPage->setSignerFactory(nullptr);
        return;
    }

    m_agentsPage->setSignerFactory([this](const QString& address) -> Signer* {
        if (m_sessionPassword.isEmpty()) {
            return nullptr;
        }
        auto wallet = WalletDb::getByAddressRecord(address);
        if (!wallet || wallet->isHardwareWallet()) {
            return nullptr;
        }
        WalletCrypto::EncryptedBlob blob;
        blob.salt = wallet->salt;
        blob.nonce = wallet->nonce;
        blob.ciphertext = wallet->ciphertext;
        QByteArray secretKey = WalletCrypto::decrypt(blob, m_sessionPassword);
        if (secretKey.isEmpty()) {
            return nullptr;
        }
        Keypair keypair = Keypair::fromSecretKey(secretKey);
        sodium_memzero(secretKey.data(), secretKey.size());
        return new SoftwareSigner(keypair);
    });
}
