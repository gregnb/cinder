#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "widgets/ActionIconButton.h"
#include "widgets/AddressInput.h"
#include "widgets/AddressLink.h"
#include "widgets/AmountInput.h"
#include "widgets/PillButtonGroup.h"
#include "widgets/StyledCheckbox.h"
#include "widgets/TokenDropdown.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
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
    constexpr int kFieldRowSpacingPx = 12;
    constexpr int kFieldColumnSpacingPx = 6;

    constexpr int kReviewButtonHeightPx = 48;
    constexpr int kDeleteColumnWidthPx = 28;
    constexpr int kDeleteButtonContainerHeightPx = 44;
    constexpr int kNonceInfoLeftInsetPx = 4;
    constexpr int kAddressColumnStretch = 3;
    constexpr int kAmountColumnStretch = 1;
    constexpr int kAutoSpeedIndex = 0;
    constexpr int kActionCardMinHeightPx = 240;
    constexpr int kActionCardMarginHorizontalPx = 20;
    constexpr int kActionCardMarginTopPx = 15;
    constexpr int kActionCardMarginBottomPx = 20;
    constexpr int kActionCardTitleSpacingPx = 8;
    constexpr double kActionCardDefaultBorderOpacity = 0.40;
} // namespace

QWidget* SendReceivePage::buildSendForm() {
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
        resetForm();
        if (m_externalEntry) {
            m_externalEntry = false;
            setCurrentPage(StackPage::CardGrid);
            emit backRequested();
        } else {
            setCurrentPage(StackPage::CardGrid);
        }
    });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Send"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    layout->addSpacing(kSmallSpacingPx);

    // ── Token Dropdown ──────────────────────────────────
    QLabel* tokenLabel = new QLabel(tr("Token"));
    tokenLabel->setObjectName("txFormLabel");
    layout->addWidget(tokenLabel);

    m_tokenDropdown = new TokenDropdown();
    // Seed with SOL 0.00 — refreshBalances() will update with real data from DB
    m_tokenDropdown->addToken(":/icons/tokens/sol.png", "SOL  \xe2\x80\x94  Solana", "0.00 SOL");
    m_tokenDropdown->setCurrentToken(":/icons/tokens/sol.png", "SOL  \xe2\x80\x94  Solana",
                                     "0.00 SOL");
    layout->addWidget(m_tokenDropdown);

    layout->addSpacing(kTinySpacingPx);

    // ── Recipients section ──────────────────────────────
    m_recipientsLayout = new QVBoxLayout();
    m_recipientsLayout->setSpacing(kFieldRowSpacingPx);

    // "+ Add Recipient" button (created first so addRecipientRow can insert before it)
    m_addRecipientBtn = new QPushButton("+ " + tr("Add recipient"));
    m_addRecipientBtn->setObjectName("srAddRecipientBtn");
    m_addRecipientBtn->setCursor(Qt::PointingHandCursor);
    connect(m_addRecipientBtn, &QPushButton::clicked, this, &SendReceivePage::addRecipientRow);
    m_recipientsLayout->addWidget(m_addRecipientBtn, 0, Qt::AlignLeft);

    // First recipient row
    addRecipientRow();

    layout->addLayout(m_recipientsLayout);

    layout->addSpacing(kSmallSpacingPx);

    // Priority Fee
    QLabel* speedLabel = new QLabel(tr("Priority Fee"));
    speedLabel->setObjectName("txFormLabel");
    layout->addWidget(speedLabel);
    layout->addWidget(createSpeedSelector());

    layout->addSpacing(kSmallSpacingPx);

    // ── Durable Nonce Option ──────────────────────────────
    m_nonceCheckbox = new StyledCheckbox(tr("Durable Nonce"));
    m_nonceCheckbox->setProperty("uiClass", "check13Dim");
    connect(m_nonceCheckbox, &QCheckBox::toggled, this, &SendReceivePage::onNonceCheckboxToggled);
    layout->addWidget(m_nonceCheckbox);

    m_nonceInfoRow = new QWidget();
    m_nonceInfoRow->setObjectName("srTransparentRow");
    QHBoxLayout* nonceInfoH = new QHBoxLayout(m_nonceInfoRow);
    nonceInfoH->setContentsMargins(kNonceInfoLeftInsetPx, 0, 0, 0);
    nonceInfoH->setSpacing(kSmallSpacingPx);

    QLabel* nonceLabel = new QLabel(tr("Nonce Address:"));
    nonceLabel->setObjectName("srMutedLabel13");
    nonceInfoH->addWidget(nonceLabel);

    m_nonceAddrLabel = new AddressLink({});
    nonceInfoH->addWidget(m_nonceAddrLabel);
    nonceInfoH->addStretch();

    m_nonceInfoRow->setVisible(false);
    layout->addWidget(m_nonceInfoRow);

    layout->addSpacing(kSmallSpacingPx);

    // Review button — starts disabled until form is valid
    m_reviewBtn = new QPushButton(tr("Review Transaction"));
    m_reviewBtn->setObjectName("reviewTxButton");
    m_reviewBtn->setCursor(Qt::PointingHandCursor);
    m_reviewBtn->setMinimumHeight(kReviewButtonHeightPx);
    m_reviewBtn->setEnabled(false);
    m_reviewBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(m_reviewBtn, &QPushButton::clicked, this, [this]() {
        populateReviewPage();
        setCurrentPage(StackPage::Review);
    });
    layout->addWidget(m_reviewBtn);

    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

// ── Recipient row management ────────────────────────────────────

void SendReceivePage::addRecipientRow() {
    bool canDelete = !m_recipientRows.isEmpty();

    RecipientRow row;
    row.container = new QWidget();

    QHBoxLayout* rowLayout = new QHBoxLayout(row.container);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(kFieldRowSpacingPx);

    // ── Address column (stretch 3) ──────────────────────
    QVBoxLayout* addrCol = new QVBoxLayout();
    addrCol->setSpacing(kFieldColumnSpacingPx);

    row.label = new QLabel(tr("Recipient address"));
    row.label->setObjectName("txFormLabel");
    addrCol->addWidget(row.label);

    row.addressInput = new AddressInput();
    addrCol->addWidget(row.addressInput);

    rowLayout->addLayout(addrCol, kAddressColumnStretch);

    // ── Amount column (stretch 1) ───────────────────────
    QVBoxLayout* amtCol = new QVBoxLayout();
    amtCol->setSpacing(kFieldColumnSpacingPx);

    QLabel* amountLabel = new QLabel(tr("Amount"));
    amountLabel->setObjectName("txFormLabel");
    amtCol->addWidget(amountLabel);

    row.amountInput = new AmountInput();
    connect(row.amountInput, &AmountInput::maxClicked, this, [this, input = row.amountInput]() {
        QString balText = m_tokenDropdown->currentBalanceText();
        int spaceIdx = balText.indexOf(' ');
        QString amount = (spaceIdx > 0) ? balText.left(spaceIdx) : balText;
        double bal = amount.toDouble();

        // For native SOL, reserve lamports for fees so the tx doesn't fail.
        // Base fee (5000 lamports) + estimated priority fee (priority-fee-ceiling
        // micro-lamports/CU * compute-unit-limit / 1e6) + small buffer.
        auto it = m_tokenMeta.find(m_tokenDropdown->currentIconPath());
        bool isSol = it != m_tokenMeta.end() &&
                     it->mint == QStringLiteral("So11111111111111111111111111111111111111112");
        if (isSol) {
            constexpr double kBaseFee = 5000.0;            // lamports
            constexpr double kPriorityEstimate = 100000.0; // lamports (~median priority)
            constexpr double kBuffer = 50000.0;            // lamports safety margin
            double reserveLamports = kBaseFee + kPriorityEstimate + kBuffer;
            double reserveSol = reserveLamports / 1e9;
            if (bal > reserveSol) {
                bal -= reserveSol;
                amount = QString::number(bal, 'f', 9);
                while (amount.endsWith('0')) {
                    amount.chop(1);
                }
                if (amount.endsWith('.')) {
                    amount.chop(1);
                }
            }
        }

        input->setText(amount);
    });
    amtCol->addWidget(row.amountInput);
    rowLayout->addLayout(amtCol, kAmountColumnStretch);

    // ── Delete column (hideable — only visible when 2+ recipients) ──
    // Mirrors the exact layout structure of addr/amt columns so Qt
    // aligns everything automatically (no pixel guessing).
    row.deleteCol = new QWidget();
    row.deleteCol->setFixedWidth(kDeleteColumnWidthPx);

    QVBoxLayout* delLayout = new QVBoxLayout(row.deleteCol);
    delLayout->setContentsMargins(0, 0, 0, 0);
    delLayout->setSpacing(kFieldColumnSpacingPx); // same spacing as addrCol / amtCol

    // Invisible label — same style as "Recipient address" / "Amount"
    // so it gets the identical height, keeping rows vertically aligned
    QLabel* delLabel = new QLabel(QStringLiteral(" "));
    delLabel->setObjectName("txFormLabel");
    delLayout->addWidget(delLabel);

    // 44px container matching input field height, button centered inside
    QWidget* btnContainer = new QWidget();
    btnContainer->setFixedHeight(kDeleteButtonContainerHeightPx);
    QHBoxLayout* btnInner = new QHBoxLayout(btnContainer);
    btnInner->setContentsMargins(0, 0, 0, 0);

    row.deleteBtn = nullptr;
    if (canDelete) {
        row.deleteBtn = new ActionIconButton(":/icons/action/delete.png");

        connect(row.deleteBtn, &QPushButton::clicked, this, [this, container = row.container]() {
            for (int i = 0; i < m_recipientRows.size(); ++i) {
                if (m_recipientRows[i].container == container) {
                    removeRecipientRow(i);
                    break;
                }
            }
        });

        btnInner->addWidget(row.deleteBtn, 0, Qt::AlignCenter);
    }

    delLayout->addWidget(btnContainer);

    row.deleteCol->setVisible(false); // hidden by default, shown when multi
    rowLayout->addWidget(row.deleteCol);

    // Wire validation — re-check form whenever address or amount changes
    connect(row.addressInput, &AddressInput::addressChanged, this, &SendReceivePage::validateForm);
    connect(row.amountInput, &AmountInput::textChanged, this, &SendReceivePage::validateForm);

    // Insert into layout before the "+ Add Recipient" button
    int insertIdx = m_recipientRows.size();
    m_recipientRows.append(row);
    m_recipientsLayout->insertWidget(insertIdx, row.container);

    updateRecipientLabels();
    validateForm();
}

void SendReceivePage::removeRecipientRow(int index) {
    if (index <= 0 || index >= m_recipientRows.size()) {
        return; // can't delete first row or invalid index
    }

    RecipientRow row = m_recipientRows.takeAt(index);
    m_recipientsLayout->removeWidget(row.container);
    row.container->deleteLater();

    updateRecipientLabels();
    validateForm();
}

void SendReceivePage::updateRecipientLabels() {
    bool multi = m_recipientRows.size() > 1;
    for (int i = 0; i < m_recipientRows.size(); ++i) {
        if (multi) {
            m_recipientRows[i].label->setText(tr("Recipient address - #%1").arg(i + 1));
        } else {
            m_recipientRows[i].label->setText(tr("Recipient address"));
        }
        // Show/hide delete column based on multi-recipient state
        if (m_recipientRows[i].deleteCol) {
            m_recipientRows[i].deleteCol->setVisible(multi);
        }
    }
}

void SendReceivePage::validateForm() {
    if (!m_reviewBtn) {
        return; // called during construction before button exists
    }

    QList<SendReceiveRecipientInput> recipientInputs;
    recipientInputs.reserve(m_recipientRows.size());
    for (const auto& row : m_recipientRows) {
        recipientInputs.append({row.addressInput->address(), row.amountInput->text()});
    }
    const bool valid = m_handler->hasValidRecipient(recipientInputs);

    m_reviewBtn->setEnabled(valid);
    m_reviewBtn->setStyleSheet(valid ? Theme::primaryBtnStyle : Theme::primaryBtnDisabledStyle);
}

void SendReceivePage::resetForm() {
    // Remove all rows except the first
    while (m_recipientRows.size() > 1) {
        removeRecipientRow(m_recipientRows.size() - 1);
    }

    // Clear the remaining first row
    if (!m_recipientRows.isEmpty()) {
        m_recipientRows[0].addressInput->setAddress(QString());
        m_recipientRows[0].amountInput->clear();
    }

    // Reset token dropdown to default (SOL) — use list data for real balance
    if (m_tokenDropdown) {
        if (!m_tokenDropdown->selectByIcon(":/icons/tokens/sol.png")) {
            m_tokenDropdown->setCurrentToken(":/icons/tokens/sol.png", "SOL  — Solana", "0.00");
        }
    }

    // Reset nonce state
    stopNonceDots();
    if (m_nonceCheckbox) {
        m_nonceCheckbox->setChecked(false);
    }
    if (m_nonceInfoRow) {
        m_nonceInfoRow->setVisible(false);
    }
    m_nonceEnabled = false;
    m_nonceAddress.clear();
    m_nonceValue.clear();
    m_pendingNonceAddress.clear();

    validateForm();
}

// ── Public: open send form with a pre-filled recipient ─────────

void SendReceivePage::openWithRecipient(const QString& address) {
    m_externalEntry = true;
    openSendForm(true);
    if (!m_recipientRows.isEmpty()) {
        m_recipientRows[0].addressInput->setAddress(address);
    }
}

void SendReceivePage::openWithToken(const QString& iconPath) {
    m_tokenDropdown->selectByIcon(iconPath);
    setCurrentPage(StackPage::SendForm);
}

void SendReceivePage::openWithMint(const QString& mint) {
    // m_tokenMeta is keyed by icon path — find the entry whose mint matches
    for (auto it = m_tokenMeta.constBegin(); it != m_tokenMeta.constEnd(); ++it) {
        if (it.value().mint == mint) {
            m_tokenDropdown->selectByIcon(it.key());
            break;
        }
    }
    setCurrentPage(StackPage::SendForm);
}

void SendReceivePage::setExternalEntry(bool external) { m_externalEntry = external; }

// ── Open send form with pre-selection ──────────────────────────

void SendReceivePage::openSendForm(bool preSelectSol) {
    QString icon = preSelectSol ? ":/icons/tokens/sol.png" : ":/icons/tokens/usdc.png";
    if (!m_tokenDropdown->selectByIcon(icon)) {
        // Fallback if token isn't in the list yet
        if (preSelectSol) {
            m_tokenDropdown->setCurrentToken(icon, "SOL  \xe2\x80\x94  Solana", "0.00 SOL");
        } else {
            m_tokenDropdown->setCurrentToken(icon, "USDC  \xe2\x80\x94  USD Coin", "0.00 USDC");
        }
    }
    setCurrentPage(StackPage::SendForm);
}

// ── Helpers ─────────────────────────────────────────────────────

QWidget* SendReceivePage::createActionCard(const QString& iconPath, const QColor& accent,
                                           const QString& title, const QString& subtitle,
                                           int iconSize) {
    QWidget* card = new QWidget();
    card->setObjectName("txCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(kActionCardMinHeightPx);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    card->setProperty("accentColor", accent);
    applyCardStyle(card, accent, kActionCardDefaultBorderOpacity);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(kActionCardMarginHorizontalPx, kActionCardMarginTopPx,
                                   kActionCardMarginHorizontalPx, kActionCardMarginBottomPx);
    cardLayout->setAlignment(Qt::AlignHCenter);

    cardLayout->addStretch();

    // Icon — scale for HiDPI/Retina
    QPixmap pixmap(iconPath);
    if (!pixmap.isNull()) {
        qreal dpr = qApp->devicePixelRatio();
        pixmap = pixmap.scaled(iconSize * dpr, iconSize * dpr, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(dpr);
    }
    QLabel* iconLabel = new QLabel();
    iconLabel->setPixmap(pixmap);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);

    cardLayout->addSpacing(kActionCardTitleSpacingPx);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setObjectName("txCardTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(subtitle);
    subtitleLabel->setObjectName("txCardSubtitle");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(subtitleLabel);

    cardLayout->addStretch();

    card->installEventFilter(this);

    return card;
}

void SendReceivePage::applyCardStyle(QWidget* card, const QColor& accent, double borderOpacity) {
    card->setStyleSheet(QString("QWidget#txCard {"
                                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                                "    stop:0 %1, stop:1 %2);"
                                "  border: 1px solid rgba(%3, %4, %5, %6);"
                                "  border-radius: %7px;"
                                "}"
                                "QWidget#txCard QLabel {"
                                "  background: transparent;"
                                "  border: none;"
                                "}")
                            .arg(Theme::cardBgStart)
                            .arg(Theme::cardBgEnd)
                            .arg(accent.red())
                            .arg(accent.green())
                            .arg(accent.blue())
                            .arg(borderOpacity, 0, 'f', 2)
                            .arg(Theme::cardRadius));
}

QWidget* SendReceivePage::createSpeedSelector() {
    auto* container = new QWidget();
    container->setObjectName("srTransparentRow");
    auto* vLayout = new QVBoxLayout(container);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(kSmallSpacingPx);

    // Auto / Custom toggle
    auto* group = new PillButtonGroup();
    group->setObjectNames("speedButton", "speedButtonActive");
    group->addButton(tr("Auto"), 1);
    group->addButton(tr("Custom"), 1);
    group->setActiveIndex(kAutoSpeedIndex);
    m_speedSelector = group;

    // Paint a bolt icon directly (QPushButton icon doesn't render with QSS,
    // so we embed a QLabel child inside the button — same approach as sidebar nav).
    if (QPushButton* autoBtn = group->button(0)) {
        autoBtn->setText({});

        constexpr int kBoltPx = 14;
        qreal dpr = qApp->devicePixelRatio();
        int rpx = static_cast<int>(kBoltPx * dpr);
        QPixmap boltPx(rpx, rpx);
        boltPx.fill(Qt::transparent);
        {
            QPainter p(&boltPx);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#f5c542"));
            double s = rpx / 24.0;
            QPolygonF bolt;
            bolt << QPointF(13 * s, 2 * s) << QPointF(3 * s, 14 * s) << QPointF(12 * s, 14 * s)
                 << QPointF(11 * s, 22 * s) << QPointF(21 * s, 10 * s) << QPointF(12 * s, 10 * s);
            p.drawPolygon(bolt);
        }
        boltPx.setDevicePixelRatio(dpr);

        auto* btnLayout = new QHBoxLayout(autoBtn);
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(4);

        auto* boltLabel = new QLabel(autoBtn);
        boltLabel->setPixmap(boltPx);
        boltLabel->setFixedSize(kBoltPx, kBoltPx);
        boltLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        boltLabel->setObjectName("srTransparentRow");

        auto* textLabel = new QLabel(tr("Auto"), autoBtn);
        textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        textLabel->setObjectName("speedButtonLabel");

        btnLayout->addStretch();
        btnLayout->addWidget(boltLabel, 0, Qt::AlignVCenter);
        btnLayout->addWidget(textLabel, 0, Qt::AlignVCenter);
        btnLayout->addStretch();
    }
    vLayout->addWidget(group);

    // Custom fee input row — right-aligned under the Custom button
    m_customFeeRow = new QWidget();
    m_customFeeRow->setObjectName("srTransparentRow");
    auto* feeRowLayout = new QHBoxLayout(m_customFeeRow);
    feeRowLayout->setContentsMargins(0, 0, 0, 0);
    feeRowLayout->setSpacing(kSmallSpacingPx);

    // Left spacer matches the address column (stretch 3) so the input
    // lines up with the Amount field above it (stretch 1).
    feeRowLayout->addStretch(kAddressColumnStretch);

    m_customFeeInput = new AmountInput();
    m_customFeeInput->setPlaceholderText("0.001");
    feeRowLayout->addWidget(m_customFeeInput, kAmountColumnStretch);

    auto* solSuffix = new QLabel("SOL");
    solSuffix->setObjectName("srKvValue13");
    feeRowLayout->addWidget(solSuffix);

    m_customFeeRow->hide();
    vLayout->addWidget(m_customFeeRow);

    connect(group, &PillButtonGroup::currentChanged, this,
            [this](int index) { m_customFeeRow->setVisible(index == 1); });

    return container;
}

// ── Review Page (index 2) ───────────────────────────────────────
