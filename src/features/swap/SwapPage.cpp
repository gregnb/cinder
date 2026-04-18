#include "features/swap/SwapPage.h"

#include "Theme.h"
#include "features/swap/SwapHandler.h"
#include "models/Swap.h"
#include "widgets/AmountInput.h"
#include "widgets/TokenDropdown.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

namespace {
    constexpr int kQuoteDebounceMs = 500;
    constexpr int kQuoteRefreshSeconds = 10;
    constexpr int kRefreshIntervalMs = kQuoteRefreshSeconds * 1000;
} // namespace

SwapPage::SwapPage(QWidget* parent) : QWidget(parent), m_handler(new SwapHandler(this)) {
    setObjectName("swapContent");
    setProperty("uiClass", "content");

    m_quoteTimer.setSingleShot(true);
    m_quoteTimer.setInterval(kQuoteDebounceMs);
    connect(&m_quoteTimer, &QTimer::timeout, this, &SwapPage::requestQuote);

    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (m_quoteValid && !m_swapInProgress && !m_amountInput->text().trimmed().isEmpty()) {
            requestQuote();
        }
    });

    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, [this]() {
        m_countdownSeconds = qMax(0, m_countdownSeconds - 1);
        if (m_refreshCountdown && m_quoteValid) {
            m_refreshCountdown->setText(
                QString("Refreshes in %1s").arg(QString::number(m_countdownSeconds)));
        }
    });

    connect(m_handler, &SwapHandler::tokenOptionsReady, this, &SwapPage::applyTokenOptions);
    connect(m_handler, &SwapHandler::quoteUpdated, this, &SwapPage::applyQuoteView);
    connect(m_handler, &SwapHandler::quoteCleared, this, [this]() {
        m_quoteDetails->setVisible(false);
        m_estimatedOutput->setText("--");
        m_rateLabel->setText("--");
        m_routeLabel->setText("--");
        m_priceImpactLabel->setText("--");
        m_minReceivedLabel->setText("--");
        m_slippageLabel->setText("--");
        m_priceImpactLabel->setProperty("highImpact", false);
        m_priceImpactLabel->style()->unpolish(m_priceImpactLabel);
        m_priceImpactLabel->style()->polish(m_priceImpactLabel);
        stopRefreshTimers();
        if (m_swapInProgress) {
            return;
        }
        updateSwapButton();
    });
    connect(m_handler, &SwapHandler::statusChanged, this,
            [this](const QString& text, bool isError) { showStatus(text, isError); });
    connect(m_handler, &SwapHandler::quoteValidityChanged, this, [this](bool valid) {
        m_quoteValid = valid;
        updateSwapButton();
    });
    connect(m_handler, &SwapHandler::swapInProgressChanged, this, [this](bool inProgress) {
        m_swapInProgress = inProgress;
        if (inProgress) {
            stopRefreshTimers();
        }
        updateSwapButton();
    });

    buildUI();
    m_handler->refreshBalances();
}

void SwapPage::setSolanaApi(SolanaApi* api) { m_handler->setSolanaApi(api); }

void SwapPage::setKeypair(const Keypair& kp) {
    Q_UNUSED(kp)
    refreshBalances();
}

void SwapPage::setSigner(Signer* signer) { m_handler->setSigner(signer); }

void SwapPage::setWalletAddress(const QString& address) {
    m_walletAddress = address;
    m_handler->setWalletAddress(address);
    refreshBalances();
}

void SwapPage::refreshBalances() { m_handler->refreshBalances(); }

void SwapPage::stopRefreshTimers() {
    m_refreshTimer.stop();
    m_countdownTimer.stop();
    if (m_refreshCountdown) {
        m_refreshCountdown->setVisible(false);
    }
}

void SwapPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    if (m_quoteValid && !m_swapInProgress && !m_amountInput->text().trimmed().isEmpty()) {
        m_countdownSeconds = kQuoteRefreshSeconds;
        m_refreshTimer.start();
        m_countdownTimer.start();
        if (m_refreshCountdown) {
            m_refreshCountdown->setText(QString("Refreshes in %1s").arg(kQuoteRefreshSeconds));
            m_refreshCountdown->setVisible(true);
        }
    }
}

void SwapPage::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    stopRefreshTimers();
}

void SwapPage::buildUI() {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("swapInner");
    content->setProperty("uiClass", "content");

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    QLabel* title = new QLabel(tr("Swap"));
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    QHBoxLayout* cardRow = new QHBoxLayout();
    cardRow->addStretch();

    QWidget* card = new QWidget();
    card->setObjectName("swapCard");
    card->setFixedWidth(480);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(16);

    QLabel* fromLabel = new QLabel(tr("From"));
    fromLabel->setObjectName("swapSectionLabel");
    cardLayout->addWidget(fromLabel);

    m_fromToken = new TokenDropdown();
    cardLayout->addWidget(m_fromToken);

    m_amountInput = new AmountInput();
    connect(m_amountInput, &AmountInput::maxClicked, this, [this]() {
        if (!m_fromToken || m_walletAddress.isEmpty()) {
            return;
        }

        const QString balanceText = m_fromToken->currentBalanceText();
        const int spaceIndex = balanceText.indexOf(' ');
        const QString amount = (spaceIndex > 0) ? balanceText.left(spaceIndex) : balanceText;
        m_amountInput->setText(amount);
    });
    cardLayout->addWidget(m_amountInput);

    QHBoxLayout* flipRow = new QHBoxLayout();
    flipRow->addStretch();
    m_flipBtn = new QPushButton("⇅");
    m_flipBtn->setFixedSize(40, 40);
    m_flipBtn->setObjectName("swapFlipBtn");
    m_flipBtn->setCursor(Qt::PointingHandCursor);
    connect(m_flipBtn, &QPushButton::clicked, this, &SwapPage::swapDirection);
    flipRow->addWidget(m_flipBtn);
    flipRow->addStretch();
    cardLayout->addLayout(flipRow);

    QLabel* toLabel = new QLabel(tr("To (estimated)"));
    toLabel->setObjectName("swapSectionLabel");
    cardLayout->addWidget(toLabel);

    m_toToken = new TokenDropdown();
    cardLayout->addWidget(m_toToken);

    m_estimatedOutput = new QLabel("--");
    m_estimatedOutput->setObjectName("swapEstimatedOutput");
    cardLayout->addWidget(m_estimatedOutput);

    m_refreshCountdown = new QLabel();
    m_refreshCountdown->setObjectName("swapRefreshCountdown");
    m_refreshCountdown->setVisible(false);
    cardLayout->addWidget(m_refreshCountdown);

    m_quoteDetails = new QWidget();
    m_quoteDetails->setObjectName("swapQuoteDetails");
    m_quoteDetails->setVisible(false);

    QVBoxLayout* detailsLayout = new QVBoxLayout(m_quoteDetails);
    detailsLayout->setContentsMargins(0, 8, 0, 0);
    detailsLayout->setSpacing(6);

    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    separator->setObjectName("swapSeparator");
    detailsLayout->addWidget(separator);

    auto addDetailRow = [&](const QString& label) -> QLabel* {
        QWidget* row = new QWidget();
        row->setObjectName("swapDetailRow");

        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 4, 0, 4);

        QLabel* rowLabel = new QLabel(label);
        rowLabel->setObjectName("swapDetailLabel");
        rowLayout->addWidget(rowLabel);
        rowLayout->addStretch();

        QLabel* valueLabel = new QLabel("--");
        valueLabel->setObjectName("swapDetailValue");
        rowLayout->addWidget(valueLabel);

        detailsLayout->addWidget(row);
        return valueLabel;
    };

    m_rateLabel = addDetailRow(tr("Rate"));
    m_routeLabel = addDetailRow(tr("Route"));
    m_priceImpactLabel = addDetailRow(tr("Price Impact"));
    m_priceImpactLabel->setProperty("highImpact", false);
    m_minReceivedLabel = addDetailRow(tr("Minimum Received"));
    m_slippageLabel = addDetailRow(tr("Slippage Tolerance"));

    cardLayout->addWidget(m_quoteDetails);

    m_statusLabel = new QLabel();
    m_statusLabel->setObjectName("swapStatusLabel");
    m_statusLabel->setProperty("isError", false);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);
    cardLayout->addWidget(m_statusLabel);

    m_swapBtn = new QPushButton(tr("Swap"));
    m_swapBtn->setFixedHeight(48);
    m_swapBtn->setObjectName("swapPrimaryBtn");
    m_swapBtn->setCursor(Qt::PointingHandCursor);
    m_swapBtn->setEnabled(false);
    connect(m_swapBtn, &QPushButton::clicked, this, &SwapPage::executeSwap);
    cardLayout->addWidget(m_swapBtn);

    cardRow->addWidget(card);
    cardRow->addStretch();
    layout->addLayout(cardRow);
    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto invalidateAndRequest = [this]() {
        m_handler->invalidateQuote();
        if (!m_amountInput->text().isEmpty()) {
            m_quoteTimer.start();
        }
    };

    connect(m_fromToken, &TokenDropdown::tokenSelected, this,
            [invalidateAndRequest](const QString&, const QString&) { invalidateAndRequest(); });
    connect(m_toToken, &TokenDropdown::tokenSelected, this,
            [invalidateAndRequest](const QString&, const QString&) { invalidateAndRequest(); });
    connect(m_amountInput, &AmountInput::textChanged, this,
            [this, invalidateAndRequest](const QString&) {
                invalidateAndRequest();
                updateSwapButton();
            });
}

void SwapPage::requestQuote() {
    m_handler->requestQuote(m_fromToken->currentText(), m_toToken->currentText(),
                            m_amountInput->text());
}

void SwapPage::executeSwap() { m_handler->executeSwap(); }

void SwapPage::swapDirection() {
    const QString fromDisplay = m_fromToken->currentText();
    const QString fromIcon = m_fromToken->currentIconPath();
    const QString toDisplay = m_toToken->currentText();
    const QString toIcon = m_toToken->currentIconPath();

    m_fromToken->setCurrentToken(toIcon, toDisplay);
    m_toToken->setCurrentToken(fromIcon, fromDisplay);

    m_handler->invalidateQuote();

    if (!m_amountInput->text().isEmpty()) {
        m_quoteTimer.start();
    }
}

void SwapPage::updateSwapButton() {
    if (m_swapInProgress) {
        m_swapBtn->setText(tr("Swapping..."));
        m_swapBtn->setEnabled(false);
        return;
    }

    const bool hasAmount = !m_amountInput->text().trimmed().isEmpty();
    const bool valid = hasAmount && m_quoteValid && !m_walletAddress.isEmpty();

    m_swapBtn->setEnabled(valid);
    m_swapBtn->setText(hasAmount ? (m_quoteValid ? tr("Swap") : tr("Fetching quote..."))
                                 : tr("Enter an amount"));
}

void SwapPage::showStatus(const QString& text, bool isError) {
    if (text.isEmpty()) {
        m_statusLabel->setVisible(false);
        return;
    }

    m_statusLabel->setText(text);
    m_statusLabel->setProperty("isError", isError);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->setVisible(true);
}

void SwapPage::applyTokenOptions(const QList<SwapTokenOption>& fromOptions,
                                 const QString& fromDefaultDisplay,
                                 const QList<SwapTokenOption>& toOptions,
                                 const QString& toDefaultDisplay) {
    m_fromToken->clear();
    m_toToken->clear();

    for (const auto& opt : fromOptions) {
        m_fromToken->addToken(opt.iconPath, opt.displayName, opt.balanceText);
    }
    m_fromToken->sortItems();

    for (const auto& opt : toOptions) {
        m_toToken->addToken(opt.iconPath, opt.displayName, opt.balanceText);
    }
    m_toToken->sortItems();

    for (const auto& opt : fromOptions) {
        if (opt.displayName == fromDefaultDisplay) {
            m_fromToken->setCurrentToken(opt.iconPath, opt.displayName, opt.balanceText);
            break;
        }
    }

    for (const auto& opt : toOptions) {
        if (opt.displayName == toDefaultDisplay) {
            m_toToken->setCurrentToken(opt.iconPath, opt.displayName, opt.balanceText);
            break;
        }
    }
}

void SwapPage::applyQuoteView(const SwapQuoteView& view) {
    m_estimatedOutput->setText(view.estimatedOutput);
    m_rateLabel->setText(view.rateText);
    m_routeLabel->setText(view.routeText);
    m_priceImpactLabel->setText(view.priceImpactText);
    m_minReceivedLabel->setText(view.minReceivedText);
    m_slippageLabel->setText(view.slippageText);
    m_quoteDetails->setVisible(true);

    if (view.highPriceImpact) {
        m_priceImpactLabel->setProperty("highImpact", true);
    } else {
        m_priceImpactLabel->setProperty("highImpact", false);
    }
    m_priceImpactLabel->style()->unpolish(m_priceImpactLabel);
    m_priceImpactLabel->style()->polish(m_priceImpactLabel);

    m_countdownSeconds = kQuoteRefreshSeconds;
    m_refreshTimer.start();
    m_countdownTimer.start();
    m_refreshCountdown->setText(QString("Refreshes in %1s").arg(kQuoteRefreshSeconds));
    m_refreshCountdown->setVisible(true);
}
