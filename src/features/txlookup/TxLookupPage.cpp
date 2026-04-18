#include "TxLookupPage.h"
#include "Theme.h"
#include "db/TokenAccountDb.h"
#include "features/txlookup/TxLookupHandler.h"
#include "services/AvatarCache.h"
#include "services/IdlRegistry.h"
#include "services/model/TransactionResponse.h"
#include "tx/InstructionDecoder.h"
#include "tx/KnownTokens.h"
#include "tx/TxClassifier.h"
#include "util/ContactResolver.h"
#include "util/CopyButton.h"
#include "util/TxIconUtils.h"
#include "widgets/ComputeUnitsBar.h"
#include "widgets/PillButtonGroup.h"
#include "widgets/StyledLineEdit.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

// Color palette for instruction badges — cycles for any count
static const QColor badgePalette[] = {
    QColor("#10b981"), // emerald
    QColor("#f59e0b"), // amber
    QColor("#06b6d4"), // cyan
    QColor("#c084fc"), // soft purple
    QColor("#84cc16"), // lime
    QColor("#f97316"), // orange
    QColor("#38bdf8"), // sky blue
    QColor("#a3e635"), // yellow-green
    QColor("#fb923c"), // light orange
};
static const int paletteLen = sizeof(badgePalette) / sizeof(badgePalette[0]);

// ── Known token info (symbol + icon path) from mint address ──────
// KnownToken struct and hardcoded map live in tx/KnownTokens.h.
// This wrapper adds a DB fallback for tokens not in the hardcoded list.

static KnownToken resolveToken(const QString& mint) {
    KnownToken kt = resolveKnownToken(mint);
    if (!kt.symbol.isEmpty()) {
        return kt;
    }

    // Fallback: check token DB for symbol + on-chain logo URL
    auto tok = TokenAccountDb::getTokenRecord(mint);
    if (tok.has_value()) {
        return {tok->symbol, QString(), tok->logoUrl};
    }

    return {};
}

static const QString badgeStyleBase = "font-size: 9px; font-weight: 700; padding: 2px 6px;"
                                      "border-radius: 3px; border: none;";

static void clearLayout(QLayout* layout) {
    if (!layout) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            delete w;
        } else if (QLayout* childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        delete item;
    }
}

static void setToneProperty(QWidget* widget, const char* tone) {
    if (!widget) {
        return;
    }
    widget->setProperty("tone", tone);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

// ── Constructor ─────────────────────────────────────────────────

TxLookupPage::TxLookupPage(SolanaApi* api, IdlRegistry* idlRegistry, QWidget* parent)
    : QWidget(parent), m_idlRegistry(idlRegistry), m_handler(new TxLookupHandler(api, this)) {
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();
    m_stack->addWidget(buildInputPage());  // Step::Input
    m_stack->addWidget(buildDetailPage()); // Step::Detail

    outerLayout->addWidget(m_stack);

    connect(m_handler, &TxLookupHandler::transactionLoaded, this,
            &TxLookupPage::handleFetchedTransaction);
    connect(
        m_handler, &TxLookupHandler::transactionLoadFailed, this,
        [this](const QString& signature, TxLookupHandler::LoadError error, const QString& detail) {
            m_sigLabel->setText(signature);
            if (error == TxLookupHandler::LoadError::FetchFailed) {
                m_blockLabel->setText(tr("Failed to fetch transaction: %1").arg(detail));
            } else {
                m_blockLabel->setText(tr("Transaction not found in local database."));
            }
            resetAnalyzeButton();
            showStep(Step::Detail);
        });
}

void TxLookupPage::showStep(Step step) { m_stack->setCurrentIndex(static_cast<int>(step)); }

void TxLookupPage::setAvatarCache(AvatarCache* cache) { m_avatarCache = cache; }

void TxLookupPage::setWalletAddress(const QString& address) {
    m_walletAddress = address;
    m_handler->setWalletAddress(address);
}

// ── Input Page (index 0) ────────────────────────────────────────

QWidget* TxLookupPage::buildInputPage() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("txLookupContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Title
    QLabel* title = new QLabel(tr("TX Lookup"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Subtitle
    QLabel* subtitle =
        new QLabel(tr("Enter a Solana transaction ID to view detailed information and analysis."));
    subtitle->setObjectName("txaSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    layout->addSpacing(8);

    // Label
    QLabel* inputLabel = new QLabel(tr("Transaction ID (TXID)"));
    inputLabel->setObjectName("txFormLabel");
    layout->addWidget(inputLabel);

    // Input field — uses same #txFormInput styling as send page
    m_txInput = new StyledLineEdit();
    m_txInput->setPlaceholderText(tr("Enter transaction hash..."));
    m_txInput->setMinimumHeight(44);

    QPalette pal = m_txInput->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_txInput->setPalette(pal);
    layout->addWidget(m_txInput);

    layout->addSpacing(8);

    // Analyze button — starts disabled
    m_analyzeBtn = new QPushButton(tr("Analyze Transaction"));
    m_analyzeBtn->setCursor(Qt::PointingHandCursor);
    m_analyzeBtn->setMinimumHeight(48);
    m_analyzeBtn->setEnabled(false);
    m_analyzeBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);

    connect(m_analyzeBtn, &QPushButton::clicked, this, [this]() {
        m_analyzeBtn->setEnabled(false);
        m_analyzeBtn->setText(tr("Loading..."));
        m_analyzeBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
        loadTransaction(m_txInput->text().trimmed());
    });
    layout->addWidget(m_analyzeBtn);

    layout->addStretch();

    // Wire validation
    connect(m_txInput, &QLineEdit::textChanged, this, &TxLookupPage::validateInput);

    scroll->setWidget(content);

    return scroll;
}

// ── Detail Page (index 1) ───────────────────────────────────────

QWidget* TxLookupPage::buildDetailPage() {
    QWidget* page = new QWidget();
    page->setObjectName("txLookupContent");
    page->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Fixed header (back button, title, tab bar) ──────────
    QWidget* header = new QWidget();
    header->setProperty("uiClass", "txaTransparent");
    QVBoxLayout* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(40, 20, 40, 16);
    headerLayout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        if (m_externalEntry) {
            m_externalEntry = false;
            showStep(Step::Input);
            emit backRequested();
        } else {
            showStep(Step::Input);
        }
    });
    headerLayout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Transaction Details"));
    title->setObjectName("newTxTitle");
    headerLayout->addWidget(title);

    // Tab bar
    m_tabGroup = new PillButtonGroup();
    m_tabGroup->setObjectNames("txTabButton", "txTabButtonActive");
    m_tabGroup->addButton(tr("Overview"));
    m_tabGroup->addButton(tr("Balances"));
    m_tabGroup->addButton(tr("Instructions"));
    m_tabGroup->addButton(tr("Logs"));
    headerLayout->addWidget(m_tabGroup, 0, Qt::AlignLeft);

    layout->addWidget(header);

    // ── Helper: wrap a tab content widget in its own scroll area ──
    auto wrapInScroll = [&](QWidget* tabContent) -> QScrollArea* {
        QScrollArea* sa = new QScrollArea();
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setProperty("uiClass", "content");
        sa->viewport()->setProperty("uiClass", "contentViewport");
        tabContent->setProperty("uiClass", "content");
        // Pad content inside the scroll area — consistent across all tabs
        if (tabContent->layout()) {
            tabContent->layout()->setContentsMargins(40, 0, 40, 30);
        }
        sa->setWidget(tabContent);
        return sa;
    };

    // ── Tab content — each tab has its own scroll area ──────
    m_detailTabs = new QStackedWidget();
    m_detailTabs->addWidget(wrapInScroll(buildOverviewTab()));     // 0
    m_detailTabs->addWidget(wrapInScroll(buildBalancesTab()));     // 1
    m_detailTabs->addWidget(wrapInScroll(buildInstructionsTab())); // 2
    m_detailTabs->addWidget(wrapInScroll(buildLogsTab()));         // 3

    layout->addWidget(m_detailTabs, 1);

    // Wire tab group → stacked widget
    connect(m_tabGroup, &PillButtonGroup::currentChanged, m_detailTabs,
            &QStackedWidget::setCurrentIndex);
    m_tabGroup->setActiveIndex(0);

    return page;
}

// ── Overview Tab ────────────────────────────────────────────────

QWidget* TxLookupPage::buildOverviewTab() {
    QWidget* tab = new QWidget();
    tab->setProperty("uiClass", "txaTransparent");
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(16);

    // ── Helper lambdas ──────────────────────────────────────
    auto addSep = [](QVBoxLayout* target) {
        QFrame* sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("txaSeparator");
        sep->setFixedHeight(1);
        target->addWidget(sep);
    };

    auto addRow = [](QVBoxLayout* target, const QString& label, QWidget* valueWidget) {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 10, 0, 10);

        QLabel* lbl = new QLabel(label);
        lbl->setProperty("uiClass", "txaLabel");
        lbl->setFixedWidth(180);

        h->addWidget(lbl);
        h->addWidget(valueWidget, 1);
        target->addWidget(row);
    };

    auto makeValue = [](const QString& text, bool mono = false) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setProperty("uiClass", mono ? "txaMonoValue" : "txaValue");
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return lbl;
    };

    // Copyable address row: green mono label + copy button
    auto makeCopyableRow = [](QLabel*& labelRef, const QString& text) -> QWidget* {
        QWidget* container = new QWidget();
        container->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(container);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);

        labelRef = new QLabel(text);
        labelRef->setProperty("uiClass", "txaAddressValue");
        labelRef->setTextInteractionFlags(Qt::TextSelectableByMouse);
        labelRef->setWordWrap(true);

        QPushButton* copyBtn = new QPushButton();
        CopyButton::applyIcon(copyBtn);
        copyBtn->setFixedSize(26, 26);
        copyBtn->setCursor(Qt::PointingHandCursor);
        copyBtn->setObjectName("txaCopyBtn");

        QLabel* lbl = labelRef;
        QObject::connect(copyBtn, &QPushButton::clicked, copyBtn, [copyBtn, lbl]() {
            QApplication::clipboard()->setText(lbl->text());
            copyBtn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
            QTimer::singleShot(1500, copyBtn, [copyBtn]() { CopyButton::applyIcon(copyBtn); });
        });

        h->addWidget(labelRef);
        h->addWidget(copyBtn);
        h->addStretch();
        return container;
    };

    QString dash = QString::fromUtf8("\xe2\x80\x94");

    // ── Summary Card ────────────────────────────────────────
    {
        QWidget* summaryCard = new QWidget();
        summaryCard->setObjectName("txaCard");
        QHBoxLayout* sh = new QHBoxLayout(summaryCard);
        sh->setContentsMargins(24, 16, 24, 16);
        sh->setSpacing(12);

        m_summaryIcon = new QLabel();
        m_summaryIcon->setFixedSize(20, 20);
        m_summaryIcon->setAlignment(Qt::AlignCenter);
        m_summaryIcon->setObjectName("txaSummaryIcon");
        sh->addWidget(m_summaryIcon, 0, Qt::AlignTop);

        QWidget* summaryContent = new QWidget();
        summaryContent->setProperty("uiClass", "txaTransparent");
        m_summaryLayout = new QVBoxLayout(summaryContent);
        m_summaryLayout->setContentsMargins(0, 0, 0, 0);
        m_summaryLayout->setSpacing(6);
        sh->addWidget(summaryContent, 1);

        layout->addWidget(summaryCard);
    }

    // ── Transaction Details Card ────────────────────────────
    QWidget* detailCard = new QWidget();
    detailCard->setObjectName("txaCard");
    QVBoxLayout* cardLayout = new QVBoxLayout(detailCard);
    cardLayout->setContentsMargins(24, 16, 24, 16);
    cardLayout->setSpacing(0);

    // Signature (copyable + Solscan link)
    {
        QWidget* sigRow = makeCopyableRow(m_sigLabel, dash);
        QHBoxLayout* sigLayout = qobject_cast<QHBoxLayout*>(sigRow->layout());

        // Remove trailing stretch so we can insert before it
        QLayoutItem* stretch = sigLayout->takeAt(sigLayout->count() - 1);

        QPushButton* solscanBtn = new QPushButton();
        QPixmap scanPix(":/icons/ui/solscan.png");
        if (!scanPix.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            scanPix =
                scanPix.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scanPix.setDevicePixelRatio(dpr);
            solscanBtn->setIcon(QIcon(scanPix));
            solscanBtn->setIconSize(QSize(16, 16));
        }
        solscanBtn->setFixedSize(26, 26);
        solscanBtn->setCursor(Qt::PointingHandCursor);
        solscanBtn->setObjectName("txaCopyBtn");
        solscanBtn->setToolTip(tr("View on Solscan"));

        QLabel* sigLbl = m_sigLabel;
        connect(solscanBtn, &QPushButton::clicked, [sigLbl]() {
            const QString sig = sigLbl->text();
            if (!sig.isEmpty() && !sig.contains(QChar(0x2014))) {
                QDesktopServices::openUrl(QUrl("https://solscan.io/tx/" + sig));
            }
        });

        sigLayout->addWidget(solscanBtn);
        sigLayout->addItem(stretch);

        addRow(cardLayout, tr("Signature"), sigRow);
    }
    addSep(cardLayout);

    // Block
    m_blockLabel = makeValue(dash);
    addRow(cardLayout, tr("Block"), m_blockLabel);
    addSep(cardLayout);

    // Timestamp
    m_timestampLabel = makeValue(dash);
    addRow(cardLayout, tr("Timestamp"), m_timestampLabel);
    addSep(cardLayout);

    // Result
    {
        QWidget* resultWidget = new QWidget();
        resultWidget->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* rh = new QHBoxLayout(resultWidget);
        rh->setContentsMargins(0, 0, 0, 0);
        rh->setSpacing(10);

        m_resultBadge = new QLabel(dash);
        m_resultBadge->setObjectName("txaResultBadge");
        m_resultBadge->setFixedHeight(24);

        m_confirmLabel = new QLabel("");
        m_confirmLabel->setProperty("uiClass", "txaConfirmLabel");

        rh->addWidget(m_resultBadge);
        rh->addWidget(m_confirmLabel);
        rh->addStretch();

        addRow(cardLayout, tr("Result"), resultWidget);
    }
    addSep(cardLayout);

    // Signer (copyable address)
    addRow(cardLayout, tr("Signer"), makeCopyableRow(m_signerLabel, dash));
    addSep(cardLayout);

    // Fee
    m_feeLabel = makeValue(dash);
    addRow(cardLayout, tr("Fee"), m_feeLabel);
    addSep(cardLayout);

    // Priority Fee
    m_priorityFeeLabel = makeValue(dash);
    addRow(cardLayout, tr("Priority Fee"), m_priorityFeeLabel);
    addSep(cardLayout);

    // Compute Units Consumed
    m_cuConsumedLabel = makeValue(dash);
    addRow(cardLayout, tr("Compute Units Consumed"), m_cuConsumedLabel);
    addSep(cardLayout);

    // Transaction Version
    m_versionLabel = makeValue(dash);
    addRow(cardLayout, tr("Transaction Version"), m_versionLabel);
    addSep(cardLayout);

    // Nonce row (hidden by default, shown when tx uses durable nonce)
    {
        m_nonceRow = new QWidget();
        m_nonceRow->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(m_nonceRow);
        h->setContentsMargins(0, 10, 0, 10);

        QLabel* lbl = new QLabel(tr("Nonce"));
        lbl->setProperty("uiClass", "txaLabel");
        lbl->setFixedWidth(180);
        h->addWidget(lbl);

        m_nonceLabel = new QLabel(dash);
        m_nonceLabel->setProperty("uiClass", "txaMonoValue");
        m_nonceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_nonceLabel->setWordWrap(true);
        h->addWidget(m_nonceLabel, 1);

        m_nonceRow->setVisible(false);
        cardLayout->addWidget(m_nonceRow);
    }

    // Recent Block Hash (hidden when nonce is used, since nonce row replaces it)
    m_blockHashLabel = makeValue(dash, true);
    m_blockHashLabel->setWordWrap(true);
    addRow(cardLayout, tr("Recent Block Hash"), m_blockHashLabel);

    layout->addWidget(detailCard);

    // ── Compute Units Distribution Card ─────────────────────
    QWidget* cuCard = new QWidget();
    cuCard->setObjectName("txaCard");
    QVBoxLayout* cuLayout = new QVBoxLayout(cuCard);
    cuLayout->setContentsMargins(24, 20, 24, 20);
    cuLayout->setSpacing(12);

    // Title row with total on the right
    QWidget* cuTitleRow = new QWidget();
    cuTitleRow->setProperty("uiClass", "txaTransparent");
    QHBoxLayout* cuTitleLayout = new QHBoxLayout(cuTitleRow);
    cuTitleLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* cuTitle = new QLabel(tr("Compute Units"));
    cuTitle->setObjectName("txaCardTitleSmall");

    m_cuTotalLabel = new QLabel(dash);
    m_cuTotalLabel->setProperty("uiClass", "txaLabel");

    cuTitleLayout->addWidget(cuTitle);
    cuTitleLayout->addStretch();
    cuTitleLayout->addWidget(m_cuTotalLabel);
    cuLayout->addWidget(cuTitleRow);

    // Stacked bar (empty initially — populated by populateComputeUnits)
    m_cuBar = new ComputeUnitsBar();
    m_cuBar->setProperty("uiClass", "txaTransparent");
    cuLayout->addWidget(m_cuBar);

    // Legend (populated dynamically)
    m_cuLegendLayout = new QVBoxLayout();
    m_cuLegendLayout->setSpacing(6);
    cuLayout->addLayout(m_cuLegendLayout);

    layout->addWidget(cuCard);

    layout->addStretch();
    return tab;
}

// ── Instructions Tab (empty container — populated dynamically) ──

QWidget* TxLookupPage::buildInstructionsTab() {
    QWidget* tab = new QWidget();
    tab->setProperty("uiClass", "txaTransparent");
    m_instrLayout = new QVBoxLayout(tab);
    m_instrLayout->setContentsMargins(0, 8, 0, 0);
    m_instrLayout->setSpacing(16);
    m_instrLayout->addStretch();
    return tab;
}

// ── Balances Tab (empty container — populated dynamically) ──────

QWidget* TxLookupPage::buildBalancesTab() {
    QWidget* tab = new QWidget();
    tab->setProperty("uiClass", "txaTransparent");
    m_balancesLayout = new QVBoxLayout(tab);
    m_balancesLayout->setContentsMargins(0, 8, 0, 0);
    m_balancesLayout->setSpacing(16);
    m_balancesLayout->addStretch();
    return tab;
}

// ── Logs Tab (empty container — populated dynamically) ──────────

QWidget* TxLookupPage::buildLogsTab() {
    QWidget* tab = new QWidget();
    tab->setProperty("uiClass", "txaTransparent");
    m_logsLayout = new QVBoxLayout(tab);
    m_logsLayout->setContentsMargins(0, 8, 0, 0);
    m_logsLayout->setSpacing(16);
    m_logsLayout->addStretch();
    return tab;
}

// ── Public: open detail page directly with a signature ──────────

void TxLookupPage::openWithSignature(const QString& signature) {
    m_externalEntry = true;
    m_txInput->setText(signature);
    loadTransaction(signature);
}

// ── Load transaction: try DB first, then fall back to RPC ───────

void TxLookupPage::loadTransaction(const QString& signature) {
    // Always start on the Overview tab for a new transaction
    m_tabGroup->setActiveIndex(0);
    m_sigLabel->setText(signature);
    m_blockLabel->setText(tr("Loading from network..."));
    m_handler->loadTransaction(signature);
}

void TxLookupPage::handleFetchedTransaction(const QString& sig, const TransactionResponse& tx) {
    // Clear address hover registry from previous transaction
    m_addressLabels.clear();
    m_hoveredAddress.clear();

    populateOverview(sig, tx);
    populateComputeUnits(tx);
    populateInstructions(tx);
    populateBalances(tx);
    populateLogs(tx);
    resetAnalyzeButton();
    showStep(Step::Detail);
}

// ── Populate Overview labels ────────────────────────────────────

void TxLookupPage::populateOverview(const QString& signature, const TransactionResponse& tx) {
    disconnect(m_dexNameConn);
    m_sigLabel->setText(signature);
    m_blockLabel->setText(QLocale().toString(static_cast<qlonglong>(tx.slot)));

    if (tx.blockTime > 0) {
        QDateTime ts = QDateTime::fromSecsSinceEpoch(tx.blockTime, QTimeZone::UTC);
        m_timestampLabel->setText(ts.toString("MMM d, yyyy h:mm:ss AP") + " UTC");
    } else {
        m_timestampLabel->setText(tr("Unavailable"));
    }

    if (tx.meta.hasError) {
        m_resultBadge->setText("FAILED");
        setToneProperty(m_resultBadge, "error");
    } else {
        m_resultBadge->setText(tr("SUCCESS"));
        setToneProperty(m_resultBadge, "success");
    }
    m_confirmLabel->setText(tr("Finalized"));
    if (tx.meta.hasError) {
        setToneProperty(m_confirmLabel, "error");
    } else {
        setToneProperty(m_confirmLabel, "success");
    }

    const QString signerAddress = m_handler->firstSigner(tx);
    m_signerLabel->setText(signerAddress);
    if (!signerAddress.isEmpty()) {
        registerAddressLabel(signerAddress, m_signerLabel);
    }

    // Fee in SOL
    double feeSol = tx.meta.fee / 1e9;
    m_feeLabel->setText(QString::fromUtf8("\xe2\x97\x8e ") + QString::number(feeSol, 'f', 9) +
                        " SOL");

    // Priority fee = total fee - base fee (5000 lamports per signature)
    qint64 priorityFeeLamports = m_handler->priorityFeeLamports(tx);
    if (priorityFeeLamports > 0) {
        double priorityFeeSol = priorityFeeLamports / 1e9;
        m_priorityFeeLabel->setText(QString::fromUtf8("\xe2\x97\x8e ") +
                                    QString::number(priorityFeeSol, 'f', 9) + " SOL" + "  (" +
                                    QLocale().toString(priorityFeeLamports) + " lamports)");
    } else {
        m_priorityFeeLabel->setText("None");
    }

    m_cuConsumedLabel->setText(
        QLocale().toString(static_cast<qlonglong>(tx.meta.computeUnitsConsumed)));

    m_versionLabel->setText(tx.version);

    // Detect durable nonce: first instruction is AdvanceNonceAccount
    const bool usesNonce = m_handler->usesDurableNonce(tx);
    if (usesNonce) {
        m_nonceLabel->setText(tx.message.recentBlockhash);
        m_nonceRow->setVisible(true);
        m_blockHashLabel->parentWidget()->setVisible(false);
    } else {
        m_nonceRow->setVisible(false);
        m_blockHashLabel->parentWidget()->setVisible(true);
        m_blockHashLabel->setText(tx.message.recentBlockhash);
    }

    // ── Transaction Summary ──
    auto cls = m_handler->classify(tx, m_walletAddress);

    // Icon per type — SVG from resources, tinted per category
    QString iconName;
    QColor iconTint;
    switch (cls.type) {
        case TxClassifier::TxType::Failed:
            iconName = "failed";
            iconTint = QColor("#ef4444");
            break;
        case TxClassifier::TxType::SolTransfer:
        case TxClassifier::TxType::TokenTransfer:
            iconName = "receive";
            iconTint = QColor("#60a5fa");
            break;
        case TxClassifier::TxType::Swap:
            iconName = "swap";
            iconTint = QColor("#a78bfa");
            break;
        case TxClassifier::TxType::Stake:
        case TxClassifier::TxType::Unstake:
            iconName = "stake";
            iconTint = QColor("#34d399");
            break;
        case TxClassifier::TxType::CreateAccount:
        case TxClassifier::TxType::CreateTokenAccount:
            iconName = "create";
            iconTint = QColor("#94a3b8");
            break;
        case TxClassifier::TxType::TokenMint:
            iconName = "mint";
            iconTint = QColor("#22d3ee");
            break;
        case TxClassifier::TxType::TokenBurn:
            iconName = "burn";
            iconTint = QColor("#f97316");
            break;
        case TxClassifier::TxType::CloseTokenAccount:
            iconName = "close";
            iconTint = QColor("#94a3b8");
            break;
        default:
            iconName = "default";
            iconTint = QColor(255, 255, 255, 128);
            break;
    }
    m_summaryIcon->setPixmap(txTypeIcon(iconName, 20, devicePixelRatioF(), iconTint));

    // Rebuild summary layout
    clearLayout(m_summaryLayout);

    auto fmtAmt = [](double v) -> QString {
        if (v == 0) {
            return "0";
        }
        QLocale loc;
        if (v >= 100) {
            return loc.toString(v, 'f', 2);
        }
        if (v >= 1) {
            return loc.toString(v, 'f', 4);
        }
        // For sub-1 values, use fixed-point with enough precision to
        // avoid scientific notation (e.g. "0.00001" not "1e-05")
        int decimals = 6;
        if (v < 0.0001) {
            decimals = 9;
        }
        return loc.toString(v, 'f', decimals);
    };

    // Helper: add a token icon + symbol pair to a layout.
    // displayOverride forces a specific symbol (e.g. "SOL" for native SOL contexts).
    auto addTokenToRow = [this](QHBoxLayout* row, const QString& mint,
                                const QString& fallbackSym = QString(),
                                const QString& displayOverride = QString()) {
        KnownToken tk = resolveToken(mint);
        QPixmap pix;
        if (!tk.iconPath.isEmpty()) {
            pix = QPixmap(tk.iconPath);
        }
        if (pix.isNull() && !tk.logoUrl.isEmpty() && m_avatarCache) {
            pix = m_avatarCache->get(tk.logoUrl);
        }
        if (!pix.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            pix = pix.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            pix.setDevicePixelRatio(dpr);
            QLabel* ic = new QLabel();
            ic->setPixmap(pix);
            ic->setFixedSize(16, 16);
            ic->setProperty("uiClass", "txaTransparent");
            row->addWidget(ic);
        }
        QString sym = displayOverride.isEmpty() ? tk.symbol : displayOverride;
        if (sym.isEmpty()) {
            sym = fallbackSym;
        }
        if (sym.isEmpty() && !mint.isEmpty()) {
            sym = mint.left(4) + "..." + mint.right(4);
        }
        QLabel* symLbl = new QLabel(sym);
        symLbl->setProperty("uiClass", "txaSummarySym");
        symLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(symLbl);
    };

    // Helper: add a copyable, hoverable address row with a dim label
    auto addSummaryAddrRow = [this](const QString& label, const QString& address) {
        QWidget* row = new QWidget();
        row->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);

        QLabel* lbl = new QLabel(label);
        lbl->setProperty("uiClass", "txaSummaryDim");
        lbl->setFixedWidth(36);
        h->addWidget(lbl);

        QLabel* addr = new QLabel(address);
        addr->setProperty("uiClass", "txaAddressValue");
        addr->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(addr);
        registerAddressLabel(address, addr);

        QPushButton* copyBtn = new QPushButton();
        CopyButton::applyIcon(copyBtn);
        copyBtn->setFixedSize(26, 26);
        copyBtn->setCursor(Qt::PointingHandCursor);
        copyBtn->setObjectName("txaCopyBtn");
        QObject::connect(copyBtn, &QPushButton::clicked, copyBtn, [copyBtn, address]() {
            QApplication::clipboard()->setText(address);
            CopyButton::applyIcon(copyBtn);
            copyBtn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
            QTimer::singleShot(1500, copyBtn, [copyBtn]() { CopyButton::applyIcon(copyBtn); });
        });
        h->addWidget(copyBtn);
        h->addStretch();

        m_summaryLayout->addWidget(row);
    };

    switch (cls.type) {
        case TxClassifier::TxType::SolTransfer:
        case TxClassifier::TxType::TokenTransfer: {
            // Row 1: "SOL Send — 0.01 [icon] SOL"
            QWidget* titleRow = new QWidget();
            titleRow->setProperty("uiClass", "txaTransparent");
            QHBoxLayout* tr1 = new QHBoxLayout(titleRow);
            tr1->setContentsMargins(0, 0, 0, 0);
            tr1->setSpacing(6);

            QLabel* typeLbl = new QLabel("<b>" + cls.label + "</b>");
            typeLbl->setProperty("uiClass", "txaSummaryTitle");
            typeLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            tr1->addWidget(typeLbl);

            QLabel* dash = new QLabel(QString::fromUtf8("\xe2\x80\x94"));
            dash->setProperty("uiClass", "txaSummaryDim");
            tr1->addWidget(dash);

            QLabel* amtLbl = new QLabel(fmtAmt(cls.amount));
            amtLbl->setProperty("uiClass", "txaSummaryAmt");
            amtLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            tr1->addWidget(amtLbl);

            if (cls.type == TxClassifier::TxType::SolTransfer) {
                addTokenToRow(tr1, WSOL_MINT, "", "SOL");
            } else if (!cls.mint.isEmpty()) {
                addTokenToRow(tr1, cls.mint);
            }
            tr1->addStretch();
            m_summaryLayout->addWidget(titleRow);

            // Row 2: From [address]
            if (!cls.from.isEmpty()) {
                addSummaryAddrRow(tr("From"), cls.from);
            }
            // Row 3+: To [address(es)]
            if (!cls.to.isEmpty() && cls.extraRecipients.isEmpty()) {
                // Single recipient — flat row, no tree
                addSummaryAddrRow(tr("To"), cls.to);
            } else if (!cls.to.isEmpty()) {
                // Multiple recipients — tree hierarchy with per-address amounts
                QString tokenSym = cls.type == TxClassifier::TxType::SolTransfer
                                       ? QStringLiteral("SOL")
                                       : cls.tokenSymbol;

                auto addTreeRow = [&](const QString& address, double amt, bool isLast) {
                    QWidget* row = new QWidget();
                    row->setProperty("uiClass", "txaTransparent");
                    QHBoxLayout* h = new QHBoxLayout(row);
                    h->setContentsMargins(0, 0, 0, 0);
                    h->setSpacing(6);

                    // "To" label only on first row, blank spacer on subsequent
                    QLabel* lbl =
                        new QLabel(isLast && !cls.extraRecipients.isEmpty() ? QString() : tr("To"));
                    lbl->setProperty("uiClass", "txaSummaryDim");
                    lbl->setFixedWidth(36);
                    h->addWidget(lbl);

                    // Tree connector
                    QString connector = isLast
                                            ? QString::fromUtf8("\xe2\x94\x94\xe2\x94\x80")  // └─
                                            : QString::fromUtf8("\xe2\x94\x9c\xe2\x94\x80"); // ├─
                    QLabel* tree = new QLabel(connector);
                    tree->setProperty("uiClass", "txaSummaryDim");
                    tree->setFixedWidth(16);
                    h->addWidget(tree);

                    // Address
                    QLabel* addr = new QLabel(address);
                    addr->setProperty("uiClass", "txaAddressValue");
                    addr->setTextInteractionFlags(Qt::TextSelectableByMouse);
                    h->addWidget(addr);
                    registerAddressLabel(address, addr);

                    // Amount annotation
                    QLabel* amtLbl = new QLabel("(" + fmtAmt(amt) + " " + tokenSym + ")");
                    amtLbl->setProperty("uiClass", "txaSummaryDim");
                    h->addWidget(amtLbl);

                    // Copy button
                    QPushButton* copyBtn = new QPushButton();
                    CopyButton::applyIcon(copyBtn);
                    copyBtn->setFixedSize(26, 26);
                    copyBtn->setCursor(Qt::PointingHandCursor);
                    copyBtn->setObjectName("txaCopyBtn");
                    QObject::connect(copyBtn, &QPushButton::clicked, [address]() {
                        QApplication::clipboard()->setText(address);
                        QToolTip::showText(QCursor::pos(), "Copied!");
                    });
                    h->addWidget(copyBtn);
                    h->addStretch();

                    m_summaryLayout->addWidget(row);
                };

                addTreeRow(cls.to, cls.toAmount, false);
                for (int i = 0; i < cls.extraRecipients.size(); ++i) {
                    bool last = (i == cls.extraRecipients.size() - 1);
                    addTreeRow(cls.extraRecipients[i].address, cls.extraRecipients[i].amount, last);
                }
            }
            break;
        }
        case TxClassifier::TxType::Swap: {
            // Row 1: "Swap on Meteora DLMM"
            QWidget* titleRow = new QWidget();
            titleRow->setProperty("uiClass", "txaTransparent");
            QHBoxLayout* tr1 = new QHBoxLayout(titleRow);
            tr1->setContentsMargins(0, 0, 0, 0);
            tr1->setSpacing(6);

            QLabel* typeLbl = new QLabel("<b>" + cls.label + "</b>");
            typeLbl->setProperty("uiClass", "txaSummaryTitle");
            typeLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            tr1->addWidget(typeLbl);

            if (!cls.dexProgramId.isEmpty()) {
                // Resolve DEX name: known DEX list → IdlRegistry → truncated ID
                QString dName = TxClassifier::dexName(cls.dexProgramId);
                if (dName.contains("...")) {
                    dName = m_handler->friendlyProgramName(cls.dexProgramId, m_idlRegistry);
                }

                QLabel* onLbl = new QLabel("on");
                onLbl->setProperty("uiClass", "txaSummaryDim");
                tr1->addWidget(onLbl);

                QLabel* dexLbl = new QLabel("<b>" + dName + "</b>");
                dexLbl->setProperty("uiClass", "txaSummaryTitle");
                dexLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
                tr1->addWidget(dexLbl);

                // If name is still truncated, trigger async IDL fetch and update
                if (dName.contains("...") && m_idlRegistry) {
                    QString dexId = cls.dexProgramId;
                    m_idlRegistry->resolve(dexId);
                    disconnect(m_dexNameConn);
                    QPointer<QLabel> dexLblPtr = dexLbl;
                    m_dexNameConn = connect(m_idlRegistry, &IdlRegistry::idlReady, this,
                                            [this, dexLblPtr, dexId](const QString& programId) {
                                                if (programId == dexId && dexLblPtr) {
                                                    QString n = m_handler->friendlyProgramName(
                                                        dexId, m_idlRegistry);
                                                    dexLblPtr->setText("<b>" + n + "</b>");
                                                    disconnect(m_dexNameConn);
                                                }
                                            });
                }
            }
            tr1->addStretch();
            m_summaryLayout->addWidget(titleRow);

            // Row 2: "12.30 [icon] TOKEN  →  360.28 [icon] TOKEN"
            QWidget* amtRow = new QWidget();
            amtRow->setProperty("uiClass", "txaTransparent");
            QHBoxLayout* ar = new QHBoxLayout(amtRow);
            ar->setContentsMargins(0, 0, 0, 0);
            ar->setSpacing(6);

            QLabel* amtIn = new QLabel(fmtAmt(cls.amountIn));
            amtIn->setProperty("uiClass", "txaSummaryAmt");
            amtIn->setTextInteractionFlags(Qt::TextSelectableByMouse);
            ar->addWidget(amtIn);
            addTokenToRow(ar, cls.mintIn, "", cls.mintIn == WSOL_MINT ? "SOL" : QString());

            QLabel* arrow = new QLabel(QString::fromUtf8(" \xe2\x86\x92 "));
            arrow->setProperty("uiClass", "txaSummaryDim");
            ar->addWidget(arrow);

            QLabel* amtOut = new QLabel(fmtAmt(cls.amountOut));
            amtOut->setProperty("uiClass", "txaSummaryAmt");
            amtOut->setTextInteractionFlags(Qt::TextSelectableByMouse);
            ar->addWidget(amtOut);
            addTokenToRow(ar, cls.mintOut, "", cls.mintOut == WSOL_MINT ? "SOL" : QString());

            ar->addStretch();
            m_summaryLayout->addWidget(amtRow);
            break;
        }
        default: {
            // Simple label for other types
            QLabel* typeLbl = new QLabel("<b>" + cls.label + "</b>");
            typeLbl->setProperty("uiClass", "txaSummaryTitle");
            typeLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_summaryLayout->addWidget(typeLbl);
            break;
        }
    }
}

// ── Populate Compute Units bar and legend ───────────────────────

void TxLookupPage::populateComputeUnits(const TransactionResponse& tx) {
    // Use TxParseUtils which correctly distributes CU across all instructions
    // (including ones without explicit "consumed" log lines like ComputeBudget)
    auto cuEntries = m_handler->computeUnitEntries(tx);

    QList<ComputeUnitsBar::Segment> segments;
    for (int i = 0; i < cuEntries.size(); ++i) {
        QColor color = badgePalette[i % paletteLen];
        segments.append({m_handler->friendlyProgramName(cuEntries[i].programId, m_idlRegistry),
                         cuEntries[i].units, color});
    }

    // Fallback: no invoke/success pairs found — split total evenly per instruction
    if (segments.isEmpty() && tx.meta.computeUnitsConsumed > 0) {
        int numInstr = tx.message.instructions.size();
        if (numInstr > 0) {
            int perInstr = static_cast<int>(tx.meta.computeUnitsConsumed) / numInstr;
            int remainder = static_cast<int>(tx.meta.computeUnitsConsumed) % numInstr;
            for (int i = 0; i < numInstr; ++i) {
                QColor color = badgePalette[i % paletteLen];
                QString label = tr("Instruction #%1").arg(i + 1);
                int cu = perInstr + (i == numInstr - 1 ? remainder : 0);
                segments.append({label, cu, color});
            }
        } else {
            segments.append(
                {tr("Total"), static_cast<int>(tx.meta.computeUnitsConsumed), QColor("#10b981")});
        }
    }

    m_cuBar->setSegments(segments);

    // Total label
    QLocale locale;
    int total = 0;
    for (const auto& s : segments) {
        total += s.units;
    }
    m_cuTotalLabel->setText(tr("Total: %1 CU").arg(locale.toString(total)));

    // Rebuild legend
    clearLayout(m_cuLegendLayout);

    for (const auto& seg : segments) {
        QWidget* legendRow = new QWidget();
        legendRow->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* lh = new QHBoxLayout(legendRow);
        lh->setContentsMargins(0, 2, 0, 2);
        lh->setSpacing(10);

        QLabel* dot = new QLabel();
        dot->setFixedSize(10, 10);
        dot->setStyleSheet(
            QString("background: %1; border-radius: 5px; border: none;").arg(seg.color.name()));

        QLabel* name = new QLabel(seg.label);
        name->setProperty("uiClass", "txaLegendName");
        name->setTextInteractionFlags(Qt::TextSelectableByMouse);

        QLabel* value = new QLabel(locale.toString(seg.units) + " CU");
        value->setProperty("uiClass", "txaLegendValue");
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);

        lh->addWidget(dot);
        lh->addWidget(name);
        lh->addStretch();
        lh->addWidget(value);
        m_cuLegendLayout->addWidget(legendRow);
    }
}

// ── Populate Instructions tab ───────────────────────────────────

void TxLookupPage::populateInstructions(const TransactionResponse& tx) {
    clearLayout(m_instrLayout);

    // Client-side decode unparsed instructions (ComputeBudget, System, Anchor IDLs)
    TransactionResponse decoded = tx;
    InstructionDecoder::decodeAll(decoded, m_idlRegistry);

    // Trigger background IDL fetch for any still-unparsed programs
    // (IDL will be cached for next time the user views a tx with this program)
    if (m_idlRegistry) {
        for (const auto& ix : decoded.message.instructions) {
            if (!ix.isParsed()) {
                m_idlRegistry->resolve(ix.programId);
            }
        }
    }

    const QMap<int, QList<Instruction>> innerMap = m_handler->innerInstructionsByIndex(decoded);

    // ── Lambdas ─────────────────────────────────────────────

    auto sep = [](QVBoxLayout* t) {
        QFrame* s = new QFrame();
        s->setFrameShape(QFrame::HLine);
        s->setObjectName("txaSeparator");
        s->setFixedHeight(1);
        t->addWidget(s);
    };

    auto row = [](QVBoxLayout* t, const QString& label, const QString& value,
                  const QString& valueClass, bool copyable = false) {
        QWidget* r = new QWidget();
        r->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(r);
        h->setContentsMargins(0, 10, 0, 10);

        QLabel* l = new QLabel(label);
        l->setProperty("uiClass", "txaLabel");
        l->setFixedWidth(170);

        QLabel* v = new QLabel(value);
        v->setProperty("uiClass", valueClass);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setWordWrap(true);

        h->addWidget(l);

        if (copyable) {
            h->addWidget(v);

            QPushButton* copyBtn = new QPushButton();
            CopyButton::applyIcon(copyBtn);
            copyBtn->setFixedSize(26, 26);
            copyBtn->setCursor(Qt::PointingHandCursor);
            copyBtn->setObjectName("txaCopyBtn");

            const QString& textToCopy = value;
            QObject::connect(copyBtn, &QPushButton::clicked, [textToCopy]() {
                QApplication::clipboard()->setText(textToCopy);
                QToolTip::showText(QCursor::pos(), "Copied!");
            });
            h->addWidget(copyBtn);
            h->addStretch();
        } else {
            h->addWidget(v, 1);
        }

        t->addWidget(r);
    };

    // Chevron pixmaps for accordion expand/collapse
    auto makeChevron = [](const QString& iconPath, int logicalSize = 14) {
        QPixmap pix(iconPath);
        if (pix.isNull()) {
            return pix;
        }
        qreal dpr = qApp->devicePixelRatio();
        pix = pix.scaled(logicalSize * dpr, logicalSize * dpr, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
        pix.setDevicePixelRatio(dpr);
        return pix;
    };
    QPixmap chevronDown = makeChevron(":/icons/ui/chevron-down.png");
    QPixmap chevronUp = makeChevron(":/icons/ui/chevron-up.png");

    auto instrHeader = [](const QString& idx, const QString& title, int colorIdx,
                          QLabel** outChevron = nullptr) -> QPushButton* {
        QPushButton* btn = new QPushButton();
        btn->setObjectName("txaAccordionHeader");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(36);
        QHBoxLayout* h = new QHBoxLayout(btn);
        h->setContentsMargins(0, 6, 0, 6);
        h->setSpacing(10);

        QColor c = badgePalette[colorIdx % paletteLen];

        QLabel* badge = new QLabel(idx);
        badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        badge->setStyleSheet(QString("background: rgba(%1, %2, %3, 0.2);"
                                     "color: %4;"
                                     "border-radius: 4px;"
                                     "padding: 2px 8px;"
                                     "font-size: 11px;"
                                     "font-weight: 700;"
                                     "border: none;")
                                 .arg(c.red())
                                 .arg(c.green())
                                 .arg(c.blue())
                                 .arg(c.name()));

        QLabel* t = new QLabel(title);
        t->setAttribute(Qt::WA_TransparentForMouseEvents);
        t->setProperty("uiClass", "txaSummaryTitle");

        h->addWidget(badge);
        h->addWidget(t);
        h->addStretch();

        // Chevron icon (only for top-level cards, not inner instructions)
        if (outChevron) {
            QLabel* chevLabel = new QLabel();
            chevLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            chevLabel->setProperty("uiClass", "txaTransparent");
            chevLabel->setFixedSize(16, 16);
            chevLabel->setAlignment(Qt::AlignCenter);
            h->addWidget(chevLabel);
            *outChevron = chevLabel;
        }

        return btn;
    };

    const QMap<QString, AccountKey> accountKeyMap = m_handler->accountKeyByAddress(decoded);
    const QString feePayer = m_handler->feePayerAddress(decoded);

    // Lambda to add a custom widget row (label + arbitrary widget)
    auto rowWidget = [](QVBoxLayout* t, const QString& label, QWidget* widget) {
        QWidget* r = new QWidget();
        r->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(r);
        h->setContentsMargins(0, 10, 0, 10);

        QLabel* l = new QLabel(label);
        l->setProperty("uiClass", "txaLabel");
        l->setFixedWidth(170);

        h->addWidget(l);
        h->addWidget(widget, 1);
        t->addWidget(r);
    };

    // Lambda to create a small colored badge label
    auto makeBadge = [](const QString& text, const QString& bg, const QString& fg) -> QLabel* {
        QLabel* b = new QLabel(text);
        b->setStyleSheet(QString("background: %1; color: %2; %3").arg(bg, fg, badgeStyleBase));
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        return b;
    };

    // Lambda to render an address row with copy + WRITABLE/SIGNER/FEE PAYER badges
    // Resolves known tokens (logo + symbol) and known programs (name + PROGRAM badge)
    auto addressRow = [&](QVBoxLayout* t, const QString& label, const QString& address) {
        QWidget* r = new QWidget();
        r->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(r);
        h->setContentsMargins(0, 10, 0, 10);

        QLabel* l = new QLabel(label);
        l->setProperty("uiClass", "txaLabel");
        l->setFixedWidth(170);

        h->addWidget(l);

        bool isProgram = false;
        bool isToken = false;

        // Check if address is a known token mint
        KnownToken token = resolveToken(address);
        if (!token.symbol.isEmpty()) {
            isToken = true;
            QPixmap pix;
            if (!token.iconPath.isEmpty()) {
                pix = QPixmap(token.iconPath);
            }
            if (pix.isNull() && !token.logoUrl.isEmpty() && m_avatarCache) {
                pix = m_avatarCache->get(token.logoUrl);
            }
            if (!pix.isNull()) {
                qreal dpr = qApp->devicePixelRatio();
                pix = pix.scaled(18 * dpr, 18 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                pix.setDevicePixelRatio(dpr);
                QLabel* icon = new QLabel();
                icon->setPixmap(pix);
                icon->setFixedSize(18, 18);
                icon->setProperty("uiClass", "txaTransparent");
                h->addWidget(icon);
            }
            QLabel* sym = new QLabel(token.symbol);
            sym->setProperty("uiClass", "txaSummaryAmt");
            h->addWidget(sym);
        } else {
            // Check if address is a known program
            QString progName = m_handler->friendlyProgramName(address, m_idlRegistry);
            if (!progName.contains("...")) {
                isProgram = true;
            }
        }

        // Always show full address
        QLabel* v = new QLabel(address);
        v->setProperty("uiClass", "txaAddressValue");
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(v);
        registerAddressLabel(address, v);

        // Copy button
        QPushButton* copyBtn = new QPushButton();
        CopyButton::applyIcon(copyBtn);
        copyBtn->setFixedSize(26, 26);
        copyBtn->setCursor(Qt::PointingHandCursor);
        copyBtn->setObjectName("txaCopyBtn");
        const QString& textToCopy = address;
        QObject::connect(copyBtn, &QPushButton::clicked, copyBtn, [copyBtn, textToCopy]() {
            QApplication::clipboard()->setText(textToCopy);
            copyBtn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
            QTimer::singleShot(1500, copyBtn, [copyBtn]() { CopyButton::applyIcon(copyBtn); });
        });
        h->addWidget(copyBtn);

        // Badges from accountKeys
        if (accountKeyMap.contains(address)) {
            const auto& ak = accountKeyMap[address];
            if (ak.writable) {
                h->addWidget(makeBadge("WRITABLE", "rgba(96, 165, 250, 0.15)", "#60a5fa"));
            }
            if (ak.signer) {
                h->addWidget(makeBadge("SIGNER", "rgba(167, 139, 250, 0.15)", "#a78bfa"));
            }
            if (address == feePayer) {
                h->addWidget(makeBadge("FEE PAYER", "rgba(16, 185, 129, 0.15)", "#10b981"));
            }
        }

        // PROGRAM badge for known programs
        if (isProgram) {
            h->addWidget(makeBadge("PROGRAM", "rgba(245, 158, 11, 0.15)", "#f59e0b"));
        }

        h->addStretch();
        t->addWidget(r);
    };

    // Lambda to render a token amount row with icon + symbol
    auto tokenAmountRow = [&](QVBoxLayout* t, const QString& label, const QString& amountStr,
                              const QString& mint) {
        QWidget* w = new QWidget();
        w->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* wh = new QHBoxLayout(w);
        wh->setContentsMargins(0, 0, 0, 0);
        wh->setSpacing(6);

        // Format with commas
        bool ok = false;
        double amtVal = amountStr.toDouble(&ok);
        QString displayAmt = ok ? QLocale().toString(amtVal, 'f', amtVal >= 1 ? 2 : 6) : amountStr;
        QLabel* amtLabel = new QLabel(displayAmt);
        amtLabel->setProperty("uiClass", "txaValue");
        amtLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        wh->addWidget(amtLabel);

        KnownToken token = resolveToken(mint);
        if (!token.symbol.isEmpty()) {
            QPixmap pix;
            if (!token.iconPath.isEmpty()) {
                pix = QPixmap(token.iconPath);
            }
            if (pix.isNull() && !token.logoUrl.isEmpty() && m_avatarCache) {
                pix = m_avatarCache->get(token.logoUrl);
            }
            if (!pix.isNull()) {
                qreal dpr = qApp->devicePixelRatio();
                pix = pix.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                pix.setDevicePixelRatio(dpr);
                QLabel* icon = new QLabel();
                icon->setPixmap(pix);
                icon->setFixedSize(16, 16);
                icon->setProperty("uiClass", "txaTransparent");
                wh->addWidget(icon);
            }
            QLabel* sym = new QLabel(token.symbol);
            sym->setProperty("uiClass", "txaSummarySym");
            wh->addWidget(sym);
        }

        wh->addStretch();
        rowWidget(t, label, w);
    };

    // Lambda to render info fields for a parsed/unparsed instruction
    auto renderInfo = [&](QVBoxLayout* c, const Instruction& ix) {
        if (ix.isParsed()) {
            const QList<TxInstructionField> fields = m_handler->instructionFields(ix);
            for (const TxInstructionField& field : fields) {
                sep(c);
                switch (field.kind) {
                    case TxInstructionValueKind::Address:
                        addressRow(c, field.label, field.value);
                        break;
                    case TxInstructionValueKind::TokenAmount:
                        tokenAmountRow(c, field.label, field.value, field.mint);
                        break;
                    case TxInstructionValueKind::Text:
                    default:
                        row(c, field.label, field.value, "txaValue");
                        break;
                }
            }
        } else {
            // Unparsed instruction — show raw data + accounts
            if (!ix.data.isEmpty()) {
                sep(c);
                QString dataStr = ix.data;
                if (dataStr.length() > 80) {
                    dataStr = dataStr.left(77) + "...";
                }
                row(c, tr("Data"), dataStr, "txaMonoValue");
            }
            if (!ix.accounts.isEmpty()) {
                for (int a = 0; a < ix.accounts.size(); ++a) {
                    sep(c);
                    addressRow(c, tr("Account #%1").arg(a + 1), ix.accounts[a].toString());
                }
            }
        }
    };

    // ── Build instruction cards (accordion) ─────────────────
    // Shared state: list of (body widget, chevron label) pairs
    auto accordion = std::make_shared<QList<QPair<QWidget*, QLabel*>>>();

    for (int i = 0; i < decoded.message.instructions.size(); ++i) {
        const Instruction& ix = decoded.message.instructions[i];

        QWidget* card = new QWidget();
        card->setObjectName("txaCard");
        QVBoxLayout* c = new QVBoxLayout(card);
        c->setContentsMargins(24, 16, 24, 16);
        c->setSpacing(0);

        // ── Header (clickable, with chevron) ──
        QLabel* chevronLabel = nullptr;
        QString headerTitle = m_handler->friendlyProgramName(ix.programId, m_idlRegistry);
        if (ix.isParsed() && !ix.type.isEmpty()) {
            headerTitle += ": " + m_handler->formatTypeName(ix.type);
        }
        QPushButton* headerBtn =
            instrHeader(QString("#%1").arg(i + 1), headerTitle, i, &chevronLabel);
        c->addWidget(headerBtn);

        // ── Body (collapsible content) ──
        QWidget* body = new QWidget();
        body->setProperty("uiClass", "txaTransparent");
        QVBoxLayout* bl = new QVBoxLayout(body);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(0);

        sep(bl);

        // Program row
        QString progName = m_handler->friendlyProgramName(ix.programId, m_idlRegistry);
        if (progName.contains("...")) {
            row(bl, tr("Program"), ix.programId, "txaAddressValue", true);
        } else {
            row(bl, tr("Program"), progName, "txaProgramValue");
        }

        // Info fields
        renderInfo(bl, ix);

        // ── Inner instructions for this top-level instruction ──
        if (innerMap.contains(i)) {
            const auto& inners = innerMap[i];
            sep(bl);
            bl->addSpacing(8);

            QLabel* innerLabel = new QLabel(tr("INNER INSTRUCTIONS"));
            innerLabel->setProperty("uiClass", "txaInnerInstructionsLabel");
            bl->addWidget(innerLabel);
            bl->addSpacing(8);

            for (int j = 0; j < inners.size(); ++j) {
                const Instruction& inner = inners[j];

                QWidget* ic = new QWidget();
                ic->setObjectName("txaInnerCard");
                QVBoxLayout* il = new QVBoxLayout(ic);
                il->setContentsMargins(20, 12, 20, 12);
                il->setSpacing(0);

                QString innerTitle = m_handler->friendlyProgramName(inner.programId, m_idlRegistry);
                if (inner.isParsed() && !inner.type.isEmpty()) {
                    innerTitle += ": " + m_handler->formatTypeName(inner.type);
                }
                il->addWidget(instrHeader(QString("#%1.%2").arg(i + 1).arg(j + 1), innerTitle, i));

                // Program row for inner instruction
                QString innerProgName =
                    m_handler->friendlyProgramName(inner.programId, m_idlRegistry);
                if (innerProgName.contains("...")) {
                    row(il, tr("Program"), inner.programId, "txaAddressValue", true);
                } else {
                    row(il, tr("Program"), innerProgName, "txaProgramValue");
                }

                // Info fields
                renderInfo(il, inner);

                bl->addWidget(ic);
                if (j < inners.size() - 1) {
                    bl->addSpacing(8);
                }
            }
        }

        c->addWidget(body);

        // First card expanded, rest collapsed
        bool expanded = (i == 0);
        body->setVisible(expanded);
        if (chevronLabel) {
            chevronLabel->setPixmap(expanded ? chevronUp : chevronDown);
        }

        accordion->append({body, chevronLabel});

        // Connect header click to accordion toggle
        int cardIndex = i;
        QObject::connect(headerBtn, &QPushButton::clicked,
                         [accordion, cardIndex, chevronUp, chevronDown]() {
                             bool wasExpanded = (*accordion)[cardIndex].first->isVisible();
                             // Collapse all
                             for (int k = 0; k < accordion->size(); ++k) {
                                 (*accordion)[k].first->setVisible(false);
                                 if ((*accordion)[k].second) {
                                     (*accordion)[k].second->setPixmap(chevronDown);
                                 }
                             }
                             // Toggle clicked card (expand if was collapsed)
                             if (!wasExpanded) {
                                 (*accordion)[cardIndex].first->setVisible(true);
                                 if ((*accordion)[cardIndex].second) {
                                     (*accordion)[cardIndex].second->setPixmap(chevronUp);
                                 }
                             }
                         });

        m_instrLayout->addWidget(card);
    }

    m_instrLayout->addStretch();
}

// ── Populate Balances tab ───────────────────────────────────────

void TxLookupPage::populateBalances(const TransactionResponse& tx) {
    clearLayout(m_balancesLayout);
    const TxBalanceViewData viewData = m_handler->balanceViewData(tx);

    auto addSep = [](QVBoxLayout* target) {
        QFrame* sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("txaSeparator");
        sep->setFixedHeight(1);
        target->addWidget(sep);
    };

    // Helper: create a copyable address widget (truncated display + copy btn)
    auto makeCopyAddr = [this](const QString& address) -> QWidget* {
        QWidget* w = new QWidget();
        w->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);

        QString display = address.left(16) + "..." + address.right(8);
        QLabel* lbl = new QLabel(display);
        lbl->setProperty("uiClass", "txaAddressValue");
        lbl->setProperty("fullAddress", address);
        h->addWidget(lbl);
        registerAddressLabel(address, lbl);

        QPushButton* btn = new QPushButton();
        CopyButton::applyIcon(btn);
        btn->setFixedSize(26, 26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName("txaCopyBtn");
        QObject::connect(btn, &QPushButton::clicked, btn, [btn, address]() {
            QApplication::clipboard()->setText(address);
            btn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
            QTimer::singleShot(1500, btn, [btn]() { CopyButton::applyIcon(btn); });
        });
        h->addWidget(btn);
        return w;
    };

    // Helper: create a token badge (icon + symbol), right-aligned.
    // displayOverride forces a specific symbol (e.g. "SOL" for native context).
    auto makeTokenBadge = [this](const QString& mint,
                                 const QString& displayOverride = QString()) -> QWidget* {
        KnownToken token = resolveToken(mint);
        QString sym = displayOverride.isEmpty() ? token.symbol : displayOverride;
        if (sym.isEmpty()) {
            return nullptr;
        }

        QWidget* w = new QWidget();
        w->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* h = new QHBoxLayout(w);
        h->setContentsMargins(16, 0, 0, 0);
        h->setSpacing(4);

        QPixmap pix;
        if (!token.iconPath.isEmpty()) {
            pix = QPixmap(token.iconPath);
        }
        if (pix.isNull() && !token.logoUrl.isEmpty() && m_avatarCache) {
            pix = m_avatarCache->get(token.logoUrl);
        }
        if (!pix.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            pix = pix.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            pix.setDevicePixelRatio(dpr);
            QLabel* icon = new QLabel();
            icon->setPixmap(pix);
            icon->setFixedSize(16, 16);
            icon->setProperty("uiClass", "txaTransparent");
            h->addWidget(icon);
        }

        QLabel* symLbl = new QLabel(sym);
        symLbl->setProperty("uiClass", "txaSummarySym");
        h->addWidget(symLbl);
        return w;
    };

    static const int addrColW = 260;
    static const int tokenColW = 80;

    // Helper: add column header row (always 5 columns for alignment)
    auto addColumnHeaders = [&](QVBoxLayout* cl) {
        QWidget* hdr = new QWidget();
        hdr->setProperty("uiClass", "txaTransparent");
        QHBoxLayout* hh = new QHBoxLayout(hdr);
        hh->setContentsMargins(0, 8, 0, 8);
        hh->setSpacing(0);

        QLabel* addrH = new QLabel(tr("ADDRESS"));
        addrH->setProperty("uiClass", "txaColHeader");
        addrH->setFixedWidth(addrColW);
        hh->addWidget(addrH);

        QLabel* beforeH = new QLabel(tr("BEFORE"));
        beforeH->setProperty("uiClass", "txaColHeader");
        beforeH->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hh->addWidget(beforeH, 1);

        QLabel* afterH = new QLabel(tr("AFTER"));
        afterH->setProperty("uiClass", "txaColHeader");
        afterH->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hh->addWidget(afterH, 1);

        QLabel* changeH = new QLabel(tr("CHANGE"));
        changeH->setProperty("uiClass", "txaColHeader");
        changeH->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hh->addWidget(changeH, 1);

        QLabel* tokenH = new QLabel(tr("TOKEN"));
        tokenH->setProperty("uiClass", "txaColHeader");
        tokenH->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        tokenH->setFixedWidth(tokenColW);
        tokenH->setContentsMargins(16, 0, 0, 0);
        hh->addWidget(tokenH);

        cl->addWidget(hdr);
        addSep(cl);
    };

    // ── SOL Balance Changes ─────────────────────────────────
    {
        QWidget* card = new QWidget();
        card->setObjectName("txaCard");
        QVBoxLayout* cl = new QVBoxLayout(card);
        cl->setContentsMargins(24, 16, 24, 16);
        cl->setSpacing(0);

        QLabel* title = new QLabel(tr("SOL Balance Changes"));
        title->setObjectName("txaCardTitleSmall");
        cl->addWidget(title);
        cl->addSpacing(8);

        addColumnHeaders(cl);

        bool hasChanges = false;
        for (const TxBalanceViewRow& row : viewData.solRows) {
            hasChanges = true;

            QWidget* rowW = new QWidget();
            rowW->setProperty("uiClass", "txaTransparent");
            QHBoxLayout* h = new QHBoxLayout(rowW);
            h->setContentsMargins(0, 10, 0, 10);
            h->setSpacing(0);

            QWidget* addrW = makeCopyAddr(row.address);
            addrW->setFixedWidth(addrColW);
            h->addWidget(addrW);

            QLabel* preLabel = new QLabel(row.beforeText);
            preLabel->setProperty("uiClass", "txaCell");
            preLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            preLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            h->addWidget(preLabel, 1);

            QLabel* postLabel = new QLabel(row.afterText);
            postLabel->setProperty("uiClass", "txaCell");
            postLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            postLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            h->addWidget(postLabel, 1);

            QLabel* changeLabel = new QLabel(row.changeText);
            changeLabel->setProperty("uiClass", "txaChangeLabel");
            changeLabel->setProperty("tone", row.isPositiveChange ? "positive" : "negative");
            changeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            changeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            h->addWidget(changeLabel, 1);

            // SOL token badge — always show "SOL" for native balance changes
            QWidget* tokenW = makeTokenBadge(WSOL_MINT, "SOL");
            if (!tokenW) {
                tokenW = new QWidget();
                tokenW->setProperty("uiClass", "txaTransparent");
            }
            tokenW->setFixedWidth(tokenColW);
            h->addWidget(tokenW);

            cl->addWidget(rowW);
            addSep(cl);
        }

        if (!hasChanges) {
            QLabel* noChanges = new QLabel(tr("No SOL balance changes."));
            noChanges->setProperty("uiClass", "txaEmptyText");
            cl->addWidget(noChanges);
        }

        m_balancesLayout->addWidget(card);
    }

    // ── Token Balance Changes ───────────────────────────────
    if (!viewData.tokenRows.isEmpty()) {
        QWidget* card = new QWidget();
        card->setObjectName("txaCard");
        QVBoxLayout* cl = new QVBoxLayout(card);
        cl->setContentsMargins(24, 16, 24, 16);
        cl->setSpacing(0);

        QLabel* title = new QLabel(tr("Token Balance Changes"));
        title->setObjectName("txaCardTitleSmall");
        cl->addWidget(title);
        cl->addSpacing(8);

        addColumnHeaders(cl);

        for (const TxBalanceViewRow& row : viewData.tokenRows) {
            QWidget* rowW = new QWidget();
            rowW->setProperty("uiClass", "txaTransparent");
            QHBoxLayout* h = new QHBoxLayout(rowW);
            h->setContentsMargins(0, 10, 0, 10);
            h->setSpacing(0);

            QWidget* addrW = makeCopyAddr(row.address);
            addrW->setFixedWidth(addrColW);
            h->addWidget(addrW);

            QLabel* preLabel = new QLabel(row.beforeText);
            preLabel->setProperty("uiClass", "txaCell");
            preLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            preLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            h->addWidget(preLabel, 1);

            QLabel* postLabel = new QLabel(row.afterText);
            postLabel->setProperty("uiClass", "txaCell");
            postLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            postLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            h->addWidget(postLabel, 1);

            QLabel* changeLabel = new QLabel(row.changeText);
            changeLabel->setProperty("uiClass", "txaChangeLabel");
            changeLabel->setProperty("tone", row.isPositiveChange ? "positive" : "negative");
            changeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            changeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            h->addWidget(changeLabel, 1);

            QWidget* badge = makeTokenBadge(row.mint);
            if (!badge) {
                KnownToken fallback = resolveToken(row.mint);
                badge = new QWidget();
                badge->setProperty("uiClass", "txaTransparent");
                QHBoxLayout* fh = new QHBoxLayout(badge);
                fh->setContentsMargins(16, 0, 0, 0);
                QString sym =
                    fallback.symbol.isEmpty() ? row.mint.left(6) + "..." : fallback.symbol;
                QLabel* ml = new QLabel(sym);
                ml->setProperty("uiClass", "txaCell");
                ml->setProperty("fullAddress", row.mint);
                ml->installEventFilter(this);
                ml->setTextInteractionFlags(Qt::TextSelectableByMouse);
                fh->addWidget(ml);
                fh->addStretch();
            }
            badge->setFixedWidth(tokenColW);
            h->addWidget(badge);

            cl->addWidget(rowW);
            addSep(cl);
        }

        m_balancesLayout->addWidget(card);
    }

    m_balancesLayout->addStretch();
}

// ── Populate Logs tab ───────────────────────────────────────────

void TxLookupPage::populateLogs(const TransactionResponse& tx) {
    clearLayout(m_logsLayout);

    if (tx.meta.logMessages.isEmpty()) {
        QLabel* empty = new QLabel(tr("No log messages available."));
        empty->setProperty("uiClass", "txaEmptyText");
        m_logsLayout->addWidget(empty);
        m_logsLayout->addStretch();
        return;
    }

    QWidget* card = new QWidget();
    card->setObjectName("txaCard");
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(2);

    for (const QString& line : tx.meta.logMessages) {
        QLabel* lbl = new QLabel(line);
        lbl->setProperty("uiClass", "txaLogLine");
        lbl->setWordWrap(true);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cardLayout->addWidget(lbl);
    }

    m_logsLayout->addWidget(card);
    m_logsLayout->addStretch();
}

// ── Validation ──────────────────────────────────────────────────

void TxLookupPage::validateInput() {
    bool valid = TxLookupHandler::isValidSignature(m_txInput->text().trimmed());
    m_analyzeBtn->setEnabled(valid);
    m_analyzeBtn->setText(tr("Analyze Transaction"));
    m_analyzeBtn->setStyleSheet(valid ? Theme::primaryBtnStyle : Theme::primaryBtnDisabledStyle);
}

void TxLookupPage::resetAnalyzeButton() {
    m_analyzeBtn->setText(tr("Analyze Transaction"));
    bool valid = TxLookupHandler::isValidSignature(m_txInput->text().trimmed());
    m_analyzeBtn->setEnabled(valid);
    m_analyzeBtn->setStyleSheet(valid ? Theme::primaryBtnStyle : Theme::primaryBtnDisabledStyle);
}

// ── Address hover highlighting ──────────────────────────────────

void TxLookupPage::registerAddressLabel(const QString& address, QLabel* label) {
    m_addressLabels[address].append(label);
    if (label->property("fullAddress").toString().isEmpty()) {
        label->setProperty("fullAddress", address);
    }
    label->installEventFilter(this);
    label->setCursor(Qt::PointingHandCursor);

    // Resolve address book contact — show name + avatar instead of raw address
    auto* parentLayout = qobject_cast<QHBoxLayout*>(label->parentWidget()->layout());
    ContactResolver::resolveLabel(address, label, parentLayout);
}

void TxLookupPage::highlightAddress(const QString& address) {
    if (m_hoveredAddress == address) {
        return;
    }
    clearHighlight();
    m_hoveredAddress = address;

    auto it = m_addressLabels.find(address);
    if (it == m_addressLabels.end()) {
        return;
    }

    for (QLabel* lbl : it.value()) {
        lbl->setProperty("addressHover", true);
        lbl->style()->unpolish(lbl);
        lbl->style()->polish(lbl);
    }
}

void TxLookupPage::clearHighlight() {
    if (m_hoveredAddress.isEmpty()) {
        return;
    }

    auto it = m_addressLabels.find(m_hoveredAddress);
    if (it != m_addressLabels.end()) {
        for (QLabel* lbl : it.value()) {
            lbl->setProperty("addressHover", false);
            lbl->setProperty("uiClass", "txaAddressValue");
            lbl->style()->unpolish(lbl);
            lbl->style()->polish(lbl);
        }
    }
    m_hoveredAddress.clear();
}

void TxLookupPage::showCustomTooltip(const QPoint& globalPos, const QString& text) {
    if (!m_customTooltip) {
        m_customTooltip = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
        m_customTooltip->setObjectName("txaCustomTooltip");
    }
    m_customTooltip->setText(text);
    m_customTooltip->adjustSize();
    m_customTooltip->move(globalPos.x() + 12, globalPos.y() + 16);
    m_customTooltip->show();
}

void TxLookupPage::hideCustomTooltip() {
    if (m_customTooltip) {
        m_customTooltip->hide();
    }
}

bool TxLookupPage::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Enter) {
        QLabel* lbl = qobject_cast<QLabel*>(obj);
        if (lbl) {
            // Find which address this label belongs to
            for (auto it = m_addressLabels.begin(); it != m_addressLabels.end(); ++it) {
                if (it.value().contains(lbl)) {
                    highlightAddress(it.key());
                    break;
                }
            }
        }
    } else if (event->type() == QEvent::Leave) {
        clearHighlight();
        hideCustomTooltip();
    } else if (event->type() == QEvent::ToolTip) {
        QLabel* lbl = qobject_cast<QLabel*>(obj);
        if (lbl) {
            QString full = lbl->property("fullAddress").toString();
            if (!full.isEmpty()) {
                QHelpEvent* he = static_cast<QHelpEvent*>(event);
                showCustomTooltip(he->globalPos(), full);
                return true; // suppress native tooltip
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
