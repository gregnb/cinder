#include "AssetsPage.h"
#include "Theme.h"
#include "features/assets/AssetsHandler.h"
#include "services/AvatarCache.h"
#include "widgets/AddressLink.h"
#include "widgets/Dropdown.h"
#include "widgets/QrCodeWidget.h"
#include "widgets/SplineChart.h"
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QToolTip>
#include <QVBoxLayout>

// Format a USD value with $ prefix and 2 decimal places, with commas
static QString formatUsd(double value) {
    if (value >= 1.0) {
        return "$" + QLocale(QLocale::English).toString(value, 'f', 2);
    } else if (value >= 0.01) {
        return "$" + QString::number(value, 'f', 2);
    } else if (value > 0) {
        return "$" + QString::number(value, 'g', 4);
    }
    return "$0.00";
}

// Format a token balance with appropriate precision
static QString formatBalance(double balance, const QString& symbol) {
    QString num;
    if (balance >= 1000.0) {
        num = QLocale(QLocale::English).toString(balance, 'f', 2);
    } else if (balance >= 1.0) {
        num = QString::number(balance, 'f', 4);
    } else if (balance > 0) {
        num = QString::number(balance, 'g', 6);
    } else {
        num = "0";
    }
    return num + " " + symbol;
}

AssetsPage::AssetsPage(QWidget* parent) : QWidget(parent), m_handler(new AssetsHandler(this)) {
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();

    // ── Index 0: Assets list ────────────────────────────────────
    m_scroll = new QScrollArea();
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setObjectName("assetsContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(24);

    // ── Header row: title + search ───────────────────────────
    QHBoxLayout* headerRow = new QHBoxLayout();

    m_title = new QLabel(tr("Assets (0)"));
    m_title->setObjectName("pageTitle");
    headerRow->addWidget(m_title);
    headerRow->addStretch();

    QLineEdit* search = new QLineEdit();
    search->setObjectName("abSearchInput");
    search->setPlaceholderText(tr("Search..."));
    search->setFixedWidth(220);
    search->setMinimumHeight(36);
    QPalette pal = search->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    search->setPalette(pal);
    headerRow->addWidget(search);

    layout->addLayout(headerRow);

    // ── Portfolio summary card ───────────────────────────────
    layout->addWidget(createPortfolioCard());

    // ── Sort dropdown ─────────────────────────────────────────
    QHBoxLayout* sortRow = new QHBoxLayout();
    sortRow->addStretch();
    m_sortDropdown = new Dropdown();
    for (const AssetsSortOption option : m_handler->sortOptions()) {
        m_sortDropdown->addItem(m_handler->labelForSortOption(option));
    }
    m_sortDropdown->setCurrentItem(m_handler->labelForSortOption(AssetsSortOption::ValueHighToLow));
    m_sortDropdown->setFixedWidth(200);
    connect(m_sortDropdown, &Dropdown::itemSelected, this, [this](const QString& option) {
        const AssetsSortOption sortOption = m_handler->sortOptionFromLabel(option);
        m_handler->sortAssets(m_assets, sortOption);
        rebuildFilteredList();
    });
    sortRow->addWidget(m_sortDropdown);
    layout->addLayout(sortRow);

    // ── Virtual grid container (absolute positioning) ─────────
    m_gridContainer = new QWidget();
    m_gridContainer->setMinimumHeight(0);
    layout->addWidget(m_gridContainer);

    // ── Empty state (hidden by default) ──────────────────────
    m_emptyState = new QWidget();
    m_emptyState->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(16);
    emptyLayout->addStretch(1);

    QPixmap treasurePixmap(":/icons/ui/treasure-empty.png");
    if (!treasurePixmap.isNull()) {
        qreal dpr = qApp->devicePixelRatio();
        treasurePixmap =
            treasurePixmap.scaled(static_cast<int>(180 * dpr), static_cast<int>(180 * dpr),
                                  Qt::KeepAspectRatio, Qt::SmoothTransformation);
        treasurePixmap.setDevicePixelRatio(dpr);
    }
    QLabel* treasureIcon = new QLabel();
    treasureIcon->setFixedSize(220, 220);
    treasureIcon->setPixmap(treasurePixmap);
    treasureIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(treasureIcon, 0, Qt::AlignHCenter);

    QLabel* emptyText = new QLabel(tr("No assets found"));
    emptyText->setObjectName("assetsEmptyText");
    emptyText->setMinimumWidth(260);
    emptyText->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    emptyLayout->addWidget(emptyText, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);

    m_emptyState->setMinimumHeight(0);
    m_emptyState->hide();
    layout->addWidget(m_emptyState, 1);

    // Absorb remaining vertical space so items stay top-aligned
    // when setWidgetResizable(true) expands content to fill viewport.
    layout->addStretch(1);

    m_scroll->setWidget(content);

    // ── Create card pool ──────────────────────────────────────
    for (int i = 0; i < POOL_SIZE; ++i) {
        QWidget* card = createPoolCard();
        card->setParent(m_gridContainer);
        card->setVisible(false);
        m_cardPool.append(card);
    }

    // ── Connect scroll events ─────────────────────────────────
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &AssetsPage::relayoutVisibleCards);
    m_scroll->viewport()->installEventFilter(this);

    m_stack->addWidget(m_scroll);           // Step::AssetList
    m_stack->addWidget(buildReceivePage()); // Step::Receive

    outerLayout->addWidget(m_stack);

    m_searchInput = search;

    // ── Wire search ──────────────────────────────────────────
    connect(search, &QLineEdit::textChanged, this, &AssetsPage::filterAssets);
}

void AssetsPage::showStep(Step step) { m_stack->setCurrentIndex(static_cast<int>(step)); }

// ── Receive Page ──────────────────────────────────────────────────

QWidget* AssetsPage::buildReceivePage() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setObjectName("assetsContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::AssetList); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Receive"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Description
    QLabel* desc =
        new QLabel(tr("Share your wallet address or QR code to receive SOL and tokens."));
    desc->setObjectName("assetsReceiveDesc");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(16);

    // ── QR code ─────────────────────────────────────────────────
    m_qrCode = new QrCodeWidget();
    m_qrCode->setFixedSize(220, 220);
    m_qrCode->setModuleColor(QColor(18, 19, 31));
    m_qrCode->setBackgroundColor(QColor(255, 255, 255));
    layout->addWidget(m_qrCode, 0, Qt::AlignHCenter);

    layout->addSpacing(20);

    // ── Address label ───────────────────────────────────────────
    QLabel* addrTitle = new QLabel(tr("Your Wallet Address"));
    addrTitle->setObjectName("assetsAddrTitle");
    addrTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(addrTitle);

    layout->addSpacing(4);

    // Address with copy button in a styled container
    QWidget* addrContainer = new QWidget();
    addrContainer->setObjectName("receiveAddrBox");
    addrContainer->setMaximumWidth(500);

    QHBoxLayout* addrRow = new QHBoxLayout(addrContainer);
    addrRow->setContentsMargins(16, 14, 12, 14);
    addrRow->setSpacing(10);

    m_receiveAddress = new AddressLink({});
    addrRow->addWidget(m_receiveAddress, 1);

    layout->addWidget(addrContainer, 0, Qt::AlignHCenter);

    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

void AssetsPage::showReceivePage() {
    if (!m_ownerAddress.isEmpty()) {
        m_qrCode->setData(m_ownerAddress);
        m_receiveAddress->setAddress(m_ownerAddress);
    }
    showStep(Step::Receive);
}

// ── Refresh from DB ──────────────────────────────────────────────

void AssetsPage::refresh(const QString& ownerAddress) {
    const bool ownerChanged = (m_ownerAddress != ownerAddress);
    m_ownerAddress = ownerAddress;
    const AssetsViewData data = m_handler->loadAssets(ownerAddress);
    m_assets = data.assets;

    // Update portfolio card
    if (m_portfolioAmount) {
        m_portfolioAmount->setText(formatUsd(data.totalPortfolioValue) + " USD");
    }

    // Hide chart when balance is zero — a flat $0 line adds no value
    if (m_chart) {
        m_chart->setVisible(data.totalPortfolioValue > 0);
        m_chart->setData(data.chartPoints);
    }

    // Sort using current dropdown selection, then rebuild virtual grid
    const AssetsSortOption currentSort =
        m_handler->sortOptionFromLabel(m_sortDropdown->currentText());
    m_handler->sortAssets(m_assets, currentSort);
    rebuildFilteredList();

    // Reset stale scroll offsets when switching wallets; otherwise virtual rows
    // can start below row 0 for small datasets.
    if (ownerChanged) {
        m_scroll->verticalScrollBar()->setValue(0);
        relayoutVisibleCards();
    }
}

// ── Virtual scrolling ───────────────────────────────────────────

void AssetsPage::rebuildFilteredList() {
    // Return all active cards to the pool
    for (auto it = m_activeCards.begin(); it != m_activeCards.end(); ++it) {
        it.value()->setVisible(false);
    }
    m_activeCards.clear();

    // Rebuild filtered index list
    m_filteredIndices.clear();
    for (int i = 0; i < m_assets.size(); ++i) {
        if (m_currentFilter.isEmpty() ||
            m_assets[i].symbol.contains(m_currentFilter, Qt::CaseInsensitive) ||
            m_assets[i].name.contains(m_currentFilter, Qt::CaseInsensitive)) {
            m_filteredIndices.append(i);
        }
    }

    // Resize container to hold all rows
    int rowCount = (m_filteredIndices.size() + GRID_COLS - 1) / GRID_COLS;
    int totalH = rowCount > 0 ? rowCount * CARD_H + (rowCount - 1) * GRID_SPACING : 0;
    m_gridContainer->setMinimumHeight(totalH);
    m_gridContainer->setMaximumHeight(totalH);

    // Update empty state and title
    const bool isEmpty = m_filteredIndices.isEmpty();
    m_gridContainer->setVisible(!isEmpty);
    m_emptyState->setVisible(isEmpty);
    m_title->setText(tr("Assets (%1)").arg(m_filteredIndices.size()));

    // Force the layout to compute geometry now so m_gridContainer has a
    // valid width before we try to position cards.
    if (auto* lay = m_scroll->widget()->layout()) {
        lay->activate();
    }

    // Clamp scroll in case dataset shrank (prevents virtual row offset gaps).
    QScrollBar* sb = m_scroll->verticalScrollBar();
    if (rowCount <= 1) {
        // Single-row datasets must start at row 0; stale scroll offsets from a
        // previous larger dataset can otherwise push the only card down.
        sb->setValue(0);
    } else {
        sb->setValue(qMin(sb->value(), sb->maximum()));
    }

    relayoutVisibleCards();
}

void AssetsPage::relayoutVisibleCards() {
    int filteredCount = m_filteredIndices.size();
    if (filteredCount == 0) {
        for (auto it = m_activeCards.begin(); it != m_activeCards.end(); ++it) {
            it.value()->setVisible(false);
        }
        m_activeCards.clear();
        return;
    }

    int containerW = m_gridContainer->width();
    if (containerW <= 0) {
        return;
    }

    // Small lists don't need virtualization. Lay out deterministically from row 0
    // to avoid stale offset artifacts after dataset/owner switches.
    if (filteredCount <= POOL_SIZE) {
        for (auto it = m_activeCards.begin(); it != m_activeCards.end(); ++it) {
            it.value()->setVisible(false);
        }
        m_activeCards.clear();

        int cardW = (containerW - (GRID_COLS - 1) * GRID_SPACING) / GRID_COLS;
        for (int i = 0; i < filteredCount; ++i) {
            QWidget* card = m_cardPool[i];
            int row = i / GRID_COLS;
            int col = i % GRID_COLS;
            int x = col * (cardW + GRID_SPACING);
            int y = row * (CARD_H + GRID_SPACING);
            bindCard(card, i);
            card->move(x, y);
            card->resize(cardW, CARD_H);
            m_activeCards[i] = card;
        }
        return;
    }

    int totalRows = (filteredCount + GRID_COLS - 1) / GRID_COLS;
    int rowPitch = CARD_H + GRID_SPACING;

    // Get scroll position and viewport height
    int scrollY = m_scroll->verticalScrollBar()->value();
    int viewportH = m_scroll->viewport()->height();

    // Grid container's Y offset within the scroll content
    int containerTop = m_gridContainer->mapTo(m_scroll->widget(), QPoint(0, 0)).y();
    int gridViewTop = scrollY - containerTop;
    int gridViewBottom = gridViewTop + viewportH;

    // If viewport doesn't overlap the grid, hide everything
    if (gridViewBottom < 0 || gridViewTop > totalRows * rowPitch) {
        for (auto it = m_activeCards.begin(); it != m_activeCards.end(); ++it) {
            it.value()->setVisible(false);
        }
        m_activeCards.clear();
        return;
    }

    // Calculate buffered row range
    int firstVisibleRow = qMax(0, gridViewTop / rowPitch);
    int lastVisibleRow = qMin(totalRows - 1, gridViewBottom / rowPitch);
    int firstBufferedRow = qMax(0, firstVisibleRow - BUFFER_ROWS);
    int lastBufferedRow = qMin(totalRows - 1, lastVisibleRow + BUFFER_ROWS);

    // Convert to flat filtered indices
    int firstIdx = firstBufferedRow * GRID_COLS;
    int lastIdx = qMin(filteredCount - 1, (lastBufferedRow + 1) * GRID_COLS - 1);

    // Recycle cards outside the visible range
    QList<int> toRemove;
    for (auto it = m_activeCards.begin(); it != m_activeCards.end(); ++it) {
        if (it.key() < firstIdx || it.key() > lastIdx) {
            it.value()->setVisible(false);
            toRemove.append(it.key());
        }
    }
    for (int idx : toRemove) {
        m_activeCards.remove(idx);
    }

    // Build free pool
    QSet<QWidget*> usedCards;
    for (auto it = m_activeCards.constBegin(); it != m_activeCards.constEnd(); ++it) {
        usedCards.insert(it.value());
    }
    QList<QWidget*> freeCards;
    for (QWidget* c : std::as_const(m_cardPool)) {
        if (!usedCards.contains(c)) {
            freeCards.append(c);
        }
    }

    // Calculate card width
    int cardW = (containerW - (GRID_COLS - 1) * GRID_SPACING) / GRID_COLS;

    int freeIdx = 0;
    for (int i = firstIdx; i <= lastIdx; ++i) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;
        int x = col * (cardW + GRID_SPACING);
        int y = row * rowPitch;

        if (m_activeCards.contains(i)) {
            // Already bound — just update position/size in case of resize
            QWidget* card = m_activeCards[i];
            card->move(x, y);
            card->resize(cardW, CARD_H);
            continue;
        }

        if (freeIdx >= freeCards.size()) {
            break; // pool exhausted
        }

        QWidget* card = freeCards[freeIdx++];
        bindCard(card, i);
        card->move(x, y);
        card->resize(cardW, CARD_H);
        m_activeCards[i] = card;
    }
}

void AssetsPage::filterAssets(const QString& text) {
    m_currentFilter = text;
    rebuildFilteredList();
}

// ── Avatar cache + async icon updates ─────────────────────────────

void AssetsPage::setAvatarCache(AvatarCache* cache) {
    m_avatarCache = cache;
    if (m_avatarCache) {
        connect(m_avatarCache, &AvatarCache::avatarReady, this, [this](const QString& url) {
            for (auto it = m_activeCards.begin(); it != m_activeCards.end(); ++it) {
                int filteredIdx = it.key();
                if (filteredIdx < m_filteredIndices.size()) {
                    const AssetInfo& asset = m_assets[m_filteredIndices[filteredIdx]];
                    if (asset.logoUrl == url) {
                        bindCard(it.value(), filteredIdx);
                    }
                }
            }
        });
    }
}

QWidget* AssetsPage::createPortfolioCard() {
    QWidget* card = new QWidget();
    card->setObjectName("balanceCard");

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(30);
    shadow->setColor(Theme::shadowColor);
    shadow->setOffset(0, 8);
    card->setGraphicsEffect(shadow);

    card->setFixedHeight(160);

    QHBoxLayout* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(30, 25, 0, 25);

    // Left side — balance info
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    QLabel* label = new QLabel(tr("Total Portfolio Value"));
    label->setObjectName("balanceLabel");
    infoLayout->addWidget(label);

    m_portfolioAmount = new QLabel("$0.00 USD");
    m_portfolioAmount->setObjectName("portfolioAmount");
    infoLayout->addWidget(m_portfolioAmount);

    infoLayout->addStretch();
    cardLayout->addLayout(infoLayout);
    cardLayout->addStretch();

    // Right side — portfolio value chart with axes
    m_chart = new SplineChart();
    m_chart->setFixedSize(420, 130);
    m_chart->setLineColor(QColor(20, 241, 149));
    m_chart->setGlowColor(QColor(20, 241, 149, 150));
    m_chart->setShowYAxis(true);
    m_chart->setShowXAxis(true);
    m_chart->setYLabelFormatter([](double v) -> QString {
        if (v >= 1e6) {
            return "$" + QString::number(v / 1e6, 'f', 1) + "M";
        }
        if (v >= 1e3) {
            return "$" + QString::number(v / 1e3, 'f', 0) + "K";
        }
        if (v <= -1e6) {
            return "-$" + QString::number(-v / 1e6, 'f', 1) + "M";
        }
        if (v <= -1e3) {
            return "-$" + QString::number(-v / 1e3, 'f', 0) + "K";
        }
        if (qAbs(v) < 10) {
            return "$" + QString::number(v, 'f', 2);
        }
        return "$" + QString::number(v, 'f', 0);
    });
    m_chart->setValueFormatter(
        [](double, double y) { return "$" + QLocale(QLocale::English).toString(y, 'f', 2); });
    m_chart->setFillGradient({
        {0.0, QColor(20, 241, 149, 5)},
        {0.5, QColor(20, 241, 149, 30)},
        {1.0, QColor(20, 241, 149, 60)},
    });
    cardLayout->addWidget(m_chart);

    return card;
}

// ── Pool card creation (layout skeleton, no data) ─────────────────

QWidget* AssetsPage::createPoolCard() {
    QWidget* card = new QWidget();
    card->setObjectName("assetCard");
    card->setProperty("hovered", false);
    card->setAttribute(Qt::WA_StyledBackground, true);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(25);
    shadow->setColor(Theme::shadowColorLight);
    shadow->setOffset(0, 6);
    card->setGraphicsEffect(shadow);
    card->setCursor(Qt::PointingHandCursor);
    card->installEventFilter(this);

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(0);

    // ── Top row: icon + name ──────────────────────────────────
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    QLabel* icon = new QLabel();
    icon->setObjectName("cardIcon");
    icon->setFixedSize(32, 32);
    topRow->addWidget(icon);

    QLabel* name = new QLabel();
    name->setObjectName("assetCardName");
    name->setMinimumWidth(0);
    name->setWordWrap(true);
    name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    topRow->addWidget(name, 1);

    layout->addLayout(topRow);

    // ── Total value (prominent) ──────────────────────────────
    layout->addSpacing(12);
    QLabel* totalVal = new QLabel();
    totalVal->setObjectName("assetCardPrice");
    layout->addWidget(totalVal);

    // ── Separator ─────────────────────────────────────────────
    layout->addSpacing(12);
    QFrame* sep = new QFrame();
    sep->setObjectName("assetCardSeparator");
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setLineWidth(1);
    sep->setMidLineWidth(0);
    sep->setFixedHeight(1);
    layout->addWidget(sep);
    layout->addSpacing(12);

    // ── Stats row: holding + price per unit ───────────────────
    QHBoxLayout* statsRow = new QHBoxLayout();

    QVBoxLayout* holdCol = new QVBoxLayout();
    holdCol->setSpacing(2);
    QLabel* holdLabel = new QLabel(tr("Total Holding"));
    holdLabel->setObjectName("assetCardStatLabel");
    holdCol->addWidget(holdLabel);
    QLabel* holdValue = new QLabel();
    holdValue->setObjectName("cardHoldValue");
    holdValue->setMinimumWidth(0);
    holdCol->addWidget(holdValue);
    statsRow->addLayout(holdCol);

    statsRow->addStretch();

    QVBoxLayout* priceCol = new QVBoxLayout();
    priceCol->setSpacing(2);
    QLabel* priceLabel = new QLabel(tr("Price"));
    priceLabel->setObjectName("assetCardStatLabel");
    priceLabel->setAlignment(Qt::AlignRight);
    priceCol->addWidget(priceLabel);
    QLabel* priceValue = new QLabel();
    priceValue->setObjectName("cardPriceValue");
    priceValue->setAlignment(Qt::AlignRight);
    priceCol->addWidget(priceValue);
    statsRow->addLayout(priceCol);

    layout->addLayout(statsRow);

    // ── Button row: Send + Receive ────────────────────────────
    layout->addSpacing(14);
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    QPushButton* sendBtn = new QPushButton(tr("Send"));
    sendBtn->setObjectName("assetSendBtn");
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setMinimumHeight(36);
    btnRow->addWidget(sendBtn, 1);

    QPushButton* recvBtn = new QPushButton(tr("Receive"));
    recvBtn->setObjectName("assetReceiveBtn");
    recvBtn->setCursor(Qt::PointingHandCursor);
    recvBtn->setMinimumHeight(36);
    connect(recvBtn, &QPushButton::clicked, this, &AssetsPage::showReceivePage);
    btnRow->addWidget(recvBtn, 1);

    layout->addLayout(btnRow);

    return card;
}

// ── Bind data to a pool card ──────────────────────────────────────

void AssetsPage::bindCard(QWidget* card, int filteredIndex) {
    const AssetInfo& asset = m_assets[m_filteredIndices[filteredIndex]];

    card->setProperty("mint", asset.mint);

    // ── Icon ──────────────────────────────────────────────────
    QLabel* icon = card->findChild<QLabel*>("cardIcon");
    QPixmap iconPixmap;

    if (!asset.iconPath.isEmpty()) {
        iconPixmap = QPixmap(asset.iconPath);
    }
    if (iconPixmap.isNull() && m_avatarCache && !asset.logoUrl.isEmpty()) {
        iconPixmap = m_avatarCache->get(asset.logoUrl);
    }

    if (!iconPixmap.isNull()) {
        qreal dpr = qApp->devicePixelRatio();
        iconPixmap = AvatarCache::roundedRectClip(iconPixmap, 32, dpr);
        icon->setPixmap(iconPixmap);
        icon->setText({});
        icon->setProperty("fallback", false);
        icon->style()->unpolish(icon);
        icon->style()->polish(icon);
    } else {
        icon->setText(asset.symbol.left(1));
        icon->setPixmap(QPixmap());
        icon->setAlignment(Qt::AlignCenter);
        icon->setProperty("fallback", true);
        icon->style()->unpolish(icon);
        icon->style()->polish(icon);
    }

    // ── Name ──────────────────────────────────────────────────
    QString displayName =
        asset.name.isEmpty() ? asset.symbol : QString("%1 (%2)").arg(asset.name, asset.symbol);
    QLabel* name = card->findChild<QLabel*>("assetCardName");
    name->setText(displayName);
    name->setToolTip(displayName);

    // ── Total Value ──────────────────────────────────────────
    card->findChild<QLabel*>("assetCardPrice")->setText(formatUsd(asset.valueUsd));

    // ── Holding ──────────────────────────────────────────────
    card->findChild<QLabel*>("cardHoldValue")->setText(formatBalance(asset.balance, asset.symbol));

    // ── Price ────────────────────────────────────────────────
    card->findChild<QLabel*>("cardPriceValue")->setText(formatUsd(asset.priceUsd));

    // ── Send button — reconnect with correct mint ────────────
    QPushButton* sendBtn = card->findChild<QPushButton*>("assetSendBtn");
    disconnect(sendBtn, &QPushButton::clicked, nullptr, nullptr);
    connect(sendBtn, &QPushButton::clicked, this,
            [this, mint = asset.mint]() { emit sendAsset(mint); });

    card->setVisible(true);
}

// ── Show event — relayout cards when page becomes visible ─────────

void AssetsPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Cards couldn't be positioned while the page was hidden (width 0).
    // Now that we have real geometry, lay them out.
    relayoutVisibleCards();
}

// ── Event filter ──────────────────────────────────────────────────

bool AssetsPage::eventFilter(QObject* obj, QEvent* event) {
    // Viewport resize → relayout visible cards
    if (event->type() == QEvent::Resize && m_scroll && obj == m_scroll->viewport()) {
        relayoutVisibleCards();
        return false;
    }

    QWidget* card = qobject_cast<QWidget*>(obj);
    if (!card || card->objectName() != "assetCard") {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Enter) {
        card->setProperty("hovered", true);
        card->style()->unpolish(card);
        card->style()->polish(card);
    } else if (event->type() == QEvent::Leave) {
        card->setProperty("hovered", false);
        card->style()->unpolish(card);
        card->style()->polish(card);
    }

    return QWidget::eventFilter(obj, event);
}
