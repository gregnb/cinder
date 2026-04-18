#include "StakingPage.h"
#include "Theme.h"
#include "crypto/Signer.h"
#include "db/StakeAccountDb.h"
#include "db/StakeAccountRewardDb.h"
#include "db/TransactionDb.h"
#include "db/ValidatorCacheDb.h"
#include "features/staking/StakeLifecycle.h"
#include "features/staking/StakeStatusRail.h"
#include "features/staking/StakingHandler.h"
#include "services/AvatarCache.h"
#include "services/SolanaApi.h"
#include "services/ValidatorService.h"
#include "services/model/StakeAccountInfo.h"
#include "services/model/ValidatorInfo.h"
#include "tx/ProgramIds.h"
#include "tx/TxParseUtils.h"
#include "util/SolanaErrorParser.h"
#include "util/TxIconUtils.h"
#include "widgets/AddressLink.h"
#include "widgets/CardListItem.h"
#include "widgets/PaintedPanel.h"
#include "widgets/PillButtonGroup.h"
#include "widgets/StyledLineEdit.h"
#include "widgets/TabBar.h"
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

static QString formatSol(double sol) {
    if (sol >= 1000.0) {
        return QLocale(QLocale::English).toString(sol, 'f', 2) + " SOL";
    }
    if (sol >= 1.0) {
        return QString::number(sol, 'f', 4) + " SOL";
    }
    if (sol > 0) {
        return QString::number(sol, 'f', 6) + " SOL";
    }
    return "0 SOL";
}

static QString formatRewardSol(double sol) {
    if (sol >= 1000.0) {
        return QLocale(QLocale::English).toString(sol, 'f', 2) + " SOL";
    }
    if (sol >= 1.0) {
        return QString::number(sol, 'f', 4) + " SOL";
    }
    if (sol > 0) {
        return QString::number(sol, 'f', 12) + " SOL";
    }
    return "0 SOL";
}

static QString formatApy(double apy) { return QString::number(apy, 'f', 2) + "%"; }

static QString formatEstimatedSol(double sol) {
    if (sol <= 0.0) {
        return QStringLiteral("~0 SOL");
    }
    auto trimFixed = [](double value, int decimals) {
        QString text = QString::number(value, 'f', decimals);
        while (text.contains('.') && text.endsWith('0')) {
            text.chop(1);
        }
        if (text.endsWith('.')) {
            text.chop(1);
        }
        return text;
    };
    if (sol >= 1.0) {
        return QStringLiteral("~") + trimFixed(sol, 4) + QStringLiteral(" SOL");
    }
    if (sol >= 0.01) {
        return QStringLiteral("~") + trimFixed(sol, 6) + QStringLiteral(" SOL");
    }
    if (sol >= 0.000001) {
        return QStringLiteral("~") + trimFixed(sol, 10) + QStringLiteral(" SOL");
    }
    return QStringLiteral("~") + trimFixed(sol, 14) + QStringLiteral(" SOL");
}

static QString formatStake(double sol) {
    if (sol >= 1e6) {
        return QString::number(sol / 1e6, 'f', 1) + "M SOL";
    }
    if (sol >= 1e3) {
        return QString::number(sol / 1e3, 'f', 1) + "K SOL";
    }
    return QString::number(sol, 'f', 0) + " SOL";
}

static QString formatEpochValue(quint64 epoch) {
    constexpr quint64 kMaxU64 = std::numeric_limits<quint64>::max();
    return epoch >= kMaxU64 ? "--" : QString::number(epoch);
}

static QString relativeTimeString(qint64 unixSeconds) {
    if (unixSeconds <= 0) {
        return "--";
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 delta = qMax<qint64>(0, now - unixSeconds);
    if (delta < 60) {
        return QObject::tr("Just now");
    }
    if (delta < 3600) {
        return QObject::tr("%1 min ago").arg(delta / 60);
    }
    if (delta < 86400) {
        return QObject::tr("%1 hr ago").arg(delta / 3600);
    }
    return QObject::tr("%1 days ago").arg(delta / 86400);
}

static QString formatCountdown(qint64 totalSeconds) {
    totalSeconds = qMax<qint64>(0, totalSeconds);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

static QString classifyStakeTransaction(const TransactionResponse& tx,
                                        const QString& stakeAddress) {
    auto classifyInstruction = [&](const Instruction& ix) -> QString {
        if (ix.programId != SolanaPrograms::StakeProgram || !ix.isParsed()) {
            return {};
        }
        if (ix.type == "delegate" || ix.type == "initialize") {
            return QObject::tr("Stake");
        }
        if (ix.type == "deactivate") {
            return QObject::tr("Deactivate");
        }
        if (ix.type == "withdraw") {
            return QObject::tr("Withdraw");
        }
        if (ix.type == "split") {
            return QObject::tr("Split");
        }
        return QObject::tr("Stake Program");
    };

    for (const auto& ix : tx.message.instructions) {
        const QString label = classifyInstruction(ix);
        if (!label.isEmpty()) {
            return label;
        }
    }
    for (const auto& inner : tx.meta.innerInstructions) {
        for (const auto& ix : inner.instructions) {
            const QString label = classifyInstruction(ix);
            if (!label.isEmpty()) {
                return label;
            }
        }
    }

    const int idx = tx.accountIndex(stakeAddress);
    const qint64 delta = tx.solBalanceChange(idx);
    if (delta > 0) {
        return QObject::tr("Deposit");
    }
    if (delta < 0) {
        return QObject::tr("Withdraw");
    }
    return QObject::tr("Transaction");
}

// ── Constructor ──────────────────────────────────────────────────

StakingPage::StakingPage(QWidget* parent) : QWidget(parent) {
    m_stakingHandler = new StakingHandler(this);
    m_detailEtaTimer = new QTimer(this);
    m_detailEtaTimer->setInterval(1000);
    connect(m_detailEtaTimer, &QTimer::timeout, this, &StakingPage::updateStakeDetailEta);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();

    // ── Index 0: Main staking view ────────────────────────────────
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setObjectName("stakingContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Title
    QLabel* title = new QLabel(tr("Staking"));
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // Tabs
    m_tabs = new TabBar();
    m_tabs->addTab(tr("Validators"));
    m_tabs->addTab(tr("My Stakes"));

    layout->addWidget(m_tabs);

    // Status label
    m_statusLabel = new QLabel();
    m_statusLabel->setObjectName("stakingStatusLabel");
    m_statusLabel->hide();
    layout->addWidget(m_statusLabel);

    // Inner stacked widget for tab content
    layout->addSpacing(8);
    QStackedWidget* tabStack = new QStackedWidget();
    m_validatorPage = buildValidatorBrowser();
    m_myStakesPage = buildMyStakes();
    tabStack->addWidget(m_validatorPage); // 0
    tabStack->addWidget(m_myStakesPage);  // 1

    connect(m_tabs, &TabBar::currentChanged, tabStack, &QStackedWidget::setCurrentIndex);
    m_tabs->setActiveIndex(0);

    layout->addWidget(tabStack, 1);

    scroll->setWidget(content);

    scroll->viewport()->setObjectName("stakingViewport");
    scroll->viewport()->setAttribute(Qt::WA_StyledBackground, true);

    m_stack->addWidget(scroll);             // Step::Main
    m_stack->addWidget(buildStakeForm());   // Step::StakeForm
    m_stack->addWidget(buildStakeDetail()); // Step::StakeDetail

    outerLayout->addWidget(m_stack);

    connect(m_stakingHandler, &StakingHandler::actionUpdated, this,
            [this](const StakingActionUpdate& update) {
                auto setFormStatus = [this](const QString& text, const QString& tone) {
                    if (!m_formStatus) {
                        return;
                    }
                    m_formStatus->setText(text);
                    m_formStatus->setProperty("tone", tone);
                    m_formStatus->style()->unpolish(m_formStatus);
                    m_formStatus->style()->polish(m_formStatus);
                };

                auto setStatusLabel = [this](const QString& text, const QString& tone) {
                    if (!m_statusLabel) {
                        return;
                    }
                    m_statusLabel->setText(text);
                    m_statusLabel->setProperty("tone", tone);
                    m_statusLabel->style()->unpolish(m_statusLabel);
                    m_statusLabel->style()->polish(m_statusLabel);
                    m_statusLabel->show();
                };

                auto setStakeEnabled = [this](bool enabled) {
                    if (!m_stakeBtn) {
                        return;
                    }
                    m_stakeBtn->setEnabled(enabled);
                };

                QString displayError = update.errorMessage;
                if (update.errorCode == "rpc_send_failed" && !update.errorMessage.isEmpty()) {
                    displayError = SolanaErrorParser::humanize(update.errorMessage);
                } else if (update.errorCode == "wallet_locked") {
                    displayError = tr("Wallet is locked. Please unlock first.");
                } else if (update.errorCode == "build_failed") {
                    displayError = tr("Failed to build transaction. Please try again.");
                } else if (update.errorCode == "sign_failed" && displayError.isEmpty()) {
                    displayError = tr("Signing was cancelled or failed.");
                }
                if (displayError.isEmpty()) {
                    displayError = tr("Transaction failed. Please try again.");
                }

                switch (update.action) {
                    case StakingAction::Stake:
                        switch (update.phase) {
                            case StakingActionPhase::Building:
                                setStakeEnabled(false);
                                setFormStatus(tr("Creating stake account..."), "muted");
                                break;
                            case StakingActionPhase::Sending:
                                setFormStatus(tr("Sending transaction..."), "muted");
                                break;
                            case StakingActionPhase::Submitted:
                                setFormStatus(tr("Stake submitted! TX: %1\nStake account: %2")
                                                  .arg(update.txSignature.left(16) + "...",
                                                       update.stakeAccount),
                                              "success");
                                if (!update.stakeAccount.isEmpty()) {
                                    StakeAccountInfo optimistic;
                                    optimistic.address = update.stakeAccount;
                                    optimistic.lamports = m_rentExempt;
                                    optimistic.rentExemptReserve = m_rentExempt;
                                    optimistic.staker = m_walletAddress;
                                    optimistic.withdrawer = m_walletAddress;
                                    optimistic.voteAccount = m_selectedValidator.voteAccount;
                                    optimistic.stake = 0;
                                    optimistic.activationEpoch = m_currentEpoch + 1;
                                    optimistic.deactivationEpoch =
                                        std::numeric_limits<quint64>::max();
                                    optimistic.state = StakeAccountInfo::State::Activating;
                                    m_pendingStakeAccounts.insert(optimistic.address, optimistic);

                                    bool replacedExisting = false;
                                    for (auto& sa : m_stakeAccounts) {
                                        if (sa.address == optimistic.address) {
                                            sa = optimistic;
                                            replacedExisting = true;
                                            break;
                                        }
                                    }
                                    if (!replacedExisting) {
                                        m_stakeAccounts.prepend(optimistic);
                                    }

                                    StakeAccountDb::save(m_walletAddress, m_stakeAccounts);
                                    populateMyStakes();
                                    openStakeDetail(optimistic.address);
                                }
                                break;
                            case StakingActionPhase::Failed:
                                setFormStatus(displayError, "error");
                                setStakeEnabled(true);
                                break;
                        }
                        break;
                    case StakingAction::Deactivate:
                        if (update.phase == StakingActionPhase::Submitted) {
                            setStatusLabel(
                                tr("Unstake submitted! Deactivation takes ~2-3 days (1 epoch)."),
                                "success");
                        } else if (update.phase == StakingActionPhase::Failed) {
                            setStatusLabel(displayError, "error");
                        }
                        break;
                    case StakingAction::Withdraw:
                        if (update.phase == StakingActionPhase::Submitted) {
                            setStatusLabel(tr("Withdrawal submitted!"), "success");
                        } else if (update.phase == StakingActionPhase::Failed) {
                            setStatusLabel(displayError, "error");
                        }
                        break;
                }
            });
    connect(m_stakingHandler, &StakingHandler::refreshRequested, this,
            [this](const StakingRefreshRequest& request) {
                QTimer::singleShot(request.delayMs, this, [this, request]() {
                    if (m_walletAddress.isEmpty() || !m_solanaApi) {
                        return;
                    }
                    if (request.refreshStakeAccounts) {
                        m_solanaApi->fetchStakeAccounts(m_walletAddress);
                    }
                    if (request.refreshBalance) {
                        m_solanaApi->fetchBalance(m_walletAddress);
                    }
                });
            });
}

void StakingPage::showStep(Step step) { m_stack->setCurrentIndex(static_cast<int>(step)); }

// ── Validator Browser ────────────────────────────────────────────

QWidget* StakingPage::buildValidatorBrowser() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Summary text (no card)
    m_summaryLabel = new QLabel(tr("Loading validator data..."));
    m_summaryLabel->setObjectName("stakingSummaryLabel");
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    // Search
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText(tr("Search validators..."));
    m_searchInput->setMinimumHeight(38);
    m_searchInput->setObjectName("stakingSearchInput");
    QPalette searchPal = m_searchInput->palette();
    searchPal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_searchInput->setPalette(searchPal);
    connect(m_searchInput, &QLineEdit::textChanged, this, &StakingPage::rebuildFilteredList);
    layout->addWidget(m_searchInput);

    // Column headers (clickable for sorting) — matches Activity page style
    // Must match data row layout: margins(10,_,10,_), spacing 8, icon 36px fixed
    QWidget* header = new QWidget();
    header->setObjectName("stakingTransparent");
    QHBoxLayout* hdr = new QHBoxLayout(header);
    hdr->setContentsMargins(10, 0, 10, 0);
    hdr->setSpacing(8);

    QPixmap sortPx = txTypeIcon("sort", 12, devicePixelRatioF(), QColor(255, 255, 255, 100));
    int colIndex = 0;

    auto makeHdrCol = [&](const QString& text, int stretch, Qt::Alignment align) {
        QWidget* col = new QWidget();
        col->setObjectName("stakingTransparent");
        QHBoxLayout* colLay = new QHBoxLayout(col);
        colLay->setContentsMargins(0, 0, 0, 0);
        colLay->setSpacing(3);

        QPushButton* lbl = new QPushButton(text);
        lbl->setObjectName("stakingHeaderBtn");
        lbl->setProperty("activeSort", false);
        lbl->setCursor(Qt::PointingHandCursor);
        int ci = colIndex;
        connect(lbl, &QPushButton::clicked, this,
                [this, ci]() { sortValidators(static_cast<SortColumn>(ci)); });

        QPushButton* sortBtn = new QPushButton();
        sortBtn->setIcon(QIcon(sortPx));
        sortBtn->setIconSize(QSize(12, 12));
        sortBtn->setCursor(Qt::PointingHandCursor);
        sortBtn->setObjectName("stakingSortBtn");
        sortBtn->setFixedSize(16, 20);
        connect(sortBtn, &QPushButton::clicked, this,
                [this, ci]() { sortValidators(static_cast<SortColumn>(ci)); });
        m_sortBtns.append(sortBtn);
        m_headerLabels.append(lbl);

        if (align.testFlag(Qt::AlignRight)) {
            colLay->addStretch();
            colLay->addWidget(lbl);
            colLay->addWidget(sortBtn);
        } else {
            colLay->addWidget(lbl);
            colLay->addWidget(sortBtn);
            colLay->addStretch();
        }

        hdr->addWidget(col, stretch);
        ++colIndex;
    };

    // 36px spacer matches the 36px icon in data rows
    QWidget* iconSpacer = new QWidget();
    iconSpacer->setFixedWidth(36);
    iconSpacer->setObjectName("stakingTransparent");
    hdr->addWidget(iconSpacer);

    makeHdrCol(tr("VALIDATOR"), 4, Qt::AlignLeft);
    makeHdrCol(tr("STAKE"), 2, Qt::AlignRight);
    makeHdrCol(tr("APY"), 1, Qt::AlignRight);
    makeHdrCol(tr("COMM"), 1, Qt::AlignRight);
    makeHdrCol(tr("VERSION"), 1, Qt::AlignRight);

    layout->addWidget(header);
    updateHeaderLabels();

    // Virtual validator list: scroll area with absolutely-positioned pool rows
    m_validatorScroll = new QScrollArea();
    m_validatorScroll->setWidgetResizable(true);
    m_validatorScroll->setFrameShape(QFrame::NoFrame);
    m_validatorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_validatorScroll->viewport()->setObjectName("stakingViewport");
    m_validatorScroll->viewport()->setAttribute(Qt::WA_StyledBackground, true);

    m_validatorContainer = new QWidget();
    m_validatorContainer->setObjectName("stakingViewport");
    m_validatorContainer->setAttribute(Qt::WA_StyledBackground, true);
    m_validatorScroll->setWidget(m_validatorContainer);

    // Pre-create the row pool
    for (int i = 0; i < POOL_SIZE; ++i) {
        QWidget* row = createPoolRow();
        row->setParent(m_validatorContainer);
        row->setVisible(false);
        m_rowPool.append(row);
    }

    connect(m_validatorScroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &StakingPage::relayoutVisibleRows);

    // Also relayout on viewport resize (window resize, layout changes)
    m_validatorScroll->viewport()->installEventFilter(this);

    // "No match" label (hidden by default)
    m_noMatchLabel = new QLabel(tr("No validators match your search."));
    m_noMatchLabel->setObjectName("stakingNoMatchLabel");
    m_noMatchLabel->setAlignment(Qt::AlignCenter);
    m_noMatchLabel->hide();

    layout->addWidget(m_validatorScroll, 1);
    layout->addWidget(m_noMatchLabel);

    return page;
}

// ── My Stakes ────────────────────────────────────────────────────

QWidget* StakingPage::buildMyStakes() {
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Summary text (no card)
    m_stakeSummaryLabel = new QLabel(tr("No stake accounts found."));
    m_stakeSummaryLabel->setObjectName("stakingSummaryLabel");
    m_stakeSummaryLabel->setWordWrap(true);
    layout->addWidget(m_stakeSummaryLabel);

    // Stake account list
    m_myStakesLayout = new QVBoxLayout();
    m_myStakesLayout->setSpacing(2);
    layout->addLayout(m_myStakesLayout);

    layout->addStretch();
    return page;
}

// ── Stake Form ───────────────────────────────────────────────────

QWidget* StakingPage::buildStakeForm() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::Main); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    QLabel* formTitle = new QLabel(tr("Stake SOL"));
    formTitle->setObjectName("stakingFormTitle");
    layout->addWidget(formTitle);

    QWidget* infoCard = new QWidget();
    infoCard->setObjectName("validatorInfoCard");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(20, 16, 20, 16);
    infoLayout->setSpacing(10);

    // Header: icon + name
    QHBoxLayout* headerRow = new QHBoxLayout();
    headerRow->setSpacing(12);
    m_formIcon = new QLabel();
    m_formIcon->setFixedSize(44, 44);
    m_formIcon->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(m_formIcon);

    m_formValidatorName = new QLabel();
    m_formValidatorName->setObjectName("stakingFormValidatorName");
    m_formValidatorName->setWordWrap(true);
    headerRow->addWidget(m_formValidatorName, 1);
    infoLayout->addLayout(headerRow);

    // APY + Commission row
    QHBoxLayout* statsRow = new QHBoxLayout();
    m_formApy = new QLabel();
    m_formApy->setObjectName("stakingFormApy");
    statsRow->addWidget(m_formApy);

    m_formCommission = new QLabel();
    m_formCommission->setObjectName("stakingFormCommission");
    statsRow->addWidget(m_formCommission);
    statsRow->addStretch();
    infoLayout->addLayout(statsRow);

    // Separator
    QFrame* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("stakingSeparator");
    infoLayout->addWidget(sep);

    // Detail grid: 2 columns of label/value pairs
    auto addDetailRow = [&](const QString& label1, QLabel*& val1, const QString& label2,
                            QLabel*& val2) {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(16);

        QVBoxLayout* col1 = new QVBoxLayout();
        col1->setSpacing(2);
        QLabel* lbl1 = new QLabel(label1);
        lbl1->setObjectName("stakingDetailLabel");
        col1->addWidget(lbl1);
        val1 = new QLabel("--");
        val1->setObjectName("stakingDetailValue");
        val1->setTextInteractionFlags(Qt::TextSelectableByMouse);
        col1->addWidget(val1);
        row->addLayout(col1, 1);

        QVBoxLayout* col2 = new QVBoxLayout();
        col2->setSpacing(2);
        QLabel* lbl2 = new QLabel(label2);
        lbl2->setObjectName("stakingDetailLabel");
        col2->addWidget(lbl2);
        val2 = new QLabel("--");
        val2->setObjectName("stakingDetailValue");
        val2->setTextInteractionFlags(Qt::TextSelectableByMouse);
        col2->addWidget(val2);
        row->addLayout(col2, 1);

        infoLayout->addLayout(row);
    };

    addDetailRow(tr("Active Stake"), m_formStake, tr("Uptime"), m_formUptime);
    addDetailRow(tr("Version"), m_formVersion, tr("Score"), m_formScore);

    // Location (single full-width row)
    QLabel* locLabel = new QLabel(tr("Location"));
    locLabel->setObjectName("stakingDetailLabel");
    infoLayout->addWidget(locLabel);
    m_formLocation = new QLabel("--");
    m_formLocation->setObjectName("stakingDetailValue");
    infoLayout->addWidget(m_formLocation);

    // Vote account (full width, copyable)
    QLabel* voteLabel = new QLabel(tr("Vote Account"));
    voteLabel->setObjectName("stakingDetailLabel");
    infoLayout->addWidget(voteLabel);
    m_formVoteAccount = new AddressLink({});
    infoLayout->addWidget(m_formVoteAccount);

    // Node pubkey (full width, copyable)
    QLabel* nodeLabel = new QLabel(tr("Node Identity"));
    nodeLabel->setObjectName("stakingDetailLabel");
    infoLayout->addWidget(nodeLabel);
    m_formNodePubkey = new AddressLink({});
    infoLayout->addWidget(m_formNodePubkey);

    layout->addWidget(infoCard);

    layout->addSpacing(8);

    // Amount input
    QLabel* amountLabel = new QLabel(tr("Amount (SOL)"));
    amountLabel->setObjectName("stakingAmountLabel");
    layout->addWidget(amountLabel);

    // Amount wrapper: QLineEdit + Max button in one styled container
    PaintedPanel* amtWrapper = new PaintedPanel();
    amtWrapper->setFillColor(QColor(30, 31, 55, 204));
    amtWrapper->setBorderColor(QColor(100, 100, 150, 77));
    amtWrapper->setCornerRadius(12.0);
    amtWrapper->setBorderWidth(1.0);
    amtWrapper->setMinimumHeight(44);

    QHBoxLayout* wrapLayout = new QHBoxLayout(amtWrapper);
    wrapLayout->setContentsMargins(12, 0, 4, 0);
    wrapLayout->setSpacing(6);

    auto* amountInput = new StyledLineEdit();
    m_amountInput = amountInput;
    m_amountInput->setPlaceholderText("0.00");
    m_amountInput->setMinimumHeight(40);
    amountInput->setFrameFillColor(Qt::transparent);
    amountInput->setFrameBorderColor(Qt::transparent);
    amountInput->setFrameFocusBorderColor(Qt::transparent);
    amountInput->setFrameBorderWidth(0.0);
    amountInput->setTextMargins(0, 0, 0, 0);
    QPalette amtPal = m_amountInput->palette();
    amtPal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 60));
    m_amountInput->setPalette(amtPal);
    wrapLayout->addWidget(m_amountInput, 1);

    QPushButton* maxBtn = new QPushButton(tr("Max"));
    maxBtn->setCursor(Qt::PointingHandCursor);
    maxBtn->setObjectName("stakingMaxBtn");
    maxBtn->setFixedHeight(28);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        quint64 available = (m_solBalance > m_rentExempt) ? m_solBalance - m_rentExempt : 0;
        if (available > 0) {
            double maxSol = static_cast<double>(available) / 1e9;
            m_amountInput->setText(QString::number(maxSol, 'f', 9));
        }
    });
    wrapLayout->addWidget(maxBtn);

    layout->addWidget(amtWrapper);

    // Fee summary
    m_formStatus = new QLabel();
    m_formStatus->setObjectName("stakingFormStatus");
    m_formStatus->setWordWrap(true);
    layout->addWidget(m_formStatus);

    layout->addSpacing(8);

    // Stake button — starts disabled until amount is entered
    m_stakeBtn = new QPushButton(tr("Stake SOL"));
    m_stakeBtn->setObjectName("stakingStakeBtn");
    m_stakeBtn->setMinimumHeight(50);
    m_stakeBtn->setCursor(Qt::PointingHandCursor);
    m_stakeBtn->setEnabled(false);
    connect(m_amountInput, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok = false;
        double val = text.toDouble(&ok);
        bool valid = ok && val > 0;
        m_stakeBtn->setEnabled(valid);
    });
    connect(m_stakeBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        double sol = m_amountInput->text().toDouble(&ok);
        if (!ok || sol <= 0) {
            m_formStatus->setText(tr("Enter a valid amount."));
            m_formStatus->setProperty("tone", "error");
            m_formStatus->style()->unpolish(m_formStatus);
            m_formStatus->style()->polish(m_formStatus);
            return;
        }
        auto lamports = static_cast<quint64>(sol * 1e9);
        doStake(m_selectedValidator.voteAccount, lamports);
    });
    layout->addWidget(m_stakeBtn);

    layout->addStretch();

    scroll->setWidget(content);

    scroll->viewport()->setProperty("uiClass", "contentViewport");

    return scroll;
}

QWidget* StakingPage::buildStakeDetail() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget* content = new QWidget();
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::Main); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    QWidget* headerCard = new QWidget();
    headerCard->setObjectName("validatorInfoCard");
    QVBoxLayout* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(10);

    m_detailTitle = new QLabel(tr("Stake Position"));
    m_detailTitle->setObjectName("stakingFormTitle");
    m_detailAddress = new QLabel();
    m_detailAddress->setObjectName("stakingSummaryLabel");
    m_detailAddress->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* identityRow = new QHBoxLayout();
    identityRow->setContentsMargins(0, 0, 0, 0);
    identityRow->setSpacing(12);
    identityRow->addWidget(m_detailTitle, 0, Qt::AlignVCenter);
    identityRow->addWidget(m_detailAddress, 1, Qt::AlignVCenter);
    headerLayout->addLayout(identityRow);

    m_detailLifecycle = new StakeStatusRail();
    headerLayout->addWidget(m_detailLifecycle);

    m_detailLockup = new QLabel();
    m_detailLockup->setObjectName("stakingStatusLabel");
    m_detailLockup->hide();
    headerLayout->addWidget(m_detailLockup);

    layout->addWidget(headerCard);

    QWidget* cardsRow = new QWidget();
    cardsRow->setObjectName("stakingTransparent");
    QHBoxLayout* cardsLayout = new QHBoxLayout(cardsRow);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(16);

    auto makeDetailCard = [](const QString& title, QLabel*& slot) -> QWidget* {
        QWidget* card = new QWidget();
        card->setObjectName("validatorInfoCard");
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 18, 20, 18);
        cardLayout->setSpacing(12);

        QLabel* cardTitle = new QLabel(title);
        cardTitle->setObjectName("stakingFormValidatorName");
        cardLayout->addWidget(cardTitle);

        slot = nullptr;
        return card;
    };

    QWidget* overviewCard = makeDetailCard(tr("Overview"), m_detailTotalValue);
    QVBoxLayout* overviewLayout = qobject_cast<QVBoxLayout*>(overviewCard->layout());

    auto addValueRow = [](QVBoxLayout* parent, const QString& label, QLabel*& value) {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(16);
        QLabel* left = new QLabel(label);
        left->setObjectName("stakingDetailLabel");
        left->setMinimumWidth(160);
        row->addWidget(left);

        value = new QLabel("--");
        value->setObjectName("stakingDetailValue");
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(value, 1);
        parent->addLayout(row);
    };

    addValueRow(overviewLayout, tr("Total Balance"), m_detailTotalValue);
    addValueRow(overviewLayout, tr("Delegated Stake"), m_detailDelegatedStake);
    addValueRow(overviewLayout, tr("Active Stake"), m_detailActiveStake);
    addValueRow(overviewLayout, tr("Inactive Stake"), m_detailInactiveStake);
    addValueRow(overviewLayout, tr("Total Rewards"), m_detailTotalRewards);
    addValueRow(overviewLayout, tr("Rent Reserve"), m_detailRentReserve);

    QWidget* infoCard = makeDetailCard(tr("More Info"), m_detailValidator);
    QVBoxLayout* infoLayout = qobject_cast<QVBoxLayout*>(infoCard->layout());
    addValueRow(infoLayout, tr("Validator"), m_detailValidator);
    addValueRow(infoLayout, tr("Type"), m_detailType);

    auto addAddressRow = [](QVBoxLayout* parent, const QString& label, AddressLink*& value) {
        QLabel* left = new QLabel(label);
        left->setObjectName("stakingDetailLabel");
        parent->addWidget(left);
        value = new AddressLink({});
        parent->addWidget(value);
    };

    addAddressRow(infoLayout, tr("Vote Account"), m_detailVoteAccount);
    addAddressRow(infoLayout, tr("Withdraw Authority"), m_detailWithdrawAuthority);
    addAddressRow(infoLayout, tr("Stake Authority"), m_detailStakeAuthority);
    addValueRow(infoLayout, tr("Activation Epoch"), m_detailActivationEpoch);
    addValueRow(infoLayout, tr("Deactivation Epoch"), m_detailDeactivationEpoch);
    addValueRow(infoLayout, tr("Allocated Data Size"), m_detailAllocatedSize);
    addValueRow(infoLayout, tr("Lockup Custodian"), m_detailCustodian);

    cardsLayout->addWidget(overviewCard, 1);
    cardsLayout->addWidget(infoCard, 1);
    layout->addWidget(cardsRow);

    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(12);
    actionsLayout->addStretch();

    m_detailPrimaryAction = new QPushButton();
    m_detailPrimaryAction->setObjectName("stakingStakeBtn");
    m_detailPrimaryAction->setMinimumHeight(46);
    m_detailPrimaryAction->setCursor(Qt::PointingHandCursor);
    connect(m_detailPrimaryAction, &QPushButton::clicked, this, [this]() {
        if (m_selectedStakeAccount.address.isEmpty()) {
            return;
        }
        const StakeLifecycle lifecycle =
            StakeLifecycle::derive(m_selectedStakeAccount, m_currentEpoch);
        if (lifecycle.canUnstake) {
            doDeactivate(m_selectedStakeAccount.address);
            return;
        }
        if (lifecycle.canWithdraw) {
            doWithdraw(m_selectedStakeAccount.address, m_selectedStakeAccount.lamports);
        }
    });
    actionsLayout->addWidget(m_detailPrimaryAction);
    layout->addLayout(actionsLayout);

    m_detailActivityTabs = new PillButtonGroup();
    m_detailActivityTabs->setObjectNames("speedButton", "speedButtonActive");
    m_detailActivityTabs->addButton(tr("Transactions"));
    m_detailActivityTabs->addButton(tr("Rewards"));
    m_detailActivityTabs->setActiveIndex(0);
    layout->addWidget(m_detailActivityTabs, 0, Qt::AlignLeft);

    m_detailActivityStack = new QStackedWidget();
    m_detailActivityStack->setObjectName("stakingTransparent");
    m_detailActivityStack->setAttribute(Qt::WA_StyledBackground, true);
    layout->addWidget(m_detailActivityStack);

    QWidget* transactionsPage = new QWidget();
    transactionsPage->setObjectName("stakingTransparent");
    transactionsPage->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* transactionsLayout = new QVBoxLayout(transactionsPage);
    transactionsLayout->setContentsMargins(0, 0, 0, 0);
    transactionsLayout->setSpacing(12);

    m_detailTransactionsStatus = new QLabel(tr("No transactions loaded."));
    m_detailTransactionsStatus->setObjectName("stakingStatusLabel");
    transactionsLayout->addWidget(m_detailTransactionsStatus);

    m_detailTransactionsLayout = new QVBoxLayout();
    m_detailTransactionsLayout->setSpacing(4);
    transactionsLayout->addLayout(m_detailTransactionsLayout);
    transactionsLayout->addStretch();
    m_detailActivityStack->addWidget(transactionsPage);

    QWidget* rewardsPage = new QWidget();
    rewardsPage->setObjectName("stakingTransparent");
    rewardsPage->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* rewardsLayout = new QVBoxLayout(rewardsPage);
    rewardsLayout->setContentsMargins(0, 0, 0, 0);
    rewardsLayout->setSpacing(12);

    m_detailRewardsStatus = new QLabel(tr("No rewards loaded."));
    m_detailRewardsStatus->setObjectName("stakingRewardsEmptyLabel");
    m_detailRewardsStatus->setAlignment(Qt::AlignCenter);
    m_detailRewardsStatus->setMinimumHeight(360);
    rewardsLayout->addWidget(m_detailRewardsStatus);

    m_detailRewardsLayout = new QVBoxLayout();
    m_detailRewardsLayout->setSpacing(4);
    rewardsLayout->addLayout(m_detailRewardsLayout);
    rewardsLayout->addStretch();
    m_detailActivityStack->addWidget(rewardsPage);

    connect(m_detailActivityTabs, &PillButtonGroup::currentChanged, m_detailActivityStack,
            &QStackedWidget::setCurrentIndex);
    layout->addStretch();

    scroll->setWidget(content);
    scroll->viewport()->setProperty("uiClass", "contentViewport");
    m_stakeDetailPage = scroll;
    return scroll;
}

// ── Setters ──────────────────────────────────────────────────────

void StakingPage::setSolanaApi(SolanaApi* api) {
    m_solanaApi = api;
    m_stakingHandler->setSolanaApi(api);
    m_validatorService = new ValidatorService(api, this);

    connect(m_validatorService, &ValidatorService::validatorsReady, this,
            [this](const QList<ValidatorInfo>& validators) {
                m_validators = validators;
                m_currentEpoch = m_validatorService->currentEpoch();

                // Compute network average APY
                double totalApy = 0;
                int apyCount = 0;
                for (const auto& v : m_validators) {
                    if (!v.delinquent && v.apy > 0) {
                        totalApy += v.apy;
                        apyCount++;
                    }
                }
                double avgApy = apyCount > 0 ? totalApy / apyCount : 0;

                double totalStakedSol = 0;
                for (const auto& v : m_validators) {
                    totalStakedSol += v.stakeInSol();
                }

                m_summaryLabel->setText(
                    tr("Network: %1 validators | Avg APY: %2 | Total Staked: %3")
                        .arg(m_validators.size())
                        .arg(formatApy(avgApy), formatStake(totalStakedSol)));

                rebuildFilteredList(m_searchInput ? m_searchInput->text() : QString());
                m_statusLabel->hide();

                // Prefetch avatar images for validators that have URLs
                if (m_avatarCache) {
                    QStringList urls;
                    for (const auto& v : m_validators) {
                        if (!v.avatarUrl.isEmpty()) {
                            urls.append(v.avatarUrl);
                        }
                    }
                    m_avatarCache->prefetch(urls);
                }

                // Re-emit staking summary now that validator names/avatars are resolved
                if (!m_stakeAccounts.isEmpty()) {
                    populateMyStakes();
                }
            });

    connect(m_validatorService, &ValidatorService::error, this, [this](const QString& msg) {
        m_statusLabel->setText(tr("Error: %1").arg(msg));
        m_statusLabel->setProperty("tone", "error");
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
        m_statusLabel->show();
    });

    connect(m_solanaApi, &SolanaApi::signaturesReady, this,
            [this](const QString& address, const QList<SignatureInfo>& signatures) {
                if (address != m_selectedStakeAccount.address) {
                    return;
                }
                m_detailSignatures = signatures.mid(0, 10);
                m_detailFailedTransactions.clear();

                for (const auto& sig : m_detailSignatures) {
                    const QString rawJson = TransactionDb::getRawJson(sig.signature);
                    if (rawJson.isEmpty()) {
                        continue;
                    }
                    const QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
                    if (!doc.isObject()) {
                        continue;
                    }
                    if (!m_detailTransactions.contains(sig.signature)) {
                        m_detailTransactions.insert(sig.signature,
                                                    TransactionResponse::fromJson(doc.object()));
                    }
                }

                renderStakeDetailTransactions();
                renderStakeDetailRewards();
                for (const auto& sig : m_detailSignatures) {
                    if (m_detailTransactions.contains(sig.signature)) {
                        continue;
                    }
                    m_solanaApi->fetchTransaction(sig.signature);
                }
            });

    connect(m_solanaApi, &SolanaApi::transactionReady, this,
            [this](const QString& signature, const TransactionResponse& tx) {
                bool relevant = false;
                for (const auto& sig : std::as_const(m_detailSignatures)) {
                    if (sig.signature == signature) {
                        relevant = true;
                        break;
                    }
                }
                if (!relevant) {
                    return;
                }
                const QString rawJson = QJsonDocument(tx.rawJson).toJson(QJsonDocument::Compact);
                const QList<Activity> activities =
                    m_walletAddress.isEmpty()
                        ? QList<Activity>{}
                        : TxParseUtils::extractActivities(tx, m_walletAddress);
                TransactionDb::insertTransaction(signature, tx.slot, tx.blockTime, rawJson,
                                                 static_cast<int>(tx.meta.fee), tx.meta.hasError,
                                                 activities);
                m_detailTransactions.insert(signature, tx);
                m_detailFailedTransactions.remove(signature);
                refreshStakeDetailRewardTotal();
                renderStakeDetailTransactions();
                renderStakeDetailRewards();
            });

    connect(
        m_solanaApi, &SolanaApi::epochInfoReady, this,
        [this](quint64 epoch, quint64 slotIndex, quint64 slotsInEpoch, quint64 /*absoluteSlot*/) {
            m_currentEpoch = epoch;
            m_epochSlotIndex = slotIndex;
            m_slotsInEpoch = slotsInEpoch;
            m_epochEtaTargetSecs = 0;
            invalidateStakeDetailRewardEstimate();
            if (m_stack && m_stack->currentWidget() == m_stakeDetailPage &&
                !m_selectedStakeAccount.address.isEmpty()) {
                requestStakeDetailRewards();
            }
            updateStakeDetailEta();
        });

    connect(m_solanaApi, &SolanaApi::inflationRewardReady, this,
            [this](const QString& address, quint64 epoch, bool found, qint64 amount,
                   quint64 postBalance, quint64 effectiveSlot, int commission) {
                if (address != m_selectedStakeAccount.address) {
                    return;
                }

                m_detailPendingRewardEpochs.remove(epoch);
                if (found && amount > 0) {
                    StakeRewardInfo reward;
                    reward.epoch = epoch;
                    reward.lamports = amount;
                    reward.postBalance = postBalance;
                    reward.effectiveSlot = effectiveSlot;
                    reward.commission = commission;
                    m_detailEpochRewards.insert(epoch, reward);
                    StakeAccountRewardDb::upsert(address, reward);
                }

                refreshStakeDetailRewardTotal();
                renderStakeDetailRewards();
            });

    connect(m_solanaApi, &SolanaApi::performanceSamplesReady, this,
            [this](const QJsonArray& samples) {
                if (samples.isEmpty()) {
                    return;
                }
                double totalSecsPerSlot = 0.0;
                int count = 0;
                for (const auto& value : samples) {
                    const QJsonObject sample = value.toObject();
                    const double numSlots = sample["numSlots"].toDouble();
                    const double samplePeriodSecs = sample["samplePeriodSecs"].toDouble();
                    if (numSlots <= 0.0 || samplePeriodSecs <= 0.0) {
                        continue;
                    }
                    totalSecsPerSlot += samplePeriodSecs / numSlots;
                    count++;
                }
                if (count > 0) {
                    m_avgSecondsPerSlot = totalSecsPerSlot / count;
                    m_epochEtaTargetSecs = 0;
                }
                updateStakeDetailEta();
            });

    connect(m_solanaApi, &SolanaApi::requestFailed, this,
            [this](const QString& method, const QString& error) {
                if (!m_stakeDetailPage || m_stack->currentWidget() != m_stakeDetailPage) {
                    return;
                }
                if (method == "getSignaturesForAddress" && m_detailTransactionsStatus) {
                    m_detailTransactionsStatus->setText(
                        tr("Failed to load transactions: %1").arg(error));
                    m_detailTransactionsStatus->setProperty("tone", "error");
                    m_detailTransactionsStatus->style()->unpolish(m_detailTransactionsStatus);
                    m_detailTransactionsStatus->style()->polish(m_detailTransactionsStatus);
                    return;
                }
                if (method == "getTransaction") {
                    for (const auto& sig : std::as_const(m_detailSignatures)) {
                        if (!m_detailTransactions.contains(sig.signature) &&
                            !m_detailFailedTransactions.contains(sig.signature)) {
                            m_detailFailedTransactions.insert(sig.signature);
                            break;
                        }
                    }
                    renderStakeDetailTransactions();
                    renderStakeDetailRewards();
                    return;
                }
                if (method == "getInflationReward" && !m_detailPendingRewardEpochs.isEmpty()) {
                    auto it = m_detailPendingRewardEpochs.begin();
                    m_detailPendingRewardEpochs.erase(it);
                    refreshStakeDetailRewardTotal();
                    renderStakeDetailRewards();
                }
            });
}

void StakingPage::setKeypair(const Keypair& kp) { m_keypair = kp; }
void StakingPage::setSigner(Signer* signer) {
    m_signer = signer;
    m_stakingHandler->setSigner(signer);
}

void StakingPage::setWalletAddress(const QString& address) { m_walletAddress = address; }

void StakingPage::setAutoRefreshOnShow(bool enabled) {
    m_autoRefreshOnShow = enabled;
    if (!enabled) {
        m_hasRefreshed = true;
    }
}

void StakingPage::setAvatarCache(AvatarCache* cache) {
    m_avatarCache = cache;
    connect(m_avatarCache, &AvatarCache::avatarReady, this, &StakingPage::updateAvatarForUrl);
}

void StakingPage::updateAvatarForUrl(const QString& url) {
    if (!m_avatarCache) {
        return;
    }
    QPixmap pm = m_avatarCache->get(url);
    if (pm.isNull()) {
        return;
    }
    qreal dpr = devicePixelRatioF();
    QPixmap circle = AvatarCache::circleClip(pm, 36, dpr);

    // Only scan active (visible) rows — max ~60 widgets
    for (auto it = m_activeRows.constBegin(); it != m_activeRows.constEnd(); ++it) {
        QWidget* row = it.value();
        QLabel* icon = row->findChild<QLabel*>("validatorIcon");
        if (icon && icon->property("avatarUrl").toString() == url) {
            icon->setPixmap(circle);
            icon->setText({});
            icon->setProperty("hasAvatar", true);
            icon->style()->unpolish(icon);
            icon->style()->polish(icon);
        }
    }

    // Also refresh My Stakes if a relevant validator avatar arrived
    if (!m_stakeAccounts.isEmpty()) {
        for (const auto& sa : std::as_const(m_stakeAccounts)) {
            // Check in-memory validators
            if (m_validatorService) {
                for (const auto& v : m_validatorService->validators()) {
                    if (v.voteAccount == sa.voteAccount && v.avatarUrl == url) {
                        populateMyStakes();
                        return;
                    }
                }
            }
            // Check DB cache
            auto rec = ValidatorCacheDb::getByVoteAccountRecord(sa.voteAccount);
            if (rec && rec->avatarUrl == url) {
                populateMyStakes();
                return;
            }
        }
    }
}

// ── Refresh ──────────────────────────────────────────────────────

void StakingPage::refresh() {
    if (!m_solanaApi || m_walletAddress.isEmpty()) {
        return;
    }

    // Seed from persisted stake accounts immediately so stake views and the
    // dashboard do not flash empty while RPC catches up.
    if (m_stakeAccounts.isEmpty()) {
        const QList<StakeAccountInfo> cached = StakeAccountDb::load(m_walletAddress);
        if (!cached.isEmpty()) {
            m_stakeAccounts = cached;
            populateMyStakes();
            if (!m_selectedStakeAccount.address.isEmpty()) {
                for (const auto& sa : std::as_const(m_stakeAccounts)) {
                    if (sa.address == m_selectedStakeAccount.address) {
                        m_selectedStakeAccount = sa;
                        if (m_stack->currentWidget() == m_stakeDetailPage) {
                            populateStakeDetail();
                        }
                        break;
                    }
                }
            }
        }
    }

    if (m_validators.isEmpty()) {
        m_statusLabel->setText(tr("Loading validators..."));
        m_statusLabel->setProperty("tone", "muted");
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
        m_statusLabel->show();
    }

    // Fetch SOL balance for MAX button
    auto balConn = std::make_shared<QMetaObject::Connection>();
    *balConn = connect(m_solanaApi, &SolanaApi::balanceReady, this,
                       [this, balConn](const QString& addr, quint64 lamports) {
                           if (addr == m_walletAddress) {
                               disconnect(*balConn);
                               m_solBalance = lamports;
                           }
                       });
    m_solanaApi->fetchBalance(m_walletAddress);

    // Fetch rent exemption for stake account (200 bytes)
    auto rentConn = std::make_shared<QMetaObject::Connection>();
    *rentConn = connect(m_solanaApi, &SolanaApi::minimumBalanceReady, this,
                        [this, rentConn](quint64 lamports) {
                            disconnect(*rentConn);
                            m_rentExempt = lamports;
                        });
    m_solanaApi->fetchMinimumBalanceForRentExemption(200);

    // Fetch stake accounts — persistent connection so post-transaction
    // refreshes also populate the list
    if (!m_stakeAccountsConnected) {
        m_stakeAccountsConnected = true;
        connect(m_solanaApi, &SolanaApi::stakeAccountsReady, this,
                [this](const QString& wallet, const QJsonArray& accounts) {
                    qWarning() << "[Staking] stakeAccountsReady: wallet=" << wallet
                               << "accounts=" << accounts.size() << "myWallet=" << m_walletAddress;
                    if (wallet != m_walletAddress) {
                        qWarning() << "[Staking] wallet mismatch, ignoring";
                        return;
                    }

                    m_stakeAccounts.clear();
                    for (const auto& a : accounts) {
                        QJsonObject obj = a.toObject();
                        QString pubkey = obj["pubkey"].toString();
                        qWarning() << "[Staking] parsing stake account:" << pubkey;
                        StakeAccountInfo info = StakeAccountInfo::fromJsonParsed(
                            pubkey, obj["account"].toObject(), m_currentEpoch);
                        m_stakeAccounts.append(info);
                        m_pendingStakeAccounts.remove(pubkey);
                    }

                    for (auto it = m_pendingStakeAccounts.constBegin();
                         it != m_pendingStakeAccounts.constEnd(); ++it) {
                        bool found = false;
                        for (const auto& sa : std::as_const(m_stakeAccounts)) {
                            if (sa.address == it.key()) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            m_stakeAccounts.prepend(it.value());
                        }
                    }

                    // Persist to DB for instant dashboard display on next launch
                    StakeAccountDb::save(m_walletAddress, m_stakeAccounts);

                    qWarning() << "[Staking] populateMyStakes with" << m_stakeAccounts.size()
                               << "accounts";
                    populateMyStakes();
                    if (!m_selectedStakeAccount.address.isEmpty()) {
                        for (const auto& sa : std::as_const(m_stakeAccounts)) {
                            if (sa.address == m_selectedStakeAccount.address) {
                                m_selectedStakeAccount = sa;
                                if (m_stack->currentWidget() == m_stakeDetailPage) {
                                    populateStakeDetail();
                                }
                                break;
                            }
                        }
                    }
                });
    }
    qWarning() << "[Staking] calling fetchStakeAccounts for" << m_walletAddress;
    m_solanaApi->fetchStakeAccounts(m_walletAddress);

    // Refresh validators
    m_validatorService->refresh();
}

// ── Prefetch (data only, no user-specific queries) ───────────────

void StakingPage::prefetchValidators() {
    if (m_validatorService) {
        m_validatorService->refresh();
    }
}

// ── showEvent ────────────────────────────────────────────────────

void StakingPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_autoRefreshOnShow && !m_hasRefreshed) {
        m_hasRefreshed = true;
        refresh();
    }
}

// ── Sort ─────────────────────────────────────────────────────────

void StakingPage::sortValidators(SortColumn col) {
    if (m_sortColumn == col) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortColumn = col;
        // Default direction: descending for numeric columns, ascending for name/version
        m_sortAscending = (col == SortColumn::Name || col == SortColumn::Version);
    }

    auto cmp = [this](const ValidatorInfo& a, const ValidatorInfo& b) {
        const ValidatorInfo& lhs = m_sortAscending ? a : b;
        const ValidatorInfo& rhs = m_sortAscending ? b : a;
        switch (m_sortColumn) {
            case SortColumn::Name:
                return lhs.name.toLower() < rhs.name.toLower();
            case SortColumn::Stake:
                return lhs.activatedStake < rhs.activatedStake;
            case SortColumn::Apy:
                return lhs.apy < rhs.apy;
            case SortColumn::Commission:
                return lhs.commission < rhs.commission;
            case SortColumn::Version:
                return lhs.version < rhs.version;
        }
        return false;
    };

    std::sort(m_validators.begin(), m_validators.end(), cmp);
    updateHeaderLabels();
    rebuildFilteredList(m_searchInput ? m_searchInput->text() : QString());
}

void StakingPage::updateSortIcon(QPushButton* btn, int state) {
    qreal dpr = devicePixelRatioF();
    QPixmap pm = txTypeIcon("sort", 12, dpr, QColor(255, 255, 255, 100));
    if (state == 1) {
        QPixmap overlay = txTypeIcon("sort-asc", 12, dpr, QColor("#60a5fa"));
        QPainter p(&pm);
        p.drawPixmap(0, 0, overlay);
    } else if (state == 2) {
        QPixmap overlay = txTypeIcon("sort-desc", 12, dpr, QColor("#60a5fa"));
        QPainter p(&pm);
        p.drawPixmap(0, 0, overlay);
    }
    btn->setIcon(QIcon(pm));
}

void StakingPage::updateHeaderLabels() {
    for (int i = 0; i < m_sortBtns.size(); ++i) {
        bool active = (static_cast<int>(m_sortColumn) == i);
        int state = active ? (m_sortAscending ? 1 : 2) : 0;
        updateSortIcon(m_sortBtns[i], state);

        if (i < m_headerLabels.size()) {
            m_headerLabels[i]->setProperty("activeSort", active);
            m_headerLabels[i]->style()->unpolish(m_headerLabels[i]);
            m_headerLabels[i]->style()->polish(m_headerLabels[i]);
        }
    }
}

// ── Virtual validator list ────────────────────────────────────────

QWidget* StakingPage::createPoolRow() {
    QWidget* row = new QWidget();
    row->setObjectName("validatorRow");
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    row->setCursor(Qt::PointingHandCursor);
    row->setFixedHeight(ROW_H);
    row->installEventFilter(this);

    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(10, 6, 10, 6);
    rowLayout->setSpacing(8);

    // Icon
    QLabel* icon = new QLabel();
    icon->setObjectName("validatorIcon");
    icon->setFixedSize(36, 36);
    icon->setAlignment(Qt::AlignCenter);
    rowLayout->addWidget(icon);

    // Name column
    QWidget* nameWidget = new QWidget();
    nameWidget->setObjectName("stakingTransparent");
    QVBoxLayout* nameCol = new QVBoxLayout(nameWidget);
    nameCol->setContentsMargins(0, 0, 0, 0);
    nameCol->setSpacing(1);

    QLabel* nameLabel = new QLabel();
    nameLabel->setObjectName("validatorName");
    nameLabel->setProperty("uiClass", "stakingValidatorName");
    nameCol->addWidget(nameLabel);

    QLabel* subLabel = new QLabel();
    subLabel->setObjectName("validatorSub");
    subLabel->setProperty("uiClass", "stakingValidatorSub");
    nameCol->addWidget(subLabel);

    rowLayout->addWidget(nameWidget, 4);

    // Stake
    QLabel* stakeLabel = new QLabel();
    stakeLabel->setObjectName("validatorStake");
    stakeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stakeLabel->setProperty("uiClass", "stakingValidatorStake");
    rowLayout->addWidget(stakeLabel, 2);

    // APY
    QLabel* apyLabel = new QLabel();
    apyLabel->setObjectName("validatorApy");
    apyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(apyLabel, 1);

    // Commission
    QLabel* commLabel = new QLabel();
    commLabel->setObjectName("validatorCommission");
    commLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    commLabel->setProperty("uiClass", "stakingValidatorCommission");
    rowLayout->addWidget(commLabel, 1);

    // Version
    QLabel* verLabel = new QLabel();
    verLabel->setObjectName("validatorVersion");
    verLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    verLabel->setProperty("uiClass", "stakingValidatorVersion");
    rowLayout->addWidget(verLabel, 1);

    return row;
}

void StakingPage::bindRow(QWidget* row, int dataIndex) {
    const ValidatorInfo& v = m_validators[m_filteredValidators[dataIndex]];

    // Icon
    QLabel* icon = row->findChild<QLabel*>("validatorIcon");
    bool hasAvatar = false;
    if (m_avatarCache && !v.avatarUrl.isEmpty()) {
        QPixmap pm = m_avatarCache->get(v.avatarUrl);
        if (!pm.isNull()) {
            qreal dpr = devicePixelRatioF();
            icon->setPixmap(AvatarCache::circleClip(pm, 36, dpr));
            icon->setText({});
            icon->setProperty("hasAvatar", true);
            icon->style()->unpolish(icon);
            icon->style()->polish(icon);
            hasAvatar = true;
        }
        icon->setProperty("avatarUrl", v.avatarUrl);
    }
    if (!hasAvatar) {
        QString letter =
            v.name.isEmpty() ? v.voteAccount.left(1).toUpper() : v.name.left(1).toUpper();
        icon->setText(letter);
        icon->setPixmap(QPixmap()); // clear any previous avatar
        icon->setProperty("hasAvatar", false);
        icon->style()->unpolish(icon);
        icon->style()->polish(icon);
        icon->setProperty("avatarUrl", QVariant());
    }

    // Name
    QString displayName = v.name.isEmpty() ? v.voteAccount : v.name;
    row->findChild<QLabel*>("validatorName")->setText(displayName);

    // Subtitle
    QString subtitle;
    if (!v.city.isEmpty()) {
        subtitle = v.city;
        if (!v.country.isEmpty()) {
            subtitle += ", " + v.country;
        }
    }
    if (v.delinquent) {
        subtitle = subtitle.isEmpty() ? "Delinquent" : subtitle + " | Delinquent";
    } else if (v.superminority) {
        subtitle = subtitle.isEmpty() ? "Superminority" : subtitle + " | Superminority";
    }
    row->findChild<QLabel*>("validatorSub")->setText(subtitle);

    // Stake
    row->findChild<QLabel*>("validatorStake")->setText(formatStake(v.stakeInSol()));

    // APY
    QLabel* apyLabel = row->findChild<QLabel*>("validatorApy");
    apyLabel->setText(v.delinquent ? "--" : formatApy(v.apy));
    apyLabel->setProperty("delinquent", v.delinquent);
    apyLabel->style()->unpolish(apyLabel);
    apyLabel->style()->polish(apyLabel);

    // Commission
    row->findChild<QLabel*>("validatorCommission")->setText(QString::number(v.commission) + "%");

    // Version
    row->findChild<QLabel*>("validatorVersion")->setText(v.version.isEmpty() ? "--" : v.version);

    // Properties for event filter
    row->setProperty("voteAccount", v.voteAccount);

    // Position and show
    row->move(0, dataIndex * ROW_H);
    row->resize(m_validatorScroll->viewport()->width(), ROW_H);
    row->setVisible(true);
}

void StakingPage::rebuildFilteredList(const QString& filter) {
    // Hide all active rows so relayoutVisibleRows rebinds them with fresh data
    for (auto it = m_activeRows.begin(); it != m_activeRows.end(); ++it) {
        it.value()->setVisible(false);
    }
    m_activeRows.clear();

    m_filteredValidators.clear();
    for (int i = 0; i < m_validators.size(); ++i) {
        const ValidatorInfo& v = m_validators[i];
        if (!filter.isEmpty() && !v.name.contains(filter, Qt::CaseInsensitive) &&
            !v.voteAccount.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        m_filteredValidators.append(i);
    }

    // Resize container to hold all filtered rows
    int totalH = m_filteredValidators.size() * ROW_H;
    m_validatorContainer->setMinimumHeight(qMax(totalH, 1));

    // Show/hide no-match label
    bool noMatch = m_filteredValidators.isEmpty() && !m_validators.isEmpty();
    m_noMatchLabel->setVisible(noMatch);
    m_validatorScroll->setVisible(!noMatch);

    // Reset scroll to top on filter change
    m_validatorScroll->verticalScrollBar()->setValue(0);

    relayoutVisibleRows();
}

void StakingPage::relayoutVisibleRows() {
    if (m_filteredValidators.isEmpty()) {
        // Hide all active rows
        for (auto it = m_activeRows.begin(); it != m_activeRows.end(); ++it) {
            it.value()->setVisible(false);
        }
        m_activeRows.clear();
        return;
    }

    int scrollY = m_validatorScroll->verticalScrollBar()->value();
    int viewportH = m_validatorScroll->viewport()->height();
    int filteredCount = m_filteredValidators.size();

    int firstVisible = scrollY / ROW_H;
    int lastVisible = qMin((scrollY + viewportH) / ROW_H, filteredCount - 1);
    int firstBuffered = qMax(0, firstVisible - BUFFER_ROWS);
    int lastBuffered = qMin(filteredCount - 1, lastVisible + BUFFER_ROWS);

    // Collect rows outside the new range → return to pool
    QList<int> toRemove;
    for (auto it = m_activeRows.begin(); it != m_activeRows.end(); ++it) {
        if (it.key() < firstBuffered || it.key() > lastBuffered) {
            it.value()->setVisible(false);
            toRemove.append(it.key());
        }
    }
    for (int idx : toRemove) {
        m_activeRows.remove(idx);
    }

    // Build set of free pool rows
    QSet<QWidget*> usedRows;
    for (auto it = m_activeRows.constBegin(); it != m_activeRows.constEnd(); ++it) {
        usedRows.insert(it.value());
    }

    QList<QWidget*> freeRows;
    for (QWidget* w : std::as_const(m_rowPool)) {
        if (!usedRows.contains(w)) {
            freeRows.append(w);
        }
    }

    int freeIdx = 0;
    int containerW = m_validatorScroll->viewport()->width();

    for (int i = firstBuffered; i <= lastBuffered; ++i) {
        if (m_activeRows.contains(i)) {
            // Already bound — just update position/width in case of resize
            QWidget* row = m_activeRows[i];
            row->move(0, i * ROW_H);
            row->resize(containerW, ROW_H);
            continue;
        }
        if (freeIdx >= freeRows.size()) {
            break; // pool exhausted
        }
        QWidget* row = freeRows[freeIdx++];
        bindRow(row, i);
        m_activeRows[i] = row;
    }
}

// ── Populate my stakes ───────────────────────────────────────────

void StakingPage::populateMyStakes() {
    QLayoutItem* child;
    while ((child = m_myStakesLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    if (m_stakeAccounts.isEmpty()) {
        m_stakeSummaryLabel->setText(
            tr("No stake accounts found. Stake SOL from the Validators tab."));
        emit stakingSummaryChanged({});
        return;
    }

    double totalStaked = 0;
    QList<DashboardStakeItem> summaryItems;
    for (const auto& sa : std::as_const(m_stakeAccounts)) {
        totalStaked += sa.solAmount();

        DashboardStakeItem item;
        item.stakeAddress = sa.address;
        const StakeLifecycle lifecycle = StakeLifecycle::derive(sa, m_currentEpoch);
        item.stateString = lifecycle.statusLabel;
        item.solAmount = sa.solAmount();

        // Resolve validator name and avatar URL from in-memory list
        if (m_validatorService && !sa.voteAccount.isEmpty()) {
            for (const auto& v : m_validatorService->validators()) {
                if (v.voteAccount == sa.voteAccount) {
                    item.validatorName = v.name;
                    item.avatarUrl = v.avatarUrl;
                    break;
                }
            }
            // Fallback to DB cache if in-memory list didn't have it
            if (item.validatorName.isEmpty()) {
                auto rec = ValidatorCacheDb::getByVoteAccountRecord(sa.voteAccount);
                if (rec) {
                    item.validatorName = rec->name;
                    item.avatarUrl = rec->avatarUrl;
                }
            }
        }
        if (item.validatorName.isEmpty()) {
            item.validatorName =
                sa.voteAccount.isEmpty() ? tr("Unknown") : sa.voteAccount.left(8) + "...";
        }
        summaryItems.append(item);
    }
    m_stakeSummaryLabel->setText(tr("Your staked SOL: %1 | %2 stake account(s)")
                                     .arg(formatSol(totalStaked))
                                     .arg(m_stakeAccounts.size()));
    emit stakingSummaryChanged(summaryItems);

    qreal dpr = devicePixelRatioF();
    for (const auto& sa : std::as_const(m_stakeAccounts)) {
        auto* item = new CardListItem();

        // Try loading validator avatar; fall back to state letter icon
        bool hasAvatar = false;
        if (m_avatarCache && !sa.voteAccount.isEmpty()) {
            // Try in-memory validator list first, then DB cache
            QString avatarUrl;
            if (m_validatorService) {
                for (const auto& v : m_validatorService->validators()) {
                    if (v.voteAccount == sa.voteAccount) {
                        avatarUrl = v.avatarUrl;
                        break;
                    }
                }
            }
            if (avatarUrl.isEmpty()) {
                auto rec = ValidatorCacheDb::getByVoteAccountRecord(sa.voteAccount);
                if (rec) {
                    avatarUrl = rec->avatarUrl;
                }
            }
            if (!avatarUrl.isEmpty()) {
                QPixmap pm = m_avatarCache->get(avatarUrl);
                if (!pm.isNull()) {
                    item->setIconPixmap(AvatarCache::circleClip(pm, 36, dpr), "stakeValidatorIcon",
                                        "");
                    hasAvatar = true;
                }
            }
        }

        if (!hasAvatar) {
            // State-colored letter fallback
            QString iconBg;
            QString icon;
            const StakeLifecycle lifecycle = StakeLifecycle::derive(sa, m_currentEpoch);
            switch (lifecycle.phase) {
                case StakeLifecycle::Phase::Active:
                    iconBg = "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                             " stop:0 rgba(16,185,129,0.5), stop:1 rgba(5,150,105,0.5))";
                    icon = "A";
                    break;
                case StakeLifecycle::Phase::Activating:
                    iconBg = "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                             " stop:0 rgba(245,158,11,0.5), stop:1 rgba(217,119,6,0.5))";
                    icon = "A";
                    break;
                case StakeLifecycle::Phase::Deactivating:
                    iconBg = "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                             " stop:0 rgba(239,68,68,0.5), stop:1 rgba(220,38,38,0.5))";
                    icon = "D";
                    break;
                case StakeLifecycle::Phase::Inactive:
                    iconBg = "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                             " stop:0 rgba(100,116,139,0.5), stop:1 rgba(71,85,105,0.5))";
                    icon = "I";
                    break;
                default:
                    iconBg = Theme::txIconNeutral;
                    icon = "?";
                    break;
            }
            item->setIcon(icon, "stakeStateIcon", iconBg);
        }

        // Title: validator name or truncated vote account
        QString validatorName;
        if (m_validatorService && !sa.voteAccount.isEmpty()) {
            validatorName = m_validatorService->validatorName(sa.voteAccount);
        }
        if (validatorName.isEmpty()) {
            validatorName =
                sa.voteAccount.isEmpty() ? tr("Unknown") : sa.voteAccount.left(8) + "...";
        }
        item->setTitle(validatorName);
        const StakeLifecycle lifecycle = StakeLifecycle::derive(sa, m_currentEpoch);
        item->setSubtitle(lifecycle.statusLabel + QString::fromUtf8("  ·  ") + sa.address.left(8) +
                          "..." + sa.address.right(4));

        item->setValue(formatSol(sa.solAmount()), "listItemTitle");

        // Action button in the right area
        // We can't add buttons to CardListItem directly, so we use eventFilter clicks
        QString stakeAddr = sa.address;
        quint64 lamports = sa.lamports;
        item->setProperty("stakeAddress", stakeAddr);
        item->setProperty("stakeLamports", QVariant::fromValue(lamports));
        item->installEventFilter(this);

        item->setSubValue(tr("View details"), "listItemSubtitle");

        m_myStakesLayout->addWidget(item);
    }
}

void StakingPage::openStakeDetail(const QString& stakeAddress) {
    QList<StakeAccountInfo> candidates = m_stakeAccounts;
    if (candidates.isEmpty() && !m_walletAddress.isEmpty()) {
        candidates = StakeAccountDb::load(m_walletAddress);
        if (!candidates.isEmpty()) {
            m_stakeAccounts = candidates;
        }
    }
    for (const auto& sa : std::as_const(candidates)) {
        if (sa.address == stakeAddress) {
            resetStakeDetailActivityViews();
            if (m_detailLifecycle) {
                m_detailLifecycle->setCurrentEtaText({});
                m_detailLifecycle->setFooterText({});
                m_detailLifecycle->setFooterPending(true, tr("Estimating Reward"));
            }
            invalidateStakeDetailRewardEstimate();
            m_selectedStakeAccount = sa;
            if (m_detailActivityTabs) {
                m_detailActivityTabs->setActiveIndex(0);
            }
            populateStakeDetail();
            showStep(Step::StakeDetail);
            preloadStakeDetailData();
            return;
        }
    }
}

void StakingPage::preloadStakeDetailData() {
    if (m_selectedStakeAccount.address.isEmpty()) {
        return;
    }

    // Hydrate both activity panes immediately on detail entry so tab switches
    // are instant and never trigger first-load behavior.
    loadStakeDetailTransactions();
    requestStakeDetailRewards();
}

void StakingPage::resetStakeDetailActivityViews() {
    m_detailSignatures.clear();
    m_detailTransactions.clear();
    m_detailFailedTransactions.clear();
    m_detailEpochRewards.clear();
    m_detailPendingRewardEpochs.clear();
    invalidateStakeDetailRewardEstimate();

    if (m_detailLifecycle) {
        m_detailLifecycle->setCurrentEtaText({});
        m_detailLifecycle->setFooterText({});
        m_detailLifecycle->setFooterPending(false);
    }

    if (m_detailTransactionsLayout) {
        QLayoutItem* child;
        while ((child = m_detailTransactionsLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }
    }

    if (m_detailRewardsLayout) {
        QLayoutItem* child;
        while ((child = m_detailRewardsLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                delete child->widget();
            }
            delete child;
        }
    }

    if (m_detailTransactionsStatus) {
        m_detailTransactionsStatus->setText(tr("Loading transactions..."));
        m_detailTransactionsStatus->setProperty("tone", "muted");
        m_detailTransactionsStatus->style()->unpolish(m_detailTransactionsStatus);
        m_detailTransactionsStatus->style()->polish(m_detailTransactionsStatus);
        m_detailTransactionsStatus->show();
    }

    if (m_detailRewardsStatus) {
        m_detailRewardsStatus->setText(tr("Loading rewards..."));
        m_detailRewardsStatus->setProperty("tone", "muted");
        m_detailRewardsStatus->style()->unpolish(m_detailRewardsStatus);
        m_detailRewardsStatus->style()->polish(m_detailRewardsStatus);
        m_detailRewardsStatus->show();
    }
}

void StakingPage::invalidateStakeDetailRewardEstimate() {
    m_detailRewardEstimateValid = false;
    m_detailRewardEstimateAddress.clear();
    m_detailRewardEstimateVoteAccount.clear();
    m_detailRewardEstimateEpoch = 0;
    m_detailRewardEstimateActiveLamports = 0;
    m_detailRewardEstimateSol = 0.0;
}

void StakingPage::populateStakeDetail() {
    if (m_selectedStakeAccount.address.isEmpty()) {
        return;
    }

    QString validatorName;
    if (m_validatorService && !m_selectedStakeAccount.voteAccount.isEmpty()) {
        validatorName = m_validatorService->validatorName(m_selectedStakeAccount.voteAccount);
    }
    if (validatorName.isEmpty() && !m_selectedStakeAccount.voteAccount.isEmpty()) {
        auto rec = ValidatorCacheDb::getByVoteAccountRecord(m_selectedStakeAccount.voteAccount);
        if (rec) {
            validatorName = rec->name;
        }
    }
    if (validatorName.isEmpty()) {
        validatorName = tr("Unknown");
    }

    const StakeLifecycle lifecycle = StakeLifecycle::derive(m_selectedStakeAccount, m_currentEpoch);
    const double delegatedStake = static_cast<double>(lifecycle.delegatedLamports) / 1e9;
    const double activeStake = static_cast<double>(lifecycle.activeLamports) / 1e9;
    const double inactiveStake = static_cast<double>(lifecycle.inactiveLamports) / 1e9;
    const bool lockupActive = lifecycle.lockupActive;

    invalidateStakeDetailRewardEstimate();

    // Clear any previous detail-footer state before the new lifecycle renders.
    m_detailLifecycle->setCurrentEtaText({});
    m_detailLifecycle->setFooterText({});
    if (lifecycle.phase == StakeLifecycle::Phase::Active) {
        m_detailLifecycle->setFooterPending(true, tr("Estimating Reward"));
    } else {
        m_detailLifecycle->setFooterPending(false);
    }

    m_detailTitle->setText(validatorName);
    m_detailAddress->setText(QString::fromUtf8("— ") + m_selectedStakeAccount.address);
    m_detailTotalValue->setText(formatSol(m_selectedStakeAccount.solAmount()));
    m_detailDelegatedStake->setText(formatSol(delegatedStake));
    m_detailActiveStake->setText(formatSol(activeStake));
    m_detailInactiveStake->setText(formatSol(inactiveStake));
    m_detailTotalRewards->setText(formatSol(m_selectedStakeAccount.totalRewardsSol()));
    m_detailRentReserve->setText(
        formatSol(static_cast<double>(m_selectedStakeAccount.rentExemptReserve) / 1e9));
    m_detailType->setText(m_selectedStakeAccount.voteAccount.isEmpty() ? tr("Undelegated")
                                                                       : tr("Delegated"));
    m_detailLifecycle->setLifecycle(lifecycle);
    m_detailValidator->setText(validatorName);
    m_detailVoteAccount->setAddress(m_selectedStakeAccount.voteAccount);
    m_detailWithdrawAuthority->setAddress(m_selectedStakeAccount.withdrawer);
    m_detailStakeAuthority->setAddress(m_selectedStakeAccount.staker);
    m_detailActivationEpoch->setText(formatEpochValue(m_selectedStakeAccount.activationEpoch));
    m_detailDeactivationEpoch->setText(formatEpochValue(m_selectedStakeAccount.deactivationEpoch));
    m_detailAllocatedSize->setText(
        tr("%1 byte(s)").arg(QString::number(m_selectedStakeAccount.allocatedDataSize)));
    m_detailCustodian->setText(m_selectedStakeAccount.lockupCustodian.isEmpty()
                                   ? tr("None")
                                   : m_selectedStakeAccount.lockupCustodian);

    if (lockupActive) {
        QString untilText;
        if (m_selectedStakeAccount.lockupUnixTimestamp > QDateTime::currentSecsSinceEpoch()) {
            untilText = QDateTime::fromSecsSinceEpoch(m_selectedStakeAccount.lockupUnixTimestamp,
                                                      QTimeZone::UTC)
                            .toString("MMM d, yyyy hh:mm 'UTC'");
        } else {
            untilText = tr("epoch %1").arg(m_selectedStakeAccount.lockupEpoch);
        }
        m_detailLockup->setText(tr("Account is locked. Lockup expires at %1.").arg(untilText));
        m_detailLockup->setProperty("tone", "warning");
        m_detailLockup->show();
    } else {
        m_detailLockup->hide();
    }

    if (lifecycle.phase == StakeLifecycle::Phase::Activating ||
        lifecycle.phase == StakeLifecycle::Phase::Deactivating ||
        lifecycle.phase == StakeLifecycle::Phase::Active) {
        if (m_solanaApi) {
            m_solanaApi->fetchEpochInfo();
            m_solanaApi->fetchRecentPerformanceSamples(5);
        }
        updateStakeDetailEta();
    } else {
        m_epochEtaTargetSecs = 0;
        m_detailLifecycle->setCurrentEtaText({});
        m_detailEtaTimer->stop();
    }

    m_detailPrimaryAction->setVisible(lifecycle.actionEnabled);
    if (lifecycle.actionEnabled) {
        m_detailPrimaryAction->setText(lifecycle.actionLabel);
        m_detailPrimaryAction->setEnabled(true);
    }
}

void StakingPage::loadStakeDetailTransactions() {
    m_detailSignatures =
        TransactionDb::getRecentSignaturesForAddress(m_selectedStakeAccount.address, 10);
    m_detailTransactions.clear();
    m_detailFailedTransactions.clear();
    for (const auto& sig : std::as_const(m_detailSignatures)) {
        const QString rawJson = TransactionDb::getRawJson(sig.signature);
        if (rawJson.isEmpty()) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
        if (!doc.isObject()) {
            continue;
        }
        m_detailTransactions.insert(sig.signature, TransactionResponse::fromJson(doc.object()));
    }
    renderStakeDetailTransactions();
    renderStakeDetailRewards();

    if (!m_solanaApi || m_selectedStakeAccount.address.isEmpty()) {
        return;
    }

    m_detailTransactionsStatus->setText(tr("Loading transactions..."));
    m_detailTransactionsStatus->setProperty("tone", "muted");
    m_detailTransactionsStatus->style()->unpolish(m_detailTransactionsStatus);
    m_detailTransactionsStatus->style()->polish(m_detailTransactionsStatus);
    m_detailTransactionsStatus->show();
    m_solanaApi->fetchSignatures(m_selectedStakeAccount.address, 10);
}

void StakingPage::renderStakeDetailTransactions() {
    if (!m_detailTransactionsLayout) {
        return;
    }

    QLayoutItem* child;
    while ((child = m_detailTransactionsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    if (m_detailSignatures.isEmpty()) {
        m_detailTransactionsStatus->setText(tr("No recent transactions found."));
        m_detailTransactionsStatus->setProperty("tone", "muted");
        m_detailTransactionsStatus->style()->unpolish(m_detailTransactionsStatus);
        m_detailTransactionsStatus->style()->polish(m_detailTransactionsStatus);
        m_detailTransactionsStatus->show();
        return;
    }

    bool waitingForDetails = false;
    for (const auto& sig : std::as_const(m_detailSignatures)) {
        auto* row = new CardListItem();
        row->setIcon(sig.hasError ? "!" : QString::fromUtf8("\xe2\x86\x97"), "stakeStateIcon",
                     sig.hasError ? "qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 "
                                    "rgba(239,68,68,0.5), stop:1 rgba(220,38,38,0.5))"
                                  : "qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 "
                                    "rgba(59,130,246,0.45), stop:1 rgba(124,58,237,0.45))");

        const bool hasTx = m_detailTransactions.contains(sig.signature);
        const bool failed = m_detailFailedTransactions.contains(sig.signature);
        QString title = hasTx ? classifyStakeTransaction(m_detailTransactions.value(sig.signature),
                                                         m_selectedStakeAccount.address)
                              : (failed ? tr("Unavailable") : tr("Loading..."));
        QString subtitle = tr("%1  ·  %2").arg(relativeTimeString(sig.blockTime), sig.signature);
        row->setTitle(title);
        row->setSubtitle(subtitle);

        if (hasTx) {
            const auto& tx = m_detailTransactions.value(sig.signature);
            const int idx = tx.accountIndex(m_selectedStakeAccount.address);
            const qint64 delta = tx.solBalanceChange(idx);
            const double deltaSol = static_cast<double>(delta) / 1e9;
            QString deltaText = (delta > 0 ? "+" : "") + QString::number(deltaSol, 'f', 4) + " SOL";
            row->setValue(deltaText,
                          delta >= 0 ? "stakingDetailDeltaPositive" : "stakingDetailDeltaNegative");
            row->setSubValue(sig.hasError ? tr("Failed • View Transaction")
                                          : tr("View Transaction"),
                             "listItemSubtitle");
        } else if (failed) {
            row->setValue(tr("—"), "listItemTitle");
            row->setSubValue(tr("Failed to load transaction"), "listItemSubtitle");
        } else {
            waitingForDetails = true;
            row->setSubValue(tr("Loading transaction..."), "listItemSubtitle");
        }

        row->setProperty("detailSignature", sig.signature);
        row->installEventFilter(this);
        m_detailTransactionsLayout->addWidget(row);
    }

    if (waitingForDetails) {
        m_detailTransactionsStatus->setText(tr("Loading transactions..."));
        m_detailTransactionsStatus->setProperty("tone", "muted");
        m_detailTransactionsStatus->style()->unpolish(m_detailTransactionsStatus);
        m_detailTransactionsStatus->style()->polish(m_detailTransactionsStatus);
        m_detailTransactionsStatus->show();
    } else {
        m_detailTransactionsStatus->hide();
    }
}

void StakingPage::updateStakeDetailEta() {
    if (!m_detailLifecycle) {
        return;
    }
    if (m_selectedStakeAccount.address.isEmpty()) {
        m_detailLifecycle->setCurrentEtaText({});
        m_detailLifecycle->setFooterPending(false);
        m_detailEtaTimer->stop();
        return;
    }

    const StakeLifecycle lifecycle = StakeLifecycle::derive(m_selectedStakeAccount, m_currentEpoch);
    const bool transitional = lifecycle.phase == StakeLifecycle::Phase::Activating ||
                              lifecycle.phase == StakeLifecycle::Phase::Deactivating;
    const bool earning = lifecycle.phase == StakeLifecycle::Phase::Active;
    if ((!transitional && !earning) || m_slotsInEpoch == 0 || m_avgSecondsPerSlot <= 0.0 ||
        m_epochSlotIndex >= m_slotsInEpoch) {
        m_detailLifecycle->setCurrentEtaText({});
        m_detailLifecycle->setFooterPending(false);
        m_detailLifecycle->setFooterText(earning ? lifecycle.description : QString());
        m_detailEtaTimer->stop();
        return;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_epochEtaTargetSecs <= now) {
        const quint64 remainingSlots = m_slotsInEpoch - m_epochSlotIndex;
        const qint64 etaSecs = static_cast<qint64>(
            std::llround(static_cast<double>(remainingSlots) * m_avgSecondsPerSlot));
        m_epochEtaTargetSecs = now + etaSecs;
    }
    const qint64 remainingSecs = qMax<qint64>(0, m_epochEtaTargetSecs - now);

    if (transitional) {
        m_detailLifecycle->setCurrentEtaText(formatCountdown(remainingSecs));
        m_detailLifecycle->setFooterPending(false);
        m_detailLifecycle->setFooterText({});
    } else if (earning) {
        m_detailLifecycle->setCurrentEtaText({});

        double validatorApy = 0.0;
        bool foundValidator = false;
        if (m_validatorService && !m_selectedStakeAccount.voteAccount.isEmpty()) {
            for (const auto& v : m_validatorService->validators()) {
                if (v.voteAccount == m_selectedStakeAccount.voteAccount) {
                    foundValidator = true;
                    validatorApy = v.delinquent ? 0.0 : v.apy;
                    break;
                }
            }
        }

        if (!foundValidator) {
            m_detailLifecycle->setFooterText({});
            m_detailLifecycle->setFooterPending(true, tr("Estimating Reward"));
            if (!m_detailEtaTimer->isActive()) {
                m_detailEtaTimer->start();
            }
            return;
        }

        if (validatorApy <= 0.0) {
            m_detailLifecycle->setFooterText({});
            m_detailLifecycle->setFooterPending(true, tr("Estimating Reward"));
            if (!m_detailEtaTimer->isActive()) {
                m_detailEtaTimer->start();
            }
            return;
        }

        if (!m_detailRewardEstimateValid ||
            m_detailRewardEstimateAddress != m_selectedStakeAccount.address ||
            m_detailRewardEstimateVoteAccount != m_selectedStakeAccount.voteAccount ||
            m_detailRewardEstimateEpoch != m_currentEpoch ||
            m_detailRewardEstimateActiveLamports != lifecycle.activeLamports) {
            const double activeStakeSol = static_cast<double>(lifecycle.activeLamports) / 1e9;
            const double epochDurationSecs =
                static_cast<double>(m_slotsInEpoch) * m_avgSecondsPerSlot;
            m_detailRewardEstimateSol = activeStakeSol * (validatorApy / 100.0) *
                                        (epochDurationSecs / (365.0 * 24.0 * 60.0 * 60.0));
            m_detailRewardEstimateAddress = m_selectedStakeAccount.address;
            m_detailRewardEstimateVoteAccount = m_selectedStakeAccount.voteAccount;
            m_detailRewardEstimateEpoch = m_currentEpoch;
            m_detailRewardEstimateActiveLamports = lifecycle.activeLamports;
            m_detailRewardEstimateValid = true;
        }
        const QString footer =
            tr("Next payout est. %1 in %2")
                .arg(formatEstimatedSol(m_detailRewardEstimateSol), formatCountdown(remainingSecs));
        m_detailLifecycle->setFooterPending(false);
        m_detailLifecycle->setFooterText(footer);
    }

    if (!m_detailEtaTimer->isActive()) {
        m_detailEtaTimer->start();
    }
}

void StakingPage::requestStakeDetailRewards() {
    m_detailEpochRewards.clear();
    m_detailPendingRewardEpochs.clear();

    const QList<StakeRewardInfo> cachedRewards =
        StakeAccountRewardDb::load(m_selectedStakeAccount.address);
    for (const auto& reward : cachedRewards) {
        m_detailEpochRewards.insert(reward.epoch, reward);
    }

    if (!m_solanaApi || m_selectedStakeAccount.address.isEmpty() || m_currentEpoch == 0) {
        refreshStakeDetailRewardTotal();
        renderStakeDetailRewards();
        return;
    }

    constexpr quint64 maxU64 = std::numeric_limits<quint64>::max();
    if (m_selectedStakeAccount.activationEpoch == maxU64) {
        refreshStakeDetailRewardTotal();
        renderStakeDetailRewards();
        return;
    }

    const quint64 startEpoch = m_selectedStakeAccount.activationEpoch + 1;
    const quint64 endEpoch = m_currentEpoch > 0 ? m_currentEpoch - 1 : 0;
    if (startEpoch > endEpoch) {
        refreshStakeDetailRewardTotal();
        renderStakeDetailRewards();
        return;
    }

    quint64 fetchFromEpoch = startEpoch;
    if (const auto maxCachedEpoch = StakeAccountRewardDb::maxEpoch(m_selectedStakeAccount.address);
        maxCachedEpoch.has_value() && *maxCachedEpoch >= fetchFromEpoch) {
        fetchFromEpoch = *maxCachedEpoch + 1;
    }

    refreshStakeDetailRewardTotal();
    renderStakeDetailRewards();

    if (fetchFromEpoch > endEpoch) {
        return;
    }

    for (quint64 epoch = fetchFromEpoch; epoch <= endEpoch; ++epoch) {
        m_detailPendingRewardEpochs.insert(epoch);
        m_solanaApi->fetchInflationReward(m_selectedStakeAccount.address, epoch);
    }
}

void StakingPage::refreshStakeDetailRewardTotal() {
    if (m_selectedStakeAccount.address.isEmpty()) {
        return;
    }

    quint64 totalLamports = 0;
    for (auto it = m_detailEpochRewards.constBegin(); it != m_detailEpochRewards.constEnd(); ++it) {
        if (it.value().lamports > 0) {
            totalLamports += static_cast<quint64>(it.value().lamports);
        }
    }

    m_selectedStakeAccount.totalRewardsLamports = totalLamports;
    StakeAccountDb::setTotalRewardsLamports(m_selectedStakeAccount.address,
                                            m_selectedStakeAccount.totalRewardsLamports);
    if (m_detailTotalRewards) {
        m_detailTotalRewards->setText(formatRewardSol(m_selectedStakeAccount.totalRewardsSol()));
    }
}

void StakingPage::renderStakeDetailRewards() {
    if (!m_detailRewardsLayout) {
        return;
    }

    QLayoutItem* child;
    while ((child = m_detailRewardsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    int rewardCount = 0;
    for (auto it = m_detailEpochRewards.constEnd(); it != m_detailEpochRewards.constBegin();) {
        --it;
        const StakeRewardInfo& reward = it.value();
        auto* row = new CardListItem();
        row->setIcon(QString::fromUtf8("\xe2\x9c\xa6"), "stakeStateIcon",
                     "qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                     " stop:0 rgba(16,185,129,0.45), stop:1 rgba(14,165,233,0.45))");
        row->setTitle(tr("Staking Reward"));
        row->setSubtitle(
            tr("Epoch %1  ·  Slot %2")
                .arg(QString::number(reward.epoch), QString::number(reward.effectiveSlot)));

        const double rewardSol = static_cast<double>(reward.lamports) / 1e9;
        const QString rewardText =
            (reward.lamports > 0 ? "+" : "") + formatRewardSol(std::abs(rewardSol));
        row->setValue(rewardText, reward.lamports >= 0 ? "stakingDetailDeltaPositive"
                                                       : "stakingDetailDeltaNegative");
        row->setSubValue(
            tr("Post Balance %1").arg(formatSol(static_cast<double>(reward.postBalance) / 1e9)),
            "listItemSubtitle");
        m_detailRewardsLayout->addWidget(row);
        rewardCount++;
    }

    if (rewardCount > 0) {
        m_detailRewardsStatus->hide();
        return;
    }

    if (!m_detailPendingRewardEpochs.isEmpty()) {
        m_detailRewardsStatus->setText(tr("Loading rewards..."));
        m_detailRewardsStatus->setProperty("tone", "muted");
    } else {
        m_detailRewardsStatus->setText(tr("No staking rewards found."));
        m_detailRewardsStatus->setProperty("tone", "muted");
    }
    m_detailRewardsStatus->style()->unpolish(m_detailRewardsStatus);
    m_detailRewardsStatus->style()->polish(m_detailRewardsStatus);
    m_detailRewardsStatus->show();
}

// ── Stake Dialog ─────────────────────────────────────────────────

void StakingPage::showStakeDialog(const ValidatorInfo& validator) {
    m_selectedValidator = validator;

    // Name (fallback to vote account)
    QString displayName = validator.name.isEmpty() ? validator.voteAccount : validator.name;
    m_formValidatorName->setText(displayName);

    // Icon
    if (m_avatarCache && !validator.avatarUrl.isEmpty()) {
        QPixmap pm = m_avatarCache->get(validator.avatarUrl);
        if (!pm.isNull()) {
            qreal dpr = devicePixelRatioF();
            m_formIcon->setPixmap(AvatarCache::circleClip(pm, 44, dpr));
            m_formIcon->setText({});
            m_formIcon->setProperty("hasAvatar", true);
            m_formIcon->style()->unpolish(m_formIcon);
            m_formIcon->style()->polish(m_formIcon);
        } else {
            m_formIcon->setText(displayName.left(1).toUpper());
            m_formIcon->setProperty("hasAvatar", false);
            m_formIcon->style()->unpolish(m_formIcon);
            m_formIcon->style()->polish(m_formIcon);
        }
    } else {
        m_formIcon->setText(displayName.left(1).toUpper());
        m_formIcon->setProperty("hasAvatar", false);
        m_formIcon->style()->unpolish(m_formIcon);
        m_formIcon->style()->polish(m_formIcon);
    }

    // Stats
    m_formApy->setText(tr("APY: %1").arg(formatApy(validator.apy)));
    m_formCommission->setText(tr("  |  Commission: %1%").arg(validator.commission));

    // Detail fields
    m_formStake->setText(formatStake(validator.stakeInSol()));
    m_formUptime->setText(
        validator.uptimePct > 0 ? QString::number(validator.uptimePct, 'f', 2) + "%" : "--");
    m_formVersion->setText(validator.version.isEmpty() ? "--" : validator.version);
    m_formScore->setText(validator.score > 0 ? QString::number(validator.score) : "--");

    QString location;
    if (!validator.city.isEmpty()) {
        location = validator.city;
        if (!validator.country.isEmpty()) {
            location += ", " + validator.country;
        }
    } else if (!validator.country.isEmpty()) {
        location = validator.country;
    }
    m_formLocation->setText(location.isEmpty() ? "--" : location);

    m_formVoteAccount->setAddress(validator.voteAccount);
    m_formNodePubkey->setAddress(validator.nodePubkey.isEmpty() ? "--" : validator.nodePubkey);

    m_amountInput->clear();
    m_formStatus->setText(
        tr("Rent-exempt reserve: %1 SOL | Available: %2 SOL")
            .arg(QString::number(static_cast<double>(m_rentExempt) / 1e9, 'f', 6))
            .arg(QString::number(static_cast<double>(m_solBalance) / 1e9, 'f', 4)));
    m_formStatus->setProperty("tone", "muted");
    m_formStatus->style()->unpolish(m_formStatus);
    m_formStatus->style()->polish(m_formStatus);
    m_stakeBtn->setEnabled(false);
    showStep(Step::StakeForm);
}

// ── doStake ──────────────────────────────────────────────────────

void StakingPage::doStake(const QString& voteAccount, quint64 lamports) {
    m_stakingHandler->createStake({m_walletAddress, voteAccount, lamports, m_rentExempt});
}

// ── doDeactivate ─────────────────────────────────────────────────

void StakingPage::doDeactivate(const QString& stakeAccount) {
    m_stakingHandler->deactivateStake({m_walletAddress, stakeAccount});
}

// ── doWithdraw ───────────────────────────────────────────────────

void StakingPage::doWithdraw(const QString& stakeAccount, quint64 lamports) {
    m_stakingHandler->withdrawStake({m_walletAddress, stakeAccount, lamports});
}

// ── Event Filter ─────────────────────────────────────────────────

bool StakingPage::eventFilter(QObject* obj, QEvent* event) {
    // Viewport resize → relayout visible rows
    if (event->type() == QEvent::Resize && m_validatorScroll &&
        obj == m_validatorScroll->viewport()) {
        relayoutVisibleRows();
        return false;
    }

    if (event->type() != QEvent::MouseButtonRelease) {
        return QWidget::eventFilter(obj, event);
    }

    auto* widget = qobject_cast<QWidget*>(obj);
    if (!widget) {
        return QWidget::eventFilter(obj, event);
    }

    // Validator row click → open stake form
    QVariant voteVar = widget->property("voteAccount");
    if (voteVar.isValid()) {
        QString voteAccount = voteVar.toString();
        for (const auto& v : std::as_const(m_validators)) {
            if (v.voteAccount == voteAccount) {
                showStakeDialog(v);
                return true;
            }
        }
        return true;
    }

    QVariant detailSig = widget->property("detailSignature");
    if (detailSig.isValid()) {
        emit transactionClicked(detailSig.toString());
        return true;
    }

    // Stake account row click → open detail view
    QVariant stakeVar = widget->property("stakeAddress");
    if (stakeVar.isValid()) {
        openStakeDetail(stakeVar.toString());
        return true;
    }

    return QWidget::eventFilter(obj, event);
}
