#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "db/TokenAccountDb.h"
#include "features/sendreceive/BurnTokenHandler.h"
#include "tx/KnownTokens.h"
#include "widgets/AmountInput.h"
#include "widgets/TokenDropdown.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
    constexpr int kPageMarginHorizontalPx = 40;
    constexpr int kPageMarginTopPx = 20;
    constexpr int kPageMarginBottomPx = 30;
    constexpr int kPageSpacingPx = 16;
    constexpr int kSmallSpacingPx = 8;
    constexpr int kTinySpacingPx = 4;
    constexpr int kSectionSpacingPx = 12;
    constexpr int kWarningMarginHorizontalPx = 16;
    constexpr int kWarningMarginVerticalPx = 14;
    constexpr int kWarningSpacingPx = 12;
    constexpr int kWarningIconWidthPx = 24;
    constexpr int kActionButtonMinHeightPx = 48;
} // namespace

QWidget* SendReceivePage::buildBurnTokensPage() {
    m_burnScroll = new QScrollArea();
    m_burnScroll->setWidgetResizable(true);
    m_burnScroll->setFrameShape(QFrame::NoFrame);
    m_burnScroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("sendReceiveContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPageMarginHorizontalPx, kPageMarginTopPx, kPageMarginHorizontalPx,
                               kPageMarginBottomPx);
    layout->setSpacing(kPageSpacingPx);

    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this,
            [this]() { setCurrentPage(StackPage::CardGrid); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    QLabel* title = new QLabel(tr("Burn Tokens"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    QLabel* desc = new QLabel(tr("Permanently destroy tokens by burning them. "
                                 "This action is irreversible and reduces the token supply."));
    desc->setObjectName("srSubtleDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(kSmallSpacingPx);

    QLabel* tokenLabel = new QLabel(tr("Token"));
    tokenLabel->setObjectName("txFormLabel");
    layout->addWidget(tokenLabel);

    m_burnTokenDropdown = new TokenDropdown();
    layout->addWidget(m_burnTokenDropdown);

    m_burnBalanceLabel = new QLabel(tr("Balance: 0"));
    m_burnBalanceLabel->setObjectName("srSubtleInfo13");
    layout->addWidget(m_burnBalanceLabel);

    connect(m_burnTokenDropdown, &TokenDropdown::tokenSelected, this,
            [this](const QString&, const QString&) {
                const QString bal = m_burnTokenDropdown->currentBalanceText();
                m_burnBalanceLabel->setText(tr("Balance: %1").arg(bal.isEmpty() ? "0" : bal));
                validateBurnForm();
            });

    layout->addSpacing(kTinySpacingPx);

    QLabel* amountLabel = new QLabel(tr("Amount to Burn"));
    amountLabel->setObjectName("txFormLabel");
    layout->addWidget(amountLabel);

    m_burnAmountInput = new AmountInput();
    connect(m_burnAmountInput, &AmountInput::maxClicked, this, [this]() {
        const QString bal = m_burnTokenDropdown->currentBalanceText();
        const QString numericBal = bal.split(' ', Qt::SkipEmptyParts).first();
        m_burnAmountInput->setText(numericBal);
    });
    layout->addWidget(m_burnAmountInput);

    connect(m_burnAmountInput, &AmountInput::textChanged, this, &SendReceivePage::validateBurnForm);

    layout->addSpacing(kSectionSpacingPx);

    QWidget* warningBox = new QWidget();
    warningBox->setObjectName("burnWarning");

    QHBoxLayout* warningLayout = new QHBoxLayout(warningBox);
    warningLayout->setContentsMargins(kWarningMarginHorizontalPx, kWarningMarginVerticalPx,
                                      kWarningMarginHorizontalPx, kWarningMarginVerticalPx);
    warningLayout->setSpacing(kWarningSpacingPx);

    QLabel* warningIcon = new QLabel(QString::fromUtf8("\xe2\x9a\xa0"));
    warningIcon->setObjectName("srWarningIcon");
    warningIcon->setFixedWidth(kWarningIconWidthPx);
    warningLayout->addWidget(warningIcon, 0, Qt::AlignTop);

    QLabel* warningText =
        new QLabel(tr("Burning is permanent. Burned tokens are destroyed and cannot be "
                      "recovered. The total supply of this token will be reduced."));
    warningText->setObjectName("srWarningText");
    warningText->setWordWrap(true);
    warningLayout->addWidget(warningText, 1);

    layout->addWidget(warningBox);

    layout->addSpacing(kSmallSpacingPx);

    m_burnStatusLabel = new QLabel();
    m_burnStatusLabel->setObjectName("srStatusNeutral13");
    initializeStatusLabel(m_burnStatusLabel);
    m_burnStatusLabel->setVisible(false);
    m_burnStatusLabel->setWordWrap(true);
    layout->addWidget(m_burnStatusLabel);

    m_burnBtn = new QPushButton(tr("Burn Tokens"));
    m_burnBtn->setCursor(Qt::PointingHandCursor);
    m_burnBtn->setMinimumHeight(kActionButtonMinHeightPx);
    m_burnBtn->setEnabled(false);
    m_burnBtn->setStyleSheet(Theme::destructiveBtnDisabledStyle);
    connect(m_burnBtn, &QPushButton::clicked, this, &SendReceivePage::executeBurn);
    layout->addWidget(m_burnBtn);

    layout->addStretch();

    m_burnScroll->setWidget(content);
    return m_burnScroll;
}

void SendReceivePage::refreshBurnTokens() {
    if (!m_burnTokenDropdown || m_walletAddress.isEmpty()) {
        return;
    }

    // Reuse the same mint-authority fetch pipeline as the mint page
    QStringList unfetched = TokenAccountDb::getMintsWithoutAuthority(m_walletAddress);
    if (unfetched.isEmpty()) {
        populateBurnDropdownFromDb();
    } else {
        m_burnTokenDropdown->clear();
        m_burnBalanceLabel->setText(tr("Checking mint authorities..."));
        m_burnBtn->setEnabled(false);
        m_burnBtn->setStyleSheet(Theme::destructiveBtnDisabledStyle);
        fetchMintAuthorities();
    }
}

void SendReceivePage::populateBurnDropdownFromDb() {
    if (!m_burnTokenDropdown) {
        return;
    }

    m_burnTokenDropdown->clear();
    m_burnTokenMeta.clear();

    auto mintable = TokenAccountDb::getMintableAccounts(m_walletAddress);
    QString selectedIcon;
    QString selectedDisplay;
    QString selectedBalance;

    for (const auto& acct : mintable) {
        if (acct.tokenAddress == WSOL_MINT) {
            continue;
        }

        KnownToken known = resolveKnownToken(acct.tokenAddress);
        QString sym = known.symbol.isEmpty() ? acct.symbol : known.symbol;
        QString name = acct.name;
        if (sym == acct.tokenAddress.left(6)) {
            sym = acct.tokenAddress.left(6);
        }
        QString display = sym + QString::fromUtf8(" \xe2\x80\x94 ") + name;
        QString balance = acct.balance + " " + sym;
        QString icon = acct.logoUrl.isEmpty() ? "token_default" : acct.logoUrl;

        SendReceiveTokenMeta meta;
        meta.mint = acct.tokenAddress;
        meta.accountAddress = acct.accountAddress;
        meta.decimals = acct.decimals;
        meta.tokenProgram = acct.tokenProgram;
        m_burnTokenMeta.insert(icon, meta);

        m_burnTokenDropdown->addToken(icon, display, balance);
        if (selectedIcon.isEmpty()) {
            selectedIcon = icon;
            selectedDisplay = display;
            selectedBalance = balance;
        }
    }

    if (selectedIcon.isEmpty()) {
        m_burnBalanceLabel->setText(tr("No burnable tokens found."));
        m_burnBtn->setEnabled(false);
        m_burnBtn->setStyleSheet(Theme::destructiveBtnDisabledStyle);
        return;
    }

    m_burnTokenDropdown->sortItems();
    m_burnTokenDropdown->setCurrentToken(selectedIcon, selectedDisplay, selectedBalance);
    m_burnBalanceLabel->setText(tr("Balance: %1").arg(selectedBalance));
    validateBurnForm();
}

void SendReceivePage::validateBurnForm() {
    if (!m_burnBtn || !m_burnAmountInput || !m_burnTokenDropdown) {
        return;
    }

    const QString icon = m_burnTokenDropdown->currentIconPath();
    const bool hasToken = !icon.isEmpty() && m_burnTokenMeta.contains(icon);
    const bool hasAmount = m_burnTokenHandler->isValidBurnAmount(m_burnAmountInput->text());
    const bool valid = hasToken && hasAmount;

    m_burnBtn->setEnabled(valid);
    m_burnBtn->setStyleSheet(valid ? Theme::destructiveBtnStyle
                                   : Theme::destructiveBtnDisabledStyle);
}

void SendReceivePage::executeBurn() {
    if (!m_solanaApi || !m_signer) {
        setStatusLabelState(m_burnStatusLabel, tr("Error: wallet not available for signing."),
                            true);
        return;
    }

    const QString icon = m_burnTokenDropdown->currentIconPath();
    if (!m_burnTokenMeta.contains(icon)) {
        setStatusLabelState(m_burnStatusLabel, tr("Error: token metadata not found."), true);
        return;
    }

    const SendReceiveTokenMeta meta = m_burnTokenMeta.value(icon);
    bool parseOk = false;
    const quint64 rawAmount =
        m_burnTokenHandler->parseBurnAmountRaw(m_burnAmountInput->text(), meta.decimals, &parseOk);
    if (!parseOk || rawAmount == 0) {
        setStatusLabelState(m_burnStatusLabel, tr("Enter a valid burn amount."), true);
        return;
    }

    m_burnBtn->setEnabled(false);
    m_burnBtn->setText(tr("Burning..."));
    m_burnBtn->setStyleSheet(Theme::destructiveBtnDisabledStyle);
    m_burnStatusLabel->setVisible(false);

    auto resetBtn = [this]() {
        m_burnBtn->setEnabled(true);
        m_burnBtn->setText(tr("Burn Tokens"));
        m_burnBtn->setStyleSheet(Theme::destructiveBtnStyle);
    };

    SendReceiveHandler::ExecuteSendCallbacks callbacks;
    callbacks.onStatus = [this](const QString& text, bool isError) {
        setStatusLabelState(m_burnStatusLabel, text, isError);
    };
    callbacks.onSuccess = [this](const QString& txSig) {
        emit transactionSent(txSig);

        const QString tokenSymbol =
            m_burnTokenDropdown->currentText().split(' ', Qt::SkipEmptyParts).first();

        SendReceiveSuccessPageInfo info;
        info.title = tr("Tokens Burned");
        info.amount = m_burnAmountInput->text() + " " + tokenSymbol;
        info.sender = m_walletAddress;
        info.signature = txSig;
        showSuccessPage(info);
        startConfirmationPolling(txSig);
    };
    callbacks.onFinished = [resetBtn]() { resetBtn(); };

    m_burnTokenHandler->executeBurnFlow(m_walletAddress, meta, rawAmount, m_solanaApi, m_signer,
                                        this, callbacks);
}
