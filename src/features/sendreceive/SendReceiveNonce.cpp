#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "widgets/AddressLink.h"
#include "widgets/StyledCheckbox.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
    constexpr int kPageMarginHorizontalPx = 40;
    constexpr int kPageMarginTopPx = 20;
    constexpr int kPageMarginBottomPx = 30;
    constexpr int kPageSpacingPx = 16;
    constexpr int kSmallSpacingPx = 8;
    constexpr int kTinySpacingPx = 4;
    constexpr int kActionButtonMinHeightPx = 48;
    constexpr double kLamportsPerSol = 1e9;
    constexpr double kNonceNetworkFeeSol = 0.000005;
    constexpr int kNonceDotsIntervalMs = 400;
    constexpr int kNonceDotsCycleSize = 3;
} // namespace

QWidget* SendReceivePage::buildNonceSetupPage() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("sendReceiveContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPageMarginHorizontalPx, kPageMarginTopPx, kPageMarginHorizontalPx,
                               kPageMarginBottomPx);
    layout->setSpacing(kPageSpacingPx);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        m_nonceCheckbox->setChecked(false);
        setCurrentPage(StackPage::SendForm);
    });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Create Nonce Account"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Description
    QLabel* desc = new QLabel(tr("A durable nonce account lets you sign transactions that "
                                 "never expire. The nonce replaces the recent blockhash, "
                                 "so your transaction can be submitted at any time."));
    desc->setObjectName("srSubtleDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(kSmallSpacingPx);

    // Authority (read-only)
    QLabel* authLabel = new QLabel(tr("Authority (your wallet)"));
    authLabel->setObjectName("txFormLabel");
    layout->addWidget(authLabel);

    m_nonceAuthorityLabel = new QLabel();
    m_nonceAuthorityLabel->setObjectName("srNonceField");
    m_nonceAuthorityLabel->setProperty("uiClass", "srNonceAuthorityField");
    layout->addWidget(m_nonceAuthorityLabel);

    layout->addSpacing(kTinySpacingPx);

    // Cost display
    QLabel* costLabel = new QLabel(tr("Creation Cost"));
    costLabel->setObjectName("txFormLabel");
    layout->addWidget(costLabel);

    m_nonceCostLabel = new QLabel(tr("Fetching..."));
    m_nonceCostLabel->setObjectName("srNonceField");
    layout->addWidget(m_nonceCostLabel);

    // Network fee note
    QLabel* feeNote =
        new QLabel(tr("+ ~%1 SOL network fee").arg(QString::number(kNonceNetworkFeeSol, 'f', 6)));
    feeNote->setObjectName("srFeeNote12");
    layout->addWidget(feeNote);

    layout->addSpacing(kSmallSpacingPx);

    // Status label (for progress/error messages)
    m_nonceStatusLabel = new QLabel();
    m_nonceStatusLabel->setObjectName("srStatusNeutral13");
    initializeStatusLabel(m_nonceStatusLabel);
    m_nonceStatusLabel->setWordWrap(true);
    m_nonceStatusLabel->setVisible(false);
    layout->addWidget(m_nonceStatusLabel);

    // Create button (disabled until cost is fetched)
    m_createNonceBtn = new QPushButton(tr("Create Nonce Account"));
    m_createNonceBtn->setCursor(Qt::PointingHandCursor);
    m_createNonceBtn->setMinimumHeight(kActionButtonMinHeightPx);
    m_createNonceBtn->setEnabled(false);
    m_createNonceBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(m_createNonceBtn, &QPushButton::clicked, this, &SendReceivePage::createNonceAccount);
    layout->addWidget(m_createNonceBtn);

    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

void SendReceivePage::wireNonceRetryButton() {
    m_createNonceBtn->setEnabled(true);
    m_createNonceBtn->setText(tr("Retry"));
    m_createNonceBtn->setStyleSheet(Theme::primaryBtnStyle);
    m_createNonceBtn->disconnect();
    connect(m_createNonceBtn, &QPushButton::clicked, this, [this]() {
        m_createNonceBtn->disconnect();
        connect(m_createNonceBtn, &QPushButton::clicked, this,
                &SendReceivePage::createNonceAccount);
        pollNonceAccount(m_pendingNonceAddress);
    });
}

void SendReceivePage::restoreCreateNonceButton() {
    m_createNonceBtn->disconnect();
    connect(m_createNonceBtn, &QPushButton::clicked, this, &SendReceivePage::createNonceAccount);
    m_createNonceBtn->setEnabled(true);
    m_createNonceBtn->setText(tr("Create Nonce Account"));
    m_createNonceBtn->setStyleSheet(Theme::primaryBtnStyle);
}

void SendReceivePage::applyReadyNonceState(const QString& nonceAddress, const QString& nonceValue) {
    m_nonceEnabled = true;
    m_nonceAddress = nonceAddress;
    m_nonceValue = nonceValue;
    m_pendingNonceAddress.clear();

    m_nonceAddrLabel->setAddress(nonceAddress);
    m_nonceInfoRow->setVisible(true);
    m_nonceCheckbox->setChecked(true);

    restoreCreateNonceButton();
    setCurrentPage(StackPage::SendForm);
}

void SendReceivePage::onNonceCheckboxToggled(bool checked) {
    if (!checked) {
        m_nonceEnabled = false;
        m_nonceAddress.clear();
        m_nonceValue.clear();
        m_nonceInfoRow->setVisible(false);
        return;
    }

    SendReceiveHandler::ResolveNonceToggleCallbacks callbacks;
    callbacks.onStoredNonce = [this](const QString& address, const QString& nonceValue) {
        m_nonceEnabled = true;
        m_nonceAddress = address;
        m_nonceValue = nonceValue;
        m_nonceAddrLabel->setAddress(address);
        m_nonceInfoRow->setVisible(true);
    };
    callbacks.onStoredNonceUpdated = [this](const QString& nonceValue) {
        m_nonceValue = nonceValue;
    };
    callbacks.onPendingNonceRetry = [this]() {
        m_nonceAuthorityLabel->setText(m_walletAddress);
        m_nonceStatusLabel->setVisible(false);
        m_nonceCostLabel->setText(tr("Account already created on-chain."));
        wireNonceRetryButton();
        setCurrentPage(StackPage::NonceSetup);
    };
    callbacks.onCreateRequired = [this]() {
        m_nonceAuthorityLabel->setText(m_walletAddress);
        m_nonceStatusLabel->setVisible(false);
        m_nonceCostLabel->setText(tr("Fetching..."));
        m_createNonceBtn->setEnabled(false);
        m_createNonceBtn->setText(tr("Create Nonce Account"));
        m_createNonceBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
        setCurrentPage(StackPage::NonceSetup);
    };
    callbacks.onRentReady = [this](quint64 lamports) {
        m_nonceRentLamports = lamports;
        const double sol = lamports / kLamportsPerSol;
        m_nonceCostLabel->setText(tr("Account rent: %1 SOL (%2 lamports)")
                                      .arg(QString::number(sol, 'f', 6))
                                      .arg(lamports));
        m_createNonceBtn->setEnabled(true);
        m_createNonceBtn->setStyleSheet(Theme::primaryBtnStyle);
    };
    callbacks.onRentFailed = [this](const QString& errorText) {
        m_nonceCostLabel->setText(errorText);
    };
    m_handler->resolveNonceToggleEnabled(m_walletAddress, m_pendingNonceAddress, m_solanaApi, this,
                                         callbacks);
}

// ── Animated dots for nonce status label ────────────────────────

void SendReceivePage::startNonceDots(const QString& baseText) {
    m_nonceStatusBase = baseText;
    m_nonceDotCount = 0;

    if (!m_nonceDotsTimer) {
        m_nonceDotsTimer = new QTimer(this);
        m_nonceDotsTimer->setInterval(kNonceDotsIntervalMs);
        connect(m_nonceDotsTimer, &QTimer::timeout, this, [this]() {
            m_nonceDotCount = (m_nonceDotCount % kNonceDotsCycleSize) + 1;
            QString dots = QString(".").repeated(m_nonceDotCount);
            m_nonceStatusLabel->setText(m_nonceStatusBase + dots);
        });
    }

    m_nonceStatusLabel->setText(baseText + ".");
    m_nonceStatusLabel->setVisible(true);
    m_nonceDotsTimer->start();
}

void SendReceivePage::stopNonceDots() {
    if (m_nonceDotsTimer) {
        m_nonceDotsTimer->stop();
    }
}

// ── Poll for nonce account data (used after creation + retry) ───

void SendReceivePage::pollNonceAccount(const QString& nonceAddress) {
    m_pendingNonceAddress = nonceAddress;
    m_createNonceBtn->setEnabled(false);
    m_createNonceBtn->setText(tr("Waiting for confirmation..."));
    m_createNonceBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);

    auto showRetry = [this]() {
        stopNonceDots();
        wireNonceRetryButton();
    };

    SendReceiveHandler::PollNonceCallbacks callbacks;
    callbacks.onAttempt = [this](int attempt, int maxAttempts) {
        startNonceDots(
            tr("Waiting for confirmation (attempt %1 of %2)").arg(attempt).arg(maxAttempts));
    };
    callbacks.onFailed = [this, showRetry](const QString& errorText) {
        stopNonceDots();
        m_nonceStatusLabel->setText(errorText);
        showRetry();
    };
    callbacks.onSuccess = [this](const QString& addr, const QString& nonceValue) {
        stopNonceDots();
        applyReadyNonceState(addr, nonceValue);
    };

    m_handler->pollNonceAccountFlow(nonceAddress, m_walletAddress, m_solanaApi, this, callbacks);
}

// ── Create nonce account ────────────────────────────────────────

void SendReceivePage::createNonceAccount() {
    m_createNonceBtn->setEnabled(false);
    m_createNonceBtn->setText(tr("Creating..."));
    m_createNonceBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);

    auto resetBtn = [this]() {
        stopNonceDots();
        restoreCreateNonceButton();
    };
    auto showError = [this, resetBtn](const QString& errorText) {
        stopNonceDots();
        m_nonceStatusLabel->setText(errorText);
        m_nonceStatusLabel->setVisible(true);
        resetBtn();
    };

    SendReceiveHandler::CreateAndPollNonceCallbacks callbacks;
    callbacks.onProgress = [this](const QString& text) { startNonceDots(text); };
    callbacks.onTransactionSent = [this](const QString& signature, const QString& nonceAddress) {
        m_pendingNonceAddress = nonceAddress;
        stopNonceDots();
        m_nonceStatusLabel->setText(tr("Transaction sent! TX: %1").arg(signature.left(20) + "..."));
        m_nonceStatusLabel->setVisible(true);
    };
    callbacks.onPollAttempt = [this](int attempt, int maxAttempts) {
        startNonceDots(
            tr("Waiting for confirmation (attempt %1 of %2)").arg(attempt).arg(maxAttempts));
    };
    callbacks.onFailed = [this, showError](const QString& errorText) {
        if (m_pendingNonceAddress.isEmpty()) {
            showError(errorText);
            return;
        }

        stopNonceDots();
        m_nonceStatusLabel->setText(errorText);
        m_nonceStatusLabel->setVisible(true);
        wireNonceRetryButton();
    };
    callbacks.onSuccess = [this](const QString& addr, const QString& nonceValue) {
        stopNonceDots();
        applyReadyNonceState(addr, nonceValue);
    };

    m_handler->executeCreateAndPollNonceFlow(m_walletAddress, m_nonceRentLamports, m_solanaApi,
                                             m_signer, this, callbacks);
}

// ── Execute Send ────────────────────────────────────────────────
