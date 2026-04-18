#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "db/TokenAccountDb.h"
#include "features/sendreceive/MintTokenHandler.h"
#include "services/SolanaApi.h"
#include "tx/Base58.h"
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
    QString symbolFromDisplay(const QString& displayName) {
        return displayName.split(' ', Qt::SkipEmptyParts).first();
    }

    constexpr int kPageMarginHorizontalPx = 40;
    constexpr int kPageMarginTopPx = 20;
    constexpr int kPageMarginBottomPx = 30;
    constexpr int kPageSpacingPx = 16;
    constexpr int kSmallSpacingPx = 8;
    constexpr int kActionButtonMinHeightPx = 48;
} // namespace

QWidget* SendReceivePage::buildMintTokensPage() {
    m_mintScroll = new QScrollArea();
    m_mintScroll->setWidgetResizable(true);
    m_mintScroll->setFrameShape(QFrame::NoFrame);
    m_mintScroll->viewport()->setProperty("uiClass", "contentViewport");

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

    QLabel* title = new QLabel(tr("Mint Tokens"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    QLabel* desc = new QLabel(
        tr("Mint additional supply to your wallet for a token you control as mint authority."));
    desc->setObjectName("srSubtleDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(kSmallSpacingPx);

    QLabel* tokenLabel = new QLabel(tr("Token"));
    tokenLabel->setObjectName("txFormLabel");
    layout->addWidget(tokenLabel);

    m_mintTokenDropdown = new TokenDropdown();
    layout->addWidget(m_mintTokenDropdown);

    m_mintBalanceLabel = new QLabel(tr("Balance: 0"));
    m_mintBalanceLabel->setObjectName("srSubtleInfo13");
    layout->addWidget(m_mintBalanceLabel);

    connect(m_mintTokenDropdown, &TokenDropdown::tokenSelected, this,
            [this](const QString&, const QString&) {
                const QString bal = m_mintTokenDropdown->currentBalanceText();
                m_mintBalanceLabel->setText(tr("Balance: %1").arg(bal.isEmpty() ? "0" : bal));
                validateMintForm();
            });

    QLabel* amountLabel = new QLabel(tr("Amount to Mint"));
    amountLabel->setObjectName("txFormLabel");
    layout->addWidget(amountLabel);

    m_mintAmountInput = new AmountInput();
    layout->addWidget(m_mintAmountInput);
    connect(m_mintAmountInput, &AmountInput::textChanged, this, &SendReceivePage::validateMintForm);

    m_mintStatusLabel = new QLabel();
    m_mintStatusLabel->setObjectName("srStatusNeutral13");
    initializeStatusLabel(m_mintStatusLabel);
    m_mintStatusLabel->setVisible(false);
    m_mintStatusLabel->setWordWrap(true);
    layout->addWidget(m_mintStatusLabel);

    m_mintBtn = new QPushButton(tr("Mint Tokens"));
    m_mintBtn->setCursor(Qt::PointingHandCursor);
    m_mintBtn->setMinimumHeight(kActionButtonMinHeightPx);
    m_mintBtn->setEnabled(false);
    m_mintBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(m_mintBtn, &QPushButton::clicked, this, &SendReceivePage::executeMint);
    layout->addWidget(m_mintBtn);

    layout->addStretch();
    m_mintScroll->setWidget(content);
    return m_mintScroll;
}

void SendReceivePage::refreshMintTokens() {
    if (!m_mintTokenDropdown || m_walletAddress.isEmpty()) {
        return;
    }

    // Check if any tokens still need mint_authority fetched
    QStringList unfetched = TokenAccountDb::getMintsWithoutAuthority(m_walletAddress);
    if (unfetched.isEmpty()) {
        populateMintDropdownFromDb();
    } else {
        m_mintTokenDropdown->clear();
        m_mintBalanceLabel->setText(tr("Checking mint authorities..."));
        m_mintBtn->setEnabled(false);
        m_mintBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
        fetchMintAuthorities();
    }
}

void SendReceivePage::fetchMintAuthorities() {
    if (!m_solanaApi || m_walletAddress.isEmpty()) {
        return;
    }

    QStringList unfetched = TokenAccountDb::getMintsWithoutAuthority(m_walletAddress);
    if (unfetched.isEmpty()) {
        populateMintDropdownFromDb();
        populateBurnDropdownFromDb();
        return;
    }

    m_mintAuthorityPending = QSet<QString>(unfetched.begin(), unfetched.end());

    disconnect(m_mintAuthorityConn);
    m_mintAuthorityConn =
        connect(m_solanaApi, &SolanaApi::accountInfoReady, this,
                [this](const QString& address, const QByteArray& data, const QString&, quint64) {
                    if (!m_mintAuthorityPending.contains(address)) {
                        return;
                    }
                    m_mintAuthorityPending.remove(address);

                    // Parse SPL Token Mint layout: bytes 0-3 = COption tag, 4-35 = authority pubkey
                    QString authority;
                    if (data.size() >= 36) {
                        quint32 tag = 0;
                        memcpy(&tag, data.constData(), 4); // LE
                        if (tag == 1) {
                            QByteArray pubkey = data.mid(4, 32);
                            authority = Base58::encode(pubkey);
                        }
                    }

                    if (authority.isEmpty()) {
                        authority = ""; // No mint authority (immutable)
                    }
                    TokenAccountDb::setMintAuthority(address, authority);

                    if (m_mintAuthorityPending.isEmpty()) {
                        disconnect(m_mintAuthorityConn);
                        populateMintDropdownFromDb();
                        populateBurnDropdownFromDb();
                    }
                });

    for (const QString& mint : unfetched) {
        m_solanaApi->fetchAccountInfo(mint);
    }
}

void SendReceivePage::populateMintDropdownFromDb() {
    if (!m_mintTokenDropdown) {
        return;
    }

    m_mintTokenDropdown->clear();
    m_mintTokenMeta.clear();

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

        // Register in m_tokenMeta so executeMint can find it
        SendReceiveTokenMeta meta;
        meta.mint = acct.tokenAddress;
        meta.decimals = acct.decimals;
        meta.tokenProgram = acct.tokenProgram;
        m_mintTokenMeta.insert(icon, meta);

        m_mintTokenDropdown->addToken(icon, display, balance);
        if (selectedIcon.isEmpty()) {
            selectedIcon = icon;
            selectedDisplay = display;
            selectedBalance = balance;
        }
    }

    if (selectedIcon.isEmpty()) {
        m_mintBalanceLabel->setText(tr("No mintable tokens found."));
        m_mintBtn->setEnabled(false);
        m_mintBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
        return;
    }

    m_mintTokenDropdown->sortItems();
    m_mintTokenDropdown->setCurrentToken(selectedIcon, selectedDisplay, selectedBalance);
    m_mintBalanceLabel->setText(tr("Balance: %1").arg(selectedBalance));
    validateMintForm();
}

void SendReceivePage::validateMintForm() {
    if (!m_mintBtn || !m_mintAmountInput || !m_mintTokenDropdown) {
        return;
    }

    const QString icon = m_mintTokenDropdown->currentIconPath();
    const bool hasToken = !icon.isEmpty() && m_mintTokenMeta.contains(icon);
    const bool hasAmount = m_mintTokenHandler->isValidMintAmount(m_mintAmountInput->text());
    const bool valid = hasToken && hasAmount;

    m_mintBtn->setEnabled(valid);
    m_mintBtn->setStyleSheet(valid ? Theme::primaryBtnStyle : Theme::primaryBtnDisabledStyle);
}

void SendReceivePage::executeMint() {
    if (!m_solanaApi || !m_signer) {
        setStatusLabelState(m_mintStatusLabel, tr("Error: wallet not available for signing."),
                            true);
        return;
    }

    const QString icon = m_mintTokenDropdown->currentIconPath();
    if (!m_mintTokenMeta.contains(icon)) {
        setStatusLabelState(m_mintStatusLabel, tr("Error: token metadata not found."), true);
        return;
    }

    const SendReceiveTokenMeta meta = m_mintTokenMeta.value(icon);
    bool parseOk = false;
    const quint64 rawAmount =
        m_mintTokenHandler->parseMintAmountRaw(m_mintAmountInput->text(), meta.decimals, &parseOk);
    if (!parseOk || rawAmount == 0) {
        setStatusLabelState(m_mintStatusLabel, tr("Enter a valid mint amount."), true);
        return;
    }

    m_mintBtn->setEnabled(false);
    m_mintBtn->setText(tr("Minting..."));
    m_mintBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    m_mintStatusLabel->setVisible(false);

    auto resetBtn = [this]() {
        m_mintBtn->setEnabled(true);
        m_mintBtn->setText(tr("Mint Tokens"));
        m_mintBtn->setStyleSheet(Theme::primaryBtnStyle);
    };

    SendReceiveHandler::ExecuteSendCallbacks callbacks;
    callbacks.onStatus = [this](const QString& text, bool isError) {
        setStatusLabelState(m_mintStatusLabel, text, isError);
    };
    callbacks.onSuccess = [this](const QString& txSig) {
        emit transactionSent(txSig);

        const QString tokenSymbol =
            m_mintTokenDropdown->currentText().split(' ', Qt::SkipEmptyParts).first();

        SendReceiveSuccessPageInfo info;
        info.title = tr("Tokens Minted");
        info.amount = m_mintAmountInput->text() + " " + tokenSymbol;
        info.sender = m_walletAddress;
        info.signature = txSig;
        showSuccessPage(info);
        startConfirmationPolling(txSig);
    };
    callbacks.onFinished = [resetBtn]() { resetBtn(); };

    m_mintTokenHandler->executeMintFlow(m_walletAddress, meta, rawAmount, m_solanaApi, m_signer,
                                        this, callbacks);
}
