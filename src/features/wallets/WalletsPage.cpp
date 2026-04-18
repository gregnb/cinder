#include "WalletsPage.h"
#include "Theme.h"
#include "widgets/ActionIconButton.h"
#include "widgets/AddressLink.h"
#include "widgets/UploadWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

// ── Tree Connector ───────────────────────────────────────────────

class TreeConnector : public QWidget {
  public:
    explicit TreeConnector(bool isLast, QWidget* parent = nullptr)
        : QWidget(parent), m_isLast(isLast) {
        setFixedWidth(28);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor lineColor(100, 150, 255, 65);
        p.setPen(QPen(lineColor, 1.5));

        int midX = 10;
        int midY = height() / 2;

        // Vertical line: top → center (always)
        p.drawLine(midX, 0, midX, midY);

        // Vertical line: center → bottom (if not last child)
        if (!m_isLast) {
            p.drawLine(midX, midY, midX, height());
        }

        // Horizontal line: from vertical to right edge
        p.drawLine(midX, midY, width() - 2, midY);
    }

  private:
    bool m_isLast;
};

// ── Wallet Grouping ──────────────────────────────────────────────

QList<WalletGroup> WalletsPage::groupWallets(const QList<WalletSummaryRecord>& wallets) {
    QList<WalletGroup> groups;
    QMap<int, int> parentToGroupIndex;

    // First pass: collect root wallets
    for (const auto& w : wallets) {
        if (w.parentWalletId <= 0) {
            WalletGroup group;
            group.parent = w;
            parentToGroupIndex[w.id] = groups.size();
            groups.append(group);
        }
    }

    // Second pass: assign children to their parent's group
    for (const auto& w : wallets) {
        if (w.parentWalletId > 0) {
            if (parentToGroupIndex.contains(w.parentWalletId)) {
                groups[parentToGroupIndex[w.parentWalletId]].children.append(w);
            } else {
                // Orphan — treat as standalone root
                WalletGroup group;
                group.parent = w;
                groups.append(group);
            }
        }
    }

    // Sort children by accountIndex within each group
    for (auto& group : groups) {
        std::sort(group.children.begin(), group.children.end(),
                  [](const WalletSummaryRecord& a, const WalletSummaryRecord& b) {
                      return a.accountIndex < b.accountIndex;
                  });
    }

    return groups;
}

// ── WalletsPage ──────────────────────────────────────────────────

WalletsPage::WalletsPage(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();
    m_stack->addWidget(buildListView());   // Step::List
    m_stack->addWidget(buildDetailView()); // Step::Detail
    outer->addWidget(m_stack);

    m_revealTimer = new QTimer(this);
    m_revealTimer->setSingleShot(true);
    connect(m_revealTimer, &QTimer::timeout, this, [this]() {
        if (m_revealedContainer) {
            m_revealedContainer->hide();
        }
        if (m_revealBtn) {
            m_revealBtn->show();
        }
        m_countdownTicker->stop();
    });

    m_countdownTicker = new QTimer(this);
    m_countdownTicker->setInterval(1000);
    connect(m_countdownTicker, &QTimer::timeout, this, [this]() {
        --m_countdownSecs;
        if (m_timerLabel && m_countdownSecs > 0) {
            m_timerLabel->setText(tr("Auto-hides in %1s").arg(m_countdownSecs));
        }
    });

    m_recoveryRevealTimer = new QTimer(this);
    m_recoveryRevealTimer->setSingleShot(true);
    connect(m_recoveryRevealTimer, &QTimer::timeout, this, [this]() {
        if (m_recoveryRevealedContainer) {
            m_recoveryRevealedContainer->hide();
        }
        if (m_recoveryRevealBtn) {
            m_recoveryRevealBtn->show();
        }
        m_recoveryCountdownTicker->stop();
    });

    m_recoveryCountdownTicker = new QTimer(this);
    m_recoveryCountdownTicker->setInterval(1000);
    connect(m_recoveryCountdownTicker, &QTimer::timeout, this, [this]() {
        --m_recoveryCountdownSecs;
        if (m_recoveryTimerLabel && m_recoveryCountdownSecs > 0) {
            m_recoveryTimerLabel->setText(tr("Auto-hides in %1s").arg(m_recoveryCountdownSecs));
        }
    });
}

void WalletsPage::showStep(Step step) { m_stack->setCurrentIndex(static_cast<int>(step)); }

void WalletsPage::refresh() { rebuildWalletList(); }

void WalletsPage::setActiveAddress(const QString& address) { m_activeAddress = address; }

void WalletsPage::hidePrivateKeyReveal() {
    if (m_revealedContainer) {
        m_revealedContainer->hide();
    }
    if (m_revealBtn) {
        m_revealBtn->show();
    }
    if (m_revealTimer) {
        m_revealTimer->stop();
    }
    if (m_countdownTicker) {
        m_countdownTicker->stop();
    }
}

void WalletsPage::hideRecoveryPhraseReveal() {
    if (m_recoveryRevealedContainer) {
        m_recoveryRevealedContainer->hide();
    }
    if (m_recoveryRevealBtn) {
        m_recoveryRevealBtn->show();
    }
    if (m_recoveryRevealTimer) {
        m_recoveryRevealTimer->stop();
    }
    if (m_recoveryCountdownTicker) {
        m_recoveryCountdownTicker->stop();
    }
}

// ── List View ─────────────────────────────────────────────────────

QWidget* WalletsPage::buildListView() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setObjectName("accountsContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    m_title = new QLabel(tr("Wallets (0)"));
    m_title->setObjectName("pageTitle");
    layout->addWidget(m_title);

    m_listLayout = new QVBoxLayout();
    m_listLayout->setSpacing(0);
    layout->addLayout(m_listLayout);

    // Add Wallet button
    QPushButton* addBtn = new QPushButton("+ " + tr("Add Wallet"));
    addBtn->setObjectName("accountsListAddBtn");
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, &WalletsPage::addWalletRequested);
    layout->addWidget(addBtn);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

QPushButton* WalletsPage::buildWalletRow(const WalletSummaryRecord& w, bool isDerived) {
    QPushButton* row = new QPushButton();
    row->setObjectName("accountsListRow");
    row->setCursor(Qt::PointingHandCursor);
    bool isActive = (w.address == m_activeAddress);
    row->setProperty("active", isActive);
    row->style()->unpolish(row);
    row->style()->polish(row);
    row->setMinimumHeight(68);

    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(24, 14, 24, 14);
    rowLayout->setSpacing(14);

    // Avatar
    QLabel* avatar = new QLabel();
    avatar->setFixedSize(40, 40);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setAttribute(Qt::WA_TransparentForMouseEvents);

    QString avatarRel = w.avatarPath;
    if (!avatarRel.isEmpty()) {
        QPixmap src(WalletDb::avatarFullPath(avatarRel));
        if (!src.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            int sz = 40;
            QPixmap pm(qRound(sz * dpr), qRound(sz * dpr));
            pm.setDevicePixelRatio(dpr);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addEllipse(0, 0, sz, sz);
            p.setClipPath(clip);
            QPixmap scaled = src.scaled(sz * dpr, sz * dpr, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(dpr);
            int dx = (sz - scaled.width() / dpr) / 2;
            int dy = (sz - scaled.height() / dpr) / 2;
            p.drawPixmap(dx, dy, scaled);
            avatar->setPixmap(pm);
            avatar->setObjectName("accountsTransparentIcon");
        }
    } else {
        QColor color = m_handler.avatarColor(w.address);
        avatar->setStyleSheet(QString("background: %1; color: white; border-radius: 20px;"
                                      "font-size: 15px; font-weight: bold; font-family: %2;")
                                  .arg(color.name(), Theme::fontFamily));
        avatar->setText(m_handler.avatarText(w));
    }
    rowLayout->addWidget(avatar);

    // Name + address column
    QVBoxLayout* textCol = new QVBoxLayout();
    textCol->setSpacing(2);

    QHBoxLayout* nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    QLabel* name = new QLabel(w.label);
    name->setProperty("uiClass", "accountsName");
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameRow->addWidget(name);

    QString badgeText = isDerived ? tr("Derived") : m_handler.typeBadge(w);
    QLabel* badge = new QLabel(badgeText);
    badge->setObjectName("accountsTypeBadge");
    badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameRow->addWidget(badge);
    nameRow->addStretch();
    textCol->addLayout(nameRow);

    QLabel* addrLabel = new QLabel(w.address);
    addrLabel->setProperty("uiClass", "accountsAddress");
    addrLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    textCol->addWidget(addrLabel);

    rowLayout->addLayout(textCol, 1);

    // Active indicator
    if (isActive) {
        qreal dpr = qApp->devicePixelRatio();
        QLabel* indicator = new QLabel();
        indicator->setFixedSize(16, 16);
        indicator->setAttribute(Qt::WA_TransparentForMouseEvents);
        indicator->setObjectName("accountsTransparentIcon");
        QPixmap pm(qRound(16 * dpr), qRound(16 * dpr));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor("#14F195"), 2));
            p.drawEllipse(2, 2, 12, 12);
            p.setBrush(QColor("#14F195"));
            p.setPen(Qt::NoPen);
            p.drawEllipse(5, 5, 6, 6);
        }
        indicator->setPixmap(pm);
        rowLayout->addWidget(indicator, 0, Qt::AlignVCenter);
    }

    // Chevron
    {
        qreal dpr = qApp->devicePixelRatio();
        QLabel* chevron = new QLabel();
        chevron->setFixedSize(16, 16);
        chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
        chevron->setObjectName("accountsTransparentIcon");
        QPixmap cpm(qRound(16 * dpr), qRound(16 * dpr));
        cpm.setDevicePixelRatio(dpr);
        cpm.fill(Qt::transparent);
        {
            QPainter p(&cpm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(
                QPen(QColor(255, 255, 255, 153), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(5, 2, 11, 8);
            p.drawLine(11, 8, 5, 14);
        }
        chevron->setPixmap(cpm);
        rowLayout->addWidget(chevron, 0, Qt::AlignVCenter);
    }

    connect(row, &QPushButton::clicked, this, [this, w]() { showWalletDetail(w); });
    return row;
}

void WalletsPage::rebuildWalletList() {
    // Clear existing rows
    while (m_listLayout->count() > 0) {
        QLayoutItem* item = m_listLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QList<WalletSummaryRecord> wallets = m_handler.listWallets();
    m_title->setText(tr("Wallets (%1)").arg(wallets.size()));

    QList<WalletGroup> groups = groupWallets(wallets);

    // Container card
    QWidget* card = new QWidget();
    card->setObjectName("accountsListCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 10, 0, 10);
    cardLayout->setSpacing(0);

    for (int g = 0; g < groups.size(); ++g) {
        const WalletGroup& group = groups[g];

        // Parent row
        cardLayout->addWidget(buildWalletRow(group.parent, false));

        // Child rows with tree connectors
        for (int c = 0; c < group.children.size(); ++c) {
            const WalletSummaryRecord& child = group.children[c];
            bool isLastChild = (c == group.children.size() - 1);

            QWidget* childContainer = new QWidget();
            childContainer->setAttribute(Qt::WA_TransparentForMouseEvents, false);
            QHBoxLayout* childLayout = new QHBoxLayout(childContainer);
            childLayout->setContentsMargins(24, 0, 0, 0);
            childLayout->setSpacing(0);

            childLayout->addWidget(new TreeConnector(isLastChild));
            childLayout->addWidget(buildWalletRow(child, true), 1);

            cardLayout->addWidget(childContainer);
        }

        // Per-group "+ Add Account" if parent has a stored seed
        if (WalletDb::getSeedBlobRecord(group.parent.id).has_value()) {
            QPushButton* addAccBtn = new QPushButton("+ " + tr("Add Account"));
            addAccBtn->setCursor(Qt::PointingHandCursor);
            addAccBtn->setProperty("uiClass", "accountsAddAccountBtn");
            int parentId = group.parent.id;
            connect(addAccBtn, &QPushButton::clicked, this,
                    [this, parentId]() { emit addAccountRequested(parentId); });
            cardLayout->addWidget(addAccBtn);
        }

        // Separator between groups (not after last)
        if (g < groups.size() - 1) {
            QFrame* sep = new QFrame();
            sep->setObjectName("accountsListSeparator");
            sep->setProperty("inset", true);
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Plain);
            sep->setFixedHeight(1);
            cardLayout->addWidget(sep);
        }
    }

    m_listLayout->addWidget(card);
}

// ── Detail View ───────────────────────────────────────────────────

QWidget* WalletsPage::buildDetailView() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setObjectName("accountsContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        hidePrivateKeyReveal();
        hideRecoveryPhraseReveal();
        showStep(Step::List);
    });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // ── Avatar + name header ──────────────────────────────────
    layout->addSpacing(8);

    m_detailAvatar = new UploadWidget(UploadWidget::Circle, 64, this);
    connect(m_detailAvatar, &UploadWidget::imageSelected, this, &WalletsPage::saveAvatar);
    layout->addWidget(m_detailAvatar, 0, Qt::AlignHCenter);

    m_detailName = new QLabel();
    m_detailName->setObjectName("accountsDetailName");
    m_detailName->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_detailName);

    m_detailType = new QLabel();
    m_detailType->setObjectName("accountsDetailType");
    m_detailType->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_detailType);

    layout->addSpacing(16);

    // ── Profile section ───────────────────────────────────────
    QLabel* profileHeader = new QLabel(tr("PROFILE"));
    profileHeader->setObjectName("accountsSectionTitle");
    layout->addWidget(profileHeader);
    layout->addSpacing(4);

    QWidget* profileCard = new QWidget();
    profileCard->setObjectName("accountsProfileCard");
    profileCard->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(0);

    // Wallet Name row (editable)
    {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Wallet Name"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_nameValue = new QLabel();
        m_nameValue->setProperty("uiClass", "accountsValue");
        h->addWidget(m_nameValue);

        m_nameEdit = new QLineEdit();
        m_nameEdit->setObjectName("accountsInlineEdit");
        m_nameEdit->setFixedWidth(180);
        m_nameEdit->hide();
        connect(m_nameEdit, &QLineEdit::editingFinished, this, &WalletsPage::saveWalletName);
        h->addWidget(m_nameEdit);

        m_nameEditBtn = new ActionIconButton(":/icons/action/edit.png");
        connect(m_nameEditBtn, &QPushButton::clicked, this, &WalletsPage::editWalletName);
        h->addWidget(m_nameEditBtn);

        profileLayout->addWidget(row);
    }
    profileLayout->addWidget(makeSeparator());

    // Status row (active badge or switch button)
    {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Status"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        // Green pill shown for active wallet
        m_statusPill = new QLabel(QString::fromUtf8("\xe2\x97\x8f") + "  " + tr("Active"));
        m_statusPill->setProperty("uiClass", "accountsStatusPill");
        h->addWidget(m_statusPill);

        // Switch button shown for non-active wallets
        m_switchBtn = new QPushButton(tr("SWITCH TO ACTIVE"));
        m_switchBtn->setCursor(Qt::PointingHandCursor);
        m_switchBtn->setProperty("uiClass", "accountsSwitchBtn");
        connect(m_switchBtn, &QPushButton::clicked, this, [this]() {
            QString address = m_selectedWallet.address;
            if (!address.isEmpty()) {
                m_activeAddress = address;
                populateDetail(m_selectedWallet);
                rebuildWalletList();
                emit walletSwitched(address);
            }
        });
        h->addWidget(m_switchBtn);

        profileLayout->addWidget(row);
    }
    profileLayout->addWidget(makeSeparator());

    // Wallet Address row
    {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Wallet Address"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_addressLink = new AddressLink({});
        h->addWidget(m_addressLink, 0, Qt::AlignRight);

        profileLayout->addWidget(row);
    }

    // Derivation Path row (conditionally visible)
    profileLayout->addWidget(makeSeparator());
    {
        m_derivPathRow = new QWidget();
        m_derivPathRow->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(m_derivPathRow);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Derivation Path"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_derivPathValue = new QLabel();
        m_derivPathValue->setProperty("uiClass", "accountsMonoValue");
        h->addWidget(m_derivPathValue);

        profileLayout->addWidget(m_derivPathRow);
    }

    // Wallet Index row (conditionally visible)
    profileLayout->addWidget(makeSeparator());
    {
        m_indexRow = new QWidget();
        m_indexRow->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(m_indexRow);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Wallet Index"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_indexValue = new QLabel();
        m_indexValue->setProperty("uiClass", "accountsValue");
        h->addWidget(m_indexValue);

        profileLayout->addWidget(m_indexRow);
    }

    // Created date row
    profileLayout->addWidget(makeSeparator());
    {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Created"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_createdValue = new QLabel();
        m_createdValue->setProperty("uiClass", "accountsValue");
        h->addWidget(m_createdValue);

        profileLayout->addWidget(row);
    }

    layout->addWidget(profileCard);

    // ── Security section ──────────────────────────────────────
    layout->addSpacing(8);

    m_privateKeySection = new QWidget();
    QVBoxLayout* secLayout = new QVBoxLayout(m_privateKeySection);
    secLayout->setContentsMargins(0, 0, 0, 0);
    secLayout->setSpacing(4);

    QLabel* secHeader = new QLabel(tr("SECURITY"));
    secHeader->setObjectName("accountsSectionTitle");
    secLayout->addWidget(secHeader);
    secLayout->addSpacing(4);

    // ── Recovery Phrase card (mnemonic wallets only) ────────
    m_recoveryCard = new QWidget();
    m_recoveryCard->setObjectName("accountsRecoveryCard");
    m_recoveryCard->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* recCardLayout = new QVBoxLayout(m_recoveryCard);
    recCardLayout->setContentsMargins(0, 0, 0, 0);
    recCardLayout->setSpacing(0);

    // Recovery Phrase → Reveal row
    {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Recovery Phrase"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_recoveryRevealBtn =
            new QPushButton(QString("%1  %2").arg(tr("Reveal"), QString::fromUtf8("\xe2\x80\xba")));
        m_recoveryRevealBtn->setCursor(Qt::PointingHandCursor);
        m_recoveryRevealBtn->setObjectName("accountsRevealLink");
        connect(m_recoveryRevealBtn, &QPushButton::clicked, this,
                &WalletsPage::revealRecoveryPhrase);
        h->addWidget(m_recoveryRevealBtn);

        recCardLayout->addWidget(row);
    }

    // Revealed words container (hidden by default)
    {
        m_recoveryRevealedContainer = new QWidget();
        m_recoveryRevealedContainer->setProperty("uiClass", "accountsRow");
        m_recoveryRevealedContainer->hide();
        QVBoxLayout* revLayout = new QVBoxLayout(m_recoveryRevealedContainer);
        revLayout->setContentsMargins(20, 0, 20, 16);
        revLayout->setSpacing(8);

        QWidget* gridContainer = new QWidget();
        gridContainer->setObjectName("accountsSecretGrid");
        gridContainer->setAttribute(Qt::WA_StyledBackground, true);
        m_wordGrid = new QGridLayout(gridContainer);
        m_wordGrid->setContentsMargins(12, 12, 12, 12);
        m_wordGrid->setSpacing(6);
        revLayout->addWidget(gridContainer);

        QHBoxLayout* revActions = new QHBoxLayout();
        revActions->setSpacing(12);

        m_recoveryTimerLabel = new QLabel(tr("Auto-hides in 30s"));
        m_recoveryTimerLabel->setProperty("uiClass", "accountsTimer");
        revActions->addWidget(m_recoveryTimerLabel);
        revActions->addStretch();

        QPushButton* hideBtn = new QPushButton(tr("Hide"));
        hideBtn->setCursor(Qt::PointingHandCursor);
        hideBtn->setObjectName("accountsHideLink");
        connect(hideBtn, &QPushButton::clicked, this, [this]() {
            m_recoveryRevealedContainer->hide();
            m_recoveryRevealBtn->show();
            m_recoveryRevealTimer->stop();
            m_recoveryCountdownTicker->stop();
        });
        revActions->addWidget(hideBtn);

        revLayout->addLayout(revActions);
        recCardLayout->addWidget(m_recoveryRevealedContainer);
    }

    secLayout->addWidget(m_recoveryCard);
    secLayout->addSpacing(8);

    // ── Private Key card ─────────────────────────────────────
    QWidget* secCard = new QWidget();
    secCard->setObjectName("accountsSecCard");
    secCard->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* secCardLayout = new QVBoxLayout(secCard);
    secCardLayout->setContentsMargins(0, 0, 0, 0);
    secCardLayout->setSpacing(0);

    // Private Key → Reveal row
    {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "accountsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(20, 16, 20, 16);
        h->setSpacing(12);

        QLabel* label = new QLabel(tr("Private Key"));
        label->setProperty("uiClass", "accountsLabel");
        h->addWidget(label);
        h->addStretch();

        m_revealBtn =
            new QPushButton(QString("%1  %2").arg(tr("Reveal"), QString::fromUtf8("\xe2\x80\xba")));
        m_revealBtn->setCursor(Qt::PointingHandCursor);
        m_revealBtn->setObjectName("accountsRevealLink");
        connect(m_revealBtn, &QPushButton::clicked, this, &WalletsPage::revealPrivateKey);
        h->addWidget(m_revealBtn);

        secCardLayout->addWidget(row);
    }

    // Revealed key container (hidden by default)
    {
        m_revealedContainer = new QWidget();
        m_revealedContainer->setProperty("uiClass", "accountsRow");
        m_revealedContainer->hide();
        QVBoxLayout* revLayout = new QVBoxLayout(m_revealedContainer);
        revLayout->setContentsMargins(20, 0, 20, 16);
        revLayout->setSpacing(8);

        m_revealedKeyLabel = new QLabel();
        m_revealedKeyLabel->setObjectName("accountsSecretValue");
        m_revealedKeyLabel->setWordWrap(true);
        m_revealedKeyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        revLayout->addWidget(m_revealedKeyLabel);

        QHBoxLayout* revActions = new QHBoxLayout();
        revActions->setSpacing(12);

        m_timerLabel = new QLabel(tr("Auto-hides in 30s"));
        m_timerLabel->setProperty("uiClass", "accountsTimer");
        revActions->addWidget(m_timerLabel);
        revActions->addStretch();

        QPushButton* copyKeyBtn = new QPushButton(tr("Copy"));
        copyKeyBtn->setCursor(Qt::PointingHandCursor);
        copyKeyBtn->setObjectName("accountsActionLink");
        connect(copyKeyBtn, &QPushButton::clicked, this, [this, copyKeyBtn]() {
            qApp->clipboard()->setText(m_revealedKeyLabel->text());
            copyKeyBtn->setText(tr("Copied!"));
            QTimer::singleShot(1500, copyKeyBtn,
                               [copyKeyBtn, this]() { copyKeyBtn->setText(tr("Copy")); });
        });
        revActions->addWidget(copyKeyBtn);

        QPushButton* hideBtn = new QPushButton(tr("Hide"));
        hideBtn->setCursor(Qt::PointingHandCursor);
        hideBtn->setObjectName("accountsHideLink");
        connect(hideBtn, &QPushButton::clicked, this, [this]() {
            m_revealedContainer->hide();
            m_revealBtn->show();
            m_revealTimer->stop();
            m_countdownTicker->stop();
        });
        revActions->addWidget(hideBtn);

        revLayout->addLayout(revActions);
        secCardLayout->addWidget(m_revealedContainer);
    }

    secLayout->addWidget(secCard);
    layout->addWidget(m_privateKeySection);

    // ── Remove Wallet ─────────────────────────────────────────
    layout->addSpacing(16);

    QPushButton* removeBtn = new QPushButton(tr("Remove Wallet"));
    removeBtn->setObjectName("accountsDangerBtn");
    removeBtn->setCursor(Qt::PointingHandCursor);
    connect(removeBtn, &QPushButton::clicked, this, &WalletsPage::removeWallet);
    layout->addWidget(removeBtn);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

void WalletsPage::showWalletDetail(const WalletSummaryRecord& wallet) {
    m_selectedWallet = m_handler.loadWalletDetail(wallet);
    populateDetail(m_selectedWallet);
    showStep(Step::Detail);
}

void WalletsPage::populateDetail(const WalletRecord& wallet) {
    WalletSummaryRecord summary;
    summary.id = wallet.id;
    summary.label = wallet.label;
    summary.address = wallet.address;
    summary.keyType = wallet.keyType;
    summary.accountIndex = wallet.accountIndex;
    summary.parentWalletId = wallet.parentWalletId;
    summary.createdAt = wallet.createdAt;
    summary.avatarPath = wallet.avatarPath;

    QString address = wallet.address;
    QString label = wallet.label;
    int walletIndex = wallet.accountIndex;
    bool hasIndex = walletIndex >= 0;

    // Avatar — custom image or deterministic colored circle
    QString avatarRel = wallet.avatarPath;
    if (!avatarRel.isEmpty()) {
        QString fullPath = WalletDb::avatarFullPath(avatarRel);
        m_detailAvatar->setImagePath(fullPath);
    } else {
        // Generate deterministic avatar pixmap
        qreal dpr = qApp->devicePixelRatio();
        int sz = 64;
        QPixmap pm(qRound(sz * dpr), qRound(sz * dpr));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            QColor color = m_handler.avatarColor(address);
            p.setBrush(color);
            p.setPen(Qt::NoPen);
            p.drawEllipse(0, 0, sz, sz);
            p.setPen(Qt::white);
            QFont f;
            f.setFamily("Exo 2");
            f.setPixelSize(24);
            f.setWeight(QFont::Bold);
            p.setFont(f);
            p.drawText(QRect(0, 0, sz, sz), Qt::AlignCenter, m_handler.avatarText(summary));
        }
        m_detailAvatar->setPixmap(pm);
    }

    // Name + type
    m_detailName->setText(label);
    m_detailType->setText(m_handler.typeBadge(summary));

    // Profile fields
    m_nameValue->setText(label);
    m_nameValue->show();
    m_nameEdit->hide();

    // Status row — pill or switch button
    bool isActive = (address == m_activeAddress);
    m_statusPill->setVisible(isActive);
    m_switchBtn->setVisible(!isActive);

    m_addressLink->setAddress(address);

    // Derivation path — show for HD-derived and hardware wallets
    const bool showDerivPath = m_handler.showDerivationPath(wallet);
    const QString derivPath = m_handler.derivationPath(wallet);
    m_derivPathRow->setVisible(showDerivPath);
    if (showDerivPath) {
        m_derivPathValue->setText(derivPath);
    }

    // Wallet Index — only for HD-derived
    m_indexRow->setVisible(hasIndex);
    if (hasIndex) {
        m_indexValue->setText(QString("#%1").arg(walletIndex + 1));
    }

    // Created date
    qint64 createdAt = wallet.createdAt;
    if (createdAt > 0) {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(createdAt);
        m_createdValue->setText(dt.toString("MMM d, yyyy"));
    } else {
        m_createdValue->setText("—");
    }

    // Security section — only for software wallets
    bool isSoftware = m_handler.isSoftwareWallet(wallet);
    m_privateKeySection->setVisible(isSoftware);

    // Recovery phrase — only if mnemonic blob exists
    m_recoveryCard->setVisible(m_handler.hasRecoveryPhrase(wallet.id));

    // Reset reveal states
    hidePrivateKeyReveal();
    hideRecoveryPhraseReveal();
}

// ── Edit Wallet Name ──────────────────────────────────────────────

void WalletsPage::editWalletName() {
    m_nameEdit->setText(m_nameValue->text());
    m_nameValue->hide();
    m_nameEditBtn->hide();
    m_nameEdit->show();
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void WalletsPage::saveWalletName() {
    QString newLabel = m_nameEdit->text().trimmed();
    if (newLabel.isEmpty()) {
        newLabel = m_selectedWallet.label;
    }

    int id = m_selectedWallet.id;
    if (m_handler.renameWallet(id, newLabel)) {
        m_selectedWallet.label = newLabel;
        m_nameValue->setText(newLabel);
        m_detailName->setText(newLabel);
        rebuildWalletList();
        emit walletRenamed();
    }

    m_nameEdit->hide();
    m_nameValue->show();
    m_nameEditBtn->show();
}

// ── Save Avatar ──────────────────────────────────────────────────

void WalletsPage::saveAvatar(const QString& path) {
    QString address = m_selectedWallet.address;
    int walletId = m_selectedWallet.id;
    if (address.isEmpty() || walletId <= 0) {
        return;
    }

    QString relName;
    if (!m_handler.saveAvatar(walletId, address, path, relName)) {
        return;
    }
    m_selectedWallet.avatarPath = relName;

    // Refresh the list view to show the new avatar there too
    rebuildWalletList();
    emit avatarChanged();
}

// ── Reveal Private Key ────────────────────────────────────────────

void WalletsPage::revealPrivateKey() {
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Confirm Password"));
    dialog->setFixedSize(420, 220);
    dialog->setObjectName("accountsDialog");

    auto* dlgLayout = new QVBoxLayout(dialog);
    dlgLayout->setContentsMargins(28, 24, 28, 24);
    dlgLayout->setSpacing(16);

    auto* title = new QLabel(tr("Enter your password to reveal the private key."));
    title->setObjectName("accountsDialogDesc");
    title->setWordWrap(true);
    dlgLayout->addWidget(title);

    auto* pwInput = new QLineEdit();
    pwInput->setEchoMode(QLineEdit::Password);
    pwInput->setPlaceholderText(tr("Password"));
    pwInput->setObjectName("accountsDialogInput");
    dlgLayout->addWidget(pwInput);

    auto* errLabel = new QLabel();
    errLabel->setObjectName("accountsDialogError");
    errLabel->setVisible(false);
    dlgLayout->addWidget(errLabel);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setObjectName("accountsDialogCancel");
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto* confirmBtn = new QPushButton(tr("Confirm"));
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setObjectName("accountsDialogConfirm");
    btnRow->addWidget(confirmBtn);
    dlgLayout->addLayout(btnRow);

    connect(confirmBtn, &QPushButton::clicked, this, [this, pwInput, errLabel, dialog]() {
        QString error;
        auto base58 = m_handler.revealPrivateKey(m_selectedWallet.address, pwInput->text(), error);
        if (!base58.has_value()) {
            errLabel->setText(tr("%1").arg(error));
            errLabel->setVisible(true);
            return;
        }

        m_revealedKeyLabel->setText(*base58);
        m_revealBtn->hide();
        m_revealedContainer->show();
        m_revealTimer->start(30000);
        m_countdownSecs = 30;
        m_timerLabel->setText(tr("Auto-hides in 30s"));
        m_countdownTicker->start();

        dialog->accept();
    });

    connect(pwInput, &QLineEdit::returnPressed, confirmBtn, &QPushButton::click);
    dialog->exec();
    dialog->deleteLater();
}

// ── Reveal Recovery Phrase ──────────────────────────────────────

void WalletsPage::revealRecoveryPhrase() {
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Confirm Password"));
    dialog->setFixedSize(420, 220);
    dialog->setObjectName("accountsDialog");

    auto* dlgLayout = new QVBoxLayout(dialog);
    dlgLayout->setContentsMargins(28, 24, 28, 24);
    dlgLayout->setSpacing(16);

    auto* title = new QLabel(tr("Enter your password to reveal the recovery phrase."));
    title->setObjectName("accountsDialogDesc");
    title->setWordWrap(true);
    dlgLayout->addWidget(title);

    auto* pwInput = new QLineEdit();
    pwInput->setEchoMode(QLineEdit::Password);
    pwInput->setPlaceholderText(tr("Password"));
    pwInput->setObjectName("accountsDialogInput");
    dlgLayout->addWidget(pwInput);

    auto* errLabel = new QLabel();
    errLabel->setObjectName("accountsDialogError");
    errLabel->setVisible(false);
    dlgLayout->addWidget(errLabel);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setObjectName("accountsDialogCancel");
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto* confirmBtn = new QPushButton(tr("Confirm"));
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setObjectName("accountsDialogConfirm");
    btnRow->addWidget(confirmBtn);
    dlgLayout->addLayout(btnRow);

    connect(confirmBtn, &QPushButton::clicked, this, [this, pwInput, errLabel, dialog]() {
        QString error;
        auto wordsResult =
            m_handler.revealRecoveryPhrase(m_selectedWallet.id, pwInput->text(), error);
        if (!wordsResult.has_value()) {
            errLabel->setText(tr("%1").arg(error));
            errLabel->setVisible(true);
            return;
        }
        const QStringList& words = *wordsResult;

        // Clear existing grid
        while (m_wordGrid->count() > 0) {
            QLayoutItem* item = m_wordGrid->takeAt(0);
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }

        // Fill grid: 3 columns, words flow down columns
        int wordCount = words.size();
        int rows = (wordCount + 2) / 3;
        for (int i = 0; i < wordCount; ++i) {
            int col = i / rows;
            int row = i % rows;
            QLabel* wordLabel = new QLabel(QString("%1. %2").arg(i + 1).arg(words[i]));
            wordLabel->setObjectName("accountsRecoveryWord");
            m_wordGrid->addWidget(wordLabel, row, col);
        }

        m_recoveryRevealBtn->hide();
        m_recoveryRevealedContainer->show();
        m_recoveryRevealTimer->start(30000);
        m_recoveryCountdownSecs = 30;
        m_recoveryTimerLabel->setText(tr("Auto-hides in 30s"));
        m_recoveryCountdownTicker->start();

        dialog->accept();
    });

    connect(pwInput, &QLineEdit::returnPressed, confirmBtn, &QPushButton::click);
    dialog->exec();
    dialog->deleteLater();
}

// ── Remove Wallet ─────────────────────────────────────────────

void WalletsPage::removeWallet() {
    QString walletLabel = m_selectedWallet.label;

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Remove Wallet"));
    dialog->setFixedSize(420, 260);
    dialog->setObjectName("accountsDialog");

    auto* dlgLayout = new QVBoxLayout(dialog);
    dlgLayout->setContentsMargins(28, 24, 28, 24);
    dlgLayout->setSpacing(16);

    auto* title = new QLabel(tr("Are you sure you want to remove this wallet?"));
    title->setObjectName("accountsDialogTitle");
    title->setWordWrap(true);
    dlgLayout->addWidget(title);

    auto* desc = new QLabel(tr("Type \"%1\" to confirm. This cannot be undone.").arg(walletLabel));
    desc->setObjectName("accountsDialogDesc");
    desc->setWordWrap(true);
    dlgLayout->addWidget(desc);

    auto* confirmInput = new QLineEdit();
    confirmInput->setPlaceholderText(walletLabel);
    confirmInput->setObjectName("accountsDialogInput");
    dlgLayout->addWidget(confirmInput);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setObjectName("accountsDialogCancel");
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto* removeBtn = new QPushButton(tr("Remove"));
    removeBtn->setCursor(Qt::PointingHandCursor);
    removeBtn->setEnabled(false);
    removeBtn->setObjectName("accountsDialogRemove");
    btnRow->addWidget(removeBtn);
    dlgLayout->addLayout(btnRow);

    connect(confirmInput, &QLineEdit::textChanged, this,
            [removeBtn, walletLabel](const QString& text) {
                removeBtn->setEnabled(text == walletLabel);
            });

    connect(removeBtn, &QPushButton::clicked, this, [this, dialog]() {
        int id = m_selectedWallet.id;
        if (m_handler.removeWallet(id)) {
            QString addr = m_selectedWallet.address;
            dialog->accept();
            showStep(Step::List);
            refresh();
            emit walletRemoved(addr);
        }
    });

    dialog->exec();
    dialog->deleteLater();
}

// ── Helpers ───────────────────────────────────────────────────────

QWidget* WalletsPage::makeSeparator() {
    QFrame* sep = new QFrame();
    sep->setObjectName("accountsSeparator");
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedHeight(1);
    sep->setContentsMargins(20, 0, 20, 0);
    return sep;
}
