#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "features/sendreceive/BurnTokenHandler.h"
#include "features/sendreceive/CloseTokenAccountsHandler.h"
#include "features/sendreceive/CreateTokenHandler.h"
#include "features/sendreceive/MintTokenHandler.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "services/AvatarCache.h"
#include "services/SolanaApi.h"
#include "widgets/TokenDropdown.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <memory>

namespace {
    constexpr char kCardTypeProperty[] = "cardType";
    constexpr int kCardGridMarginHorizontalPx = 40;
    constexpr int kCardGridMarginTopPx = 20;
    constexpr int kCardGridMarginBottomPx = 30;
    constexpr int kCardGridSpacingPx = 20;
    constexpr int kPrimaryCardHeightPx = 120;
    constexpr int kSecondaryCardHeightPx = 110;
    constexpr double kCardHoverBorderOpacity = 0.65;
    constexpr double kCardDefaultBorderOpacity = 0.40;

    enum class SendReceiveCardType {
        None = 0,
        SendSol,
        SendToken,
        MintTokens,
        CreateToken,
        BurnTokens,
        CloseAccounts,
    };

    void setCardType(QWidget* widget, SendReceiveCardType type) {
        widget->setProperty(kCardTypeProperty, static_cast<int>(type));
    }

    SendReceiveCardType cardTypeOf(const QWidget* widget) {
        if (!widget) {
            return SendReceiveCardType::None;
        }
        const QVariant value = widget->property(kCardTypeProperty);
        if (!value.isValid()) {
            return SendReceiveCardType::None;
        }
        return static_cast<SendReceiveCardType>(value.toInt());
    }
} // namespace

SendReceivePage::SendReceivePage(QWidget* parent)
    : QWidget(parent), m_handler(std::make_unique<SendReceiveHandler>()),
      m_createTokenHandler(std::make_unique<CreateTokenHandler>(m_handler.get())),
      m_mintTokenHandler(std::make_unique<MintTokenHandler>(m_handler.get())),
      m_burnTokenHandler(std::make_unique<BurnTokenHandler>(m_handler.get())),
      m_closeTokenAccountsHandler(std::make_unique<CloseTokenAccountsHandler>(m_handler.get())) {
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();
    m_stack->addWidget(buildCardGrid());          // index 0
    m_stack->addWidget(buildSendForm());          // index 1
    m_stack->addWidget(buildReviewPage());        // index 2
    m_stack->addWidget(buildCloseAccountsPage()); // index 3
    m_stack->addWidget(buildBurnTokensPage());    // index 4
    m_stack->addWidget(buildNonceSetupPage());    // index 5
    m_stack->addWidget(buildCreateTokenPage());   // index 6
    m_stack->addWidget(buildMintTokensPage());    // index 7
    m_stack->addWidget(buildSuccessPage());       // index 8

    outerLayout->addWidget(m_stack);
}

SendReceivePage::~SendReceivePage() = default;

void SendReceivePage::initializeStatusLabel(QLabel* label) {
    if (!label) {
        return;
    }
    label->setProperty("uiClass", "srStatusLabel");
    label->setProperty("tone", "neutral");
}

void SendReceivePage::setStatusLabelState(QLabel* label, const QString& text, bool isError) {
    if (!label) {
        return;
    }
    label->setText(text);
    label->setProperty("uiClass", "srStatusLabel");
    label->setProperty("tone", isError ? "error" : "neutral");
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
    label->setVisible(true);
}

void SendReceivePage::setCurrentPage(StackPage page) {
    if (!m_stack) {
        return;
    }
    m_stack->setCurrentIndex(static_cast<int>(page));
}

// ── Card Grid (index 0) ────────────────────────────────────────

QWidget* SendReceivePage::buildCardGrid() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("sendReceiveContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kCardGridMarginHorizontalPx, kCardGridMarginTopPx,
                               kCardGridMarginHorizontalPx, kCardGridMarginBottomPx);
    layout->setSpacing(kCardGridSpacingPx);

    // Title
    QLabel* title = new QLabel(tr("New Transaction"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Top row — 2 large cards
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(kCardGridSpacingPx);

    QWidget* sendSolCard =
        createActionCard(":/icons/tx/send-sol.png", Theme::txAccentSendSol, tr("Send SOL"),
                         tr("Transfer SOL to another wallet."), kPrimaryCardHeightPx);
    setCardType(sendSolCard, SendReceiveCardType::SendSol);
    topRow->addWidget(sendSolCard, 1);

    QWidget* sendTokenCard =
        createActionCard(":/icons/tx/send-token.png", Theme::txAccentSendToken, tr("Send Token"),
                         tr("Send SPL tokens to an address."), kPrimaryCardHeightPx);
    setCardType(sendTokenCard, SendReceiveCardType::SendToken);
    topRow->addWidget(sendTokenCard, 1);

    layout->addLayout(topRow);

    // Bottom row — 4 cards
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(kCardGridSpacingPx);

    QWidget* createTokenCard = createActionCard(
        ":/icons/tx/create-token.png", Theme::txAccentCreateToken, tr("Create Token"),
        tr("Mint a new Token-2022 asset."), kSecondaryCardHeightPx);
    setCardType(createTokenCard, SendReceiveCardType::CreateToken);
    bottomRow->addWidget(createTokenCard, 1);

    QWidget* mintTokensCard =
        createActionCard(":/icons/tx/mint-tokens.png", Theme::txAccentMintTokens, tr("Mint Tokens"),
                         tr("Increase supply for your mints."), kSecondaryCardHeightPx);
    setCardType(mintTokensCard, SendReceiveCardType::MintTokens);
    bottomRow->addWidget(mintTokensCard, 1);

    QWidget* burnTokensCard =
        createActionCard(":/icons/tx/burn-tokens.png", Theme::txAccentBurnTokens, tr("Burn Tokens"),
                         tr("Permanently destroy tokens."), kSecondaryCardHeightPx);
    setCardType(burnTokensCard, SendReceiveCardType::BurnTokens);
    bottomRow->addWidget(burnTokensCard, 1);

    QWidget* closeAccountsCard = createActionCard(
        ":/icons/tx/close-accounts.png", Theme::txAccentCloseAccounts, tr("Close Token\nAccounts"),
        tr("Reclaim SOL from empty accounts."), kSecondaryCardHeightPx);
    setCardType(closeAccountsCard, SendReceiveCardType::CloseAccounts);
    bottomRow->addWidget(closeAccountsCard, 1);

    layout->addLayout(bottomRow);
    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

// ── Unified Send Form (index 1) ────────────────────────────────

bool SendReceivePage::eventFilter(QObject* obj, QEvent* event) {
    // Paint the spinning arc on the confirmation spinner widget
    if (obj == m_confirmSpinner && event->type() == QEvent::Paint) {
        QPainter p(m_confirmSpinner);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(253, 214, 99, 200), 2.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        QRectF r(2.0, 2.0, m_confirmSpinner->width() - 4.0, m_confirmSpinner->height() - 4.0);
        p.drawArc(r, m_confirmAngle * 16, 270 * 16);
        return true;
    }

    QWidget* card = qobject_cast<QWidget*>(obj);
    if (!card) {
        return QWidget::eventFilter(obj, event);
    }

    // Close account row click → toggle checkbox
    if (card->objectName() == "closeAccountRow" && event->type() == QEvent::MouseButtonRelease) {
        QCheckBox* cb = card->findChild<QCheckBox*>();
        if (cb) {
            cb->toggle();
        }
        return true;
    }

    if (card->objectName() != "txCard") {
        return QWidget::eventFilter(obj, event);
    }

    QColor accent = card->property("accentColor").value<QColor>();
    if (!accent.isValid()) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Enter) {
        applyCardStyle(card, accent, kCardHoverBorderOpacity);
    } else if (event->type() == QEvent::Leave) {
        applyCardStyle(card, accent, kCardDefaultBorderOpacity);
    } else if (event->type() == QEvent::MouseButtonPress) {
        switch (cardTypeOf(card)) {
            case SendReceiveCardType::SendSol:
                openSendForm(true);
                break;
            case SendReceiveCardType::SendToken:
                openSendForm(false);
                break;
            case SendReceiveCardType::MintTokens:
                refreshMintTokens();
                setCurrentPage(StackPage::MintTokens);
                break;
            case SendReceiveCardType::CloseAccounts:
                populateCloseAccountsPage();
                setCurrentPage(StackPage::CloseAccounts);
                break;
            case SendReceiveCardType::BurnTokens:
                refreshBurnTokens();
                setCurrentPage(StackPage::BurnTokens);
                break;
            case SendReceiveCardType::CreateToken:
                setCurrentPage(StackPage::CreateToken);
                break;
            case SendReceiveCardType::None:
                break;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void SendReceivePage::setWalletAddress(const QString& address) {
    m_walletAddress = address;
    refreshBalances();
}

void SendReceivePage::refreshBalances() {
    if (m_walletAddress.isEmpty()) {
        return;
    }

    const QString currentIcon = m_tokenDropdown->currentIconPath();
    const SendReceiveTokenRefreshResult tokenData =
        m_handler->refreshTokenRows(m_walletAddress, currentIcon);

    m_tokenDropdown->clear();
    m_tokenMeta.clear();
    for (const auto& row : tokenData.rows) {
        m_tokenDropdown->addToken(row.icon, row.display, row.balance);
        m_tokenMeta[row.icon] = row.meta;
    }
    m_tokenDropdown->sortItems();
    m_tokenDropdown->setCurrentToken(tokenData.selectedIcon, tokenData.selectedDisplay,
                                     tokenData.selectedBalance);
}

// ── Service Setters ──────────────────────────────────────────────

void SendReceivePage::setSolanaApi(SolanaApi* api) { m_solanaApi = api; }

void SendReceivePage::setAvatarCache(AvatarCache* cache) { m_tokenDropdown->setAvatarCache(cache); }

void SendReceivePage::setKeypair(const Keypair& kp) { m_keypair = kp; }
void SendReceivePage::setSigner(Signer* signer) { m_signer = signer; }
