#include "AgentsPage.h"
#include "ApprovalExecutor.h"
#include "Theme.h"
#include "agents/McpClient.h"
#include "agents/McpToolDefs.h"
#include "agents/ModelProvider.h"
#include "agents/models/claude/ClaudeProvider.h"
#include "agents/models/claudecodecli/ClaudeCodeCliProvider.h"
#include "agents/models/codex/CodexProvider.h"
#include "db/Database.h"
#include "db/McpDb.h"
#include "db/NotificationDb.h"
#include "util/CopyButton.h"
#include "widgets/AddressLink.h"
#include "widgets/Dropdown.h"
#include "widgets/TabBar.h"
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

// ── Style constants ──────────────────────────────────────────────

static QLabel* sectionDesc(const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setObjectName("agentsSectionDesc");
    return label;
}

static void clearLayout(QLayout* layout) {
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        } else if (item->layout()) {
            clearLayout(item->layout());
        }
        delete item;
    }
}

// ── Constructor ──────────────────────────────────────────────────

AgentsPage::AgentsPage(QWidget* parent) : QWidget(parent) {
    m_mcpBinaryPath = m_handler.mcpBinaryPath();
    m_mcpClient = new McpClient(this);
    connect(m_mcpClient, &McpClient::testComplete, this, &AgentsPage::onTestComplete);

    // Register model providers
    m_providers.append(new ClaudeProvider(this));
    m_providers.append(new ClaudeCodeCliProvider(this));
    m_providers.append(new CodexProvider(this));

    // Outer layout
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    auto* content = new QWidget();
    content->setObjectName("agentsContent");
    content->setProperty("uiClass", "content");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Title
    auto* title = new QLabel(tr("Agents"));
    title->setObjectName("dashboardTitle");
    layout->addWidget(title);

    // Tab bar
    m_tabs = new TabBar();
    m_tabs->addTab(tr("Access Policies"));
    m_tabs->addTab(tr("Approvals"));
    m_tabs->addTab(tr("Configuration"));
    m_tabs->addTab(tr("Activity"));
    layout->addWidget(m_tabs);

    // Tab content stack
    m_tabStack = new QStackedWidget();
    m_tabStack->addWidget(buildPoliciesTab());
    m_tabStack->addWidget(buildApprovalsTab());
    m_tabStack->addWidget(buildConfigurationTab());
    m_tabStack->addWidget(buildActivityTab());
    connect(m_tabs, &TabBar::currentChanged, m_tabStack, &QStackedWidget::setCurrentIndex);
    m_tabs->setActiveIndex(0);
    layout->addWidget(m_tabStack, 1);

    layout->addStretch();
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);

    // Approval polling timer (1s)
    m_approvalTimer = new QTimer(this);
    connect(m_approvalTimer, &QTimer::timeout, this, &AgentsPage::refreshApprovals);
    m_approvalTimer->start(1000);

    // Activity log refresh timer (5s)
    m_activityTimer = new QTimer(this);
    connect(m_activityTimer, &QTimer::timeout, this, &AgentsPage::pollActivityUpdates);
    m_activityTimer->start(5000);

    // Approval executor — polls for approved-but-unexecuted records and runs them
    m_executor = new ApprovalExecutor(this);
    connect(m_executor, &ApprovalExecutor::contactsChanged, this, &AgentsPage::contactsChanged);
    connect(m_executor, &ApprovalExecutor::balancesChanged, this, &AgentsPage::balancesChanged);
    connect(m_executor, &ApprovalExecutor::stakeChanged, this, &AgentsPage::stakeChanged);
    connect(m_executor, &ApprovalExecutor::nonceAccountsChanged, this,
            &AgentsPage::nonceAccountsChanged);
    m_executor->start();
}

void AgentsPage::setWalletAddress(const QString& address) {
    m_walletAddress = address;
    m_executor->setWalletAddress(address);
}

void AgentsPage::setSolanaApi(SolanaApi* api) {
    m_solanaApi = api;
    m_executor->setSolanaApi(api);
}

void AgentsPage::setSigner(Signer* signer) {
    m_signer = signer;
    m_executor->setSigner(signer);
}

void AgentsPage::setSignerFactory(ApprovalExecutor::SignerFactory factory) {
    m_executor->setSignerFactory(std::move(factory));
}

// ── Code Block Widget ────────────────────────────────────────────

QWidget* AgentsPage::buildCodeBlock(const QString& text, const QString& language) {
    auto* container = new QWidget();
    container->setProperty("uiClass", "agentsCodeBlockStrong");

    auto* containerLay = new QVBoxLayout(container);
    containerLay->setContentsMargins(0, 0, 0, 0);
    containerLay->setSpacing(0);

    // Top bar with copy button
    auto* topBar = new QWidget();
    topBar->setFixedHeight(32);
    topBar->setProperty("uiClass", "agentsCodeTopBar");

    auto* topBarLay = new QHBoxLayout(topBar);
    topBarLay->setContentsMargins(12, 0, 6, 0);
    topBarLay->setSpacing(0);

    auto* langLabel = new QLabel(language);
    langLabel->setProperty("uiClass", "agentsCodeLang");
    topBarLay->addWidget(langLabel);
    topBarLay->addStretch();

    auto* copyBtn = new QPushButton();
    copyBtn->setCursor(Qt::PointingHandCursor);
    copyBtn->setFixedSize(28, 28);
    copyBtn->setToolTip(tr("Copy to clipboard"));
    copyBtn->setProperty("uiClass", "agentsCopyIconBtn");
    copyBtn->setProperty("copied", false);
    copyBtn->setProperty("copySize", "14");

    CopyButton::applyIcon(copyBtn);

    connect(copyBtn, &QPushButton::clicked, container, [text, copyBtn]() {
        QApplication::clipboard()->setText(text);
        copyBtn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
        copyBtn->setProperty("copied", true);
        copyBtn->style()->unpolish(copyBtn);
        copyBtn->style()->polish(copyBtn);
        QTimer::singleShot(1500, copyBtn, [copyBtn]() {
            CopyButton::applyIcon(copyBtn);
            copyBtn->setProperty("copied", false);
            copyBtn->style()->unpolish(copyBtn);
            copyBtn->style()->polish(copyBtn);
        });
    });

    topBarLay->addWidget(copyBtn);
    containerLay->addWidget(topBar);

    // Code content
    auto* codeLabel = new QLabel(text);
    codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    codeLabel->setWordWrap(false);
    codeLabel->setProperty("uiClass", "agentsCodeContentPadded");
    containerLay->addWidget(codeLabel);

    return container;
}

// ── Event filter for clickable policy cards ──────────────────────

bool AgentsPage::eventFilter(QObject* obj, QEvent* event) {
    if (m_activityScroll && obj == m_activityScroll->viewport() &&
        event->type() == QEvent::Resize) {
        relayoutActivityRows();
        return false;
    }

    auto* widget = qobject_cast<QWidget*>(obj);
    if (!widget || !widget->property("policyCard").toBool()) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Enter) {
        widget->setProperty("hovered", true);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        return false;
    }
    if (event->type() == QEvent::Leave) {
        widget->setProperty("hovered", false);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        return false;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        int policyId = widget->property("policyId").toInt();
        showPolicyDetail(policyId);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// ── Policy Permission Summary ────────────────────────────────────

QString AgentsPage::policyPermissionSummary(int policyId) {
    return m_handler.policyPermissionSummary(policyId);
}

// ── Policy Detail View ───────────────────────────────────────────

void AgentsPage::showPolicyDetail(int policyId) {
    clearLayout(m_policyDetailLayout);

    auto detail = m_handler.buildPolicyDetail(policyId);
    if (!detail.has_value()) {
        return;
    }

    const QString policyName = detail->name;
    const QString apiKey = detail->apiKey;

    // Back button
    auto* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setObjectName("txBackButton");
    connect(backBtn, &QPushButton::clicked, this, &AgentsPage::showPolicyList);
    m_policyDetailLayout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Header: policy name with inline edit
    auto* nameStack = new QStackedWidget();
    nameStack->setProperty("uiClass", "agentsTransparent");
    nameStack->setFixedHeight(48);

    // Page 0: label + pencil button
    auto* displayWidget = new QWidget();
    displayWidget->setProperty("uiClass", "agentsTransparent");
    auto* displayLay = new QHBoxLayout(displayWidget);
    displayLay->setContentsMargins(0, 0, 0, 0);
    displayLay->setSpacing(8);

    auto* titleLabel = new QLabel(policyName);
    titleLabel->setProperty("uiClass", "agentsPolicyTitle");
    displayLay->addWidget(titleLabel);

    auto* editBtn = new QPushButton();
    QPixmap editPix(":/icons/action/edit.png");
    if (!editPix.isNull()) {
        qreal dpr = qApp->devicePixelRatio();
        editPix = editPix.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        editPix.setDevicePixelRatio(dpr);
        editBtn->setIcon(QIcon(editPix));
        editBtn->setIconSize(QSize(16, 16));
    }
    editBtn->setFixedSize(30, 30);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setProperty("uiClass", "agentsEditIconBtn");
    displayLay->addWidget(editBtn);
    displayLay->addStretch();

    auto* deleteBtn = new QPushButton(tr("Delete Policy"));
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setFixedHeight(30);
    deleteBtn->setFixedWidth(120);
    deleteBtn->setProperty("uiClass", "agentsDangerBtn");
    connect(deleteBtn, &QPushButton::clicked, this,
            [this, policyId]() { onDeletePolicy(policyId); });
    displayLay->addWidget(deleteBtn, 0, Qt::AlignVCenter);

    nameStack->addWidget(displayWidget);

    // Page 1: input field + blue checkmark button
    auto* editWidget = new QWidget();
    editWidget->setProperty("uiClass", "agentsTransparent");
    auto* editLay = new QHBoxLayout(editWidget);
    editLay->setContentsMargins(0, 0, 0, 0);
    editLay->setSpacing(8);

    auto* nameInput = new QLineEdit(policyName);
    nameInput->setMinimumHeight(44);
    nameInput->setProperty("uiClass", "agentsPolicyNameInput");
    QPalette namePal = nameInput->palette();
    namePal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    nameInput->setPalette(namePal);
    editLay->addWidget(nameInput);

    auto* saveBtn = new QPushButton();
    QPixmap checkPix(":/icons/ui/checkmark.svg");
    if (!checkPix.isNull()) {
        qreal dpr = qApp->devicePixelRatio();
        checkPix =
            checkPix.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        checkPix.setDevicePixelRatio(dpr);
        saveBtn->setIcon(QIcon(checkPix));
        saveBtn->setIconSize(QSize(16, 16));
    }
    saveBtn->setFixedSize(36, 36);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setProperty("uiClass", "agentsSaveIconBtn");
    editLay->addWidget(saveBtn);
    editLay->addStretch();
    nameStack->addWidget(editWidget);

    // Wire pencil -> show input
    connect(editBtn, &QPushButton::clicked, this, [nameStack, nameInput]() {
        nameStack->setCurrentIndex(1);
        nameInput->setFocus();
        nameInput->deselect();
        nameInput->setCursorPosition(nameInput->text().length());
    });

    // Wire checkmark -> save and show label
    auto saveName = [this, nameStack, nameInput, titleLabel, policyId]() {
        const QString newName = nameInput->text().trimmed();
        if (m_handler.renamePolicyIfValid(policyId, newName)) {
            titleLabel->setText(newName.trimmed());
        }
        nameStack->setCurrentIndex(0);
    };
    connect(saveBtn, &QPushButton::clicked, this, saveName);
    connect(nameInput, &QLineEdit::returnPressed, this, saveName);

    m_policyDetailLayout->addWidget(nameStack);

    // API Key row
    auto* keyRow = new QHBoxLayout();
    keyRow->setSpacing(8);

    auto* keyLabel = new QLabel(tr("API Key:"));
    keyLabel->setProperty("uiClass", "agentsKeyLabel");
    keyRow->addWidget(keyLabel);

    const bool hasVisibleApiKey = !apiKey.isEmpty();
    QString maskedKey = hasVisibleApiKey ? apiKey.left(10) + QStringLiteral("...") + apiKey.right(6)
                                         : tr("Stored securely. Regenerate to create a new key.");
    auto* keyValue = new QLabel(maskedKey);
    keyValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    keyValue->setProperty("uiClass", "agentsKeyValue");
    keyRow->addWidget(keyValue);

    auto* copyKeyBtn = new QPushButton(tr("Copy"));
    copyKeyBtn->setCursor(Qt::PointingHandCursor);
    copyKeyBtn->setFixedHeight(26);
    copyKeyBtn->setProperty("uiClass", "agentsNeutralBtn");
    copyKeyBtn->setEnabled(hasVisibleApiKey);
    connect(copyKeyBtn, &QPushButton::clicked, this, [apiKey, copyKeyBtn]() {
        QApplication::clipboard()->setText(apiKey);
        copyKeyBtn->setText(QObject::tr("Copied!"));
        QTimer::singleShot(1500, copyKeyBtn,
                           [copyKeyBtn]() { copyKeyBtn->setText(QObject::tr("Copy")); });
    });
    keyRow->addWidget(copyKeyBtn);

    auto* regenBtn = new QPushButton(tr("Regenerate"));
    regenBtn->setCursor(Qt::PointingHandCursor);
    regenBtn->setFixedHeight(26);
    regenBtn->setProperty("uiClass", "agentsNeutralBtn");
    connect(regenBtn, &QPushButton::clicked, this,
            [this, policyId]() { onRegenerateKey(policyId); });
    keyRow->addWidget(regenBtn);

    keyRow->addStretch();
    m_policyDetailLayout->addLayout(keyRow);

    // ── Divider ──────────────────────────────────────────────────
    m_policyDetailLayout->addSpacing(16);
    auto* divider1 = new QWidget();
    divider1->setObjectName("agentsDividerStrong");
    divider1->setFixedHeight(1);
    m_policyDetailLayout->addWidget(divider1);

    // ── Wallet Bindings ──────────────────────────────────────────

    m_policyDetailLayout->addSpacing(16);
    auto* walletHeader = new QLabel(tr("Wallet Bindings"));
    walletHeader->setObjectName("agentsSectionTitle");
    m_policyDetailLayout->addWidget(walletHeader);
    m_policyDetailLayout->addWidget(
        sectionDesc(tr("Select which wallets this policy can access. The policy only allows "
                       "operations on its assigned wallets.")));

    // Column headers
    auto* colRow = new QHBoxLayout();
    colRow->setContentsMargins(0, 4, 0, 0);
    auto makeColHeader = [](const QString& text, int width) {
        auto* lbl = new QLabel(text);
        lbl->setFixedWidth(width);
        lbl->setProperty("uiClass", "agentsColHeader");
        return lbl;
    };
    colRow->addWidget(makeColHeader(tr("Name"), 140));
    colRow->addWidget(makeColHeader(tr("Address"), 0));
    colRow->addStretch();
    auto* walletAccessHeader = makeColHeader(tr("Access"), 180);
    walletAccessHeader->setAlignment(Qt::AlignCenter);
    colRow->addWidget(walletAccessHeader);
    m_policyDetailLayout->addLayout(colRow);

    auto* sep = new QFrame();
    sep->setObjectName("agentsSeparator");
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    m_policyDetailLayout->addWidget(sep);

    for (const auto& wallet : detail->wallets) {
        const QString addr = wallet.address;
        const QString label = wallet.label;
        const bool bound = wallet.bound;

        auto* row = new QWidget();
        row->setProperty("uiClass", "agentsTransparent");
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 2, 0, 2);
        rowLay->setSpacing(12);

        // Name
        auto* wLabel = new QLabel(label);
        wLabel->setFixedWidth(140);
        wLabel->setProperty("uiClass", "agentsWalletLabel");
        rowLay->addWidget(wLabel);

        // Address (AddressLink widget with hover effect)
        auto* addrLink = new AddressLink(addr, row);
        rowLay->addWidget(addrLink, 1);

        // Access 2-segment switch (No / Yes)
        auto* segWidget = new QWidget();
        segWidget->setFixedWidth(180);
        segWidget->setProperty("uiClass", "agentsTransparent");
        auto* segLay = new QHBoxLayout(segWidget);
        segLay->setContentsMargins(0, 0, 0, 0);
        segLay->setSpacing(0);

        auto* noBtn = new QPushButton(QObject::tr("No"));
        auto* yesBtn = new QPushButton(QObject::tr("Yes"));
        for (auto* btn : {noBtn, yesBtn}) {
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(24);
            btn->setProperty("uiClass", "agentsWalletAccessBtn");
        }
        noBtn->setProperty("segmentPos", "left");
        noBtn->setProperty("tone", "danger");
        yesBtn->setProperty("segmentPos", "right");
        yesBtn->setProperty("tone", "success");
        segLay->addWidget(noBtn);
        segLay->addWidget(yesBtn);

        auto applyBindStyle = [](QPushButton* no, QPushButton* yes, bool active) {
            no->setProperty("selected", !active);
            yes->setProperty("selected", active);
            no->style()->unpolish(no);
            no->style()->polish(no);
            yes->style()->unpolish(yes);
            yes->style()->polish(yes);
        };
        applyBindStyle(noBtn, yesBtn, bound);

        connect(noBtn, &QPushButton::clicked, this,
                [this, applyBindStyle, policyId, addr, noBtn, yesBtn]() {
                    m_handler.setPolicyWalletBound(policyId, addr, false);
                    applyBindStyle(noBtn, yesBtn, false);
                });
        connect(yesBtn, &QPushButton::clicked, this,
                [this, applyBindStyle, policyId, addr, noBtn, yesBtn]() {
                    m_handler.setPolicyWalletBound(policyId, addr, true);
                    applyBindStyle(noBtn, yesBtn, true);
                });
        rowLay->addWidget(segWidget);

        m_policyDetailLayout->addWidget(row);
    }

    // ── Divider ──────────────────────────────────────────────────
    m_policyDetailLayout->addSpacing(16);
    auto* divider2 = new QWidget();
    divider2->setObjectName("agentsDividerStrong");
    divider2->setFixedHeight(1);
    m_policyDetailLayout->addWidget(divider2);

    // ── Tool Access ──────────────────────────────────────────

    m_policyDetailLayout->addSpacing(16);
    auto* resHeader = new QLabel(tr("Tool Access"));
    resHeader->setObjectName("agentsSectionTitle");
    m_policyDetailLayout->addWidget(resHeader);
    m_policyDetailLayout->addWidget(
        sectionDesc(tr("Control access for each tool. Allow executes immediately, "
                       "Approval requires manual confirmation, Blocked denies the request.")));

    // Bulk actions
    auto* bulkRow = new QHBoxLayout();
    bulkRow->setSpacing(8);

    auto* allowAllBtn = new QPushButton(tr("Allow All"));
    allowAllBtn->setCursor(Qt::PointingHandCursor);
    allowAllBtn->setFixedHeight(28);
    allowAllBtn->setProperty("uiClass", "agentsAllowAllBtn");
    bulkRow->addWidget(allowAllBtn);

    auto* approvalAllBtn = new QPushButton(tr("Approval All"));
    approvalAllBtn->setCursor(Qt::PointingHandCursor);
    approvalAllBtn->setFixedHeight(28);
    approvalAllBtn->setProperty("uiClass", "agentsApprovalAllBtn");
    bulkRow->addWidget(approvalAllBtn);

    auto* blockAllBtn = new QPushButton(tr("Block All"));
    blockAllBtn->setCursor(Qt::PointingHandCursor);
    blockAllBtn->setFixedHeight(28);
    blockAllBtn->setProperty("uiClass", "agentsDangerBtn");
    bulkRow->addWidget(blockAllBtn);

    bulkRow->addStretch();
    m_policyDetailLayout->addLayout(bulkRow);

    // Search + filter row
    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);
    filterRow->setContentsMargins(0, 4, 0, 4);

    auto* resSearchInput = new QLineEdit();
    resSearchInput->setPlaceholderText(tr("Search tools..."));
    resSearchInput->setProperty("uiClass", "agentsResourceSearch");
    resSearchInput->setFixedHeight(30);
    QPalette searchPal = resSearchInput->palette();
    searchPal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    resSearchInput->setPalette(searchPal);
    filterRow->addWidget(resSearchInput, 1);

    auto* resCategoryFilter = new Dropdown();
    resCategoryFilter->setFixedWidth(120);
    resCategoryFilter->addItem(tr("All"));
    resCategoryFilter->addItem(tr("Read"));
    resCategoryFilter->addItem(tr("Write"));
    resCategoryFilter->setCurrentItem(tr("All"));
    filterRow->addWidget(resCategoryFilter);

    m_policyDetailLayout->addLayout(filterRow);

    // Column headers
    auto* resColRow = new QHBoxLayout();
    resColRow->setContentsMargins(0, 4, 0, 0);

    auto* nameHeader = new QLabel(tr("Tool"));
    nameHeader->setFixedWidth(200);
    nameHeader->setProperty("uiClass", "agentsColHeader");
    resColRow->addWidget(nameHeader);

    auto* descHeader = new QLabel(tr("Description"));
    descHeader->setProperty("uiClass", "agentsColHeader");
    resColRow->addWidget(descHeader, 1);

    auto* accessHeader = new QLabel(tr("Access"));
    accessHeader->setFixedWidth(180);
    accessHeader->setAlignment(Qt::AlignCenter);
    accessHeader->setProperty("uiClass", "agentsColHeader");
    resColRow->addWidget(accessHeader);
    m_policyDetailLayout->addLayout(resColRow);

    auto* resSep = new QFrame();
    resSep->setObjectName("agentsSeparator");
    resSep->setFrameShape(QFrame::HLine);
    resSep->setFixedHeight(1);
    m_policyDetailLayout->addWidget(resSep);

    // 3-segment switch style helper
    auto applyAccessStyle = [](QPushButton* allow, QPushButton* approval, QPushButton* blocked,
                               AgentsHandler::AccessMode accessMode) {
        allow->setProperty("selected", accessMode == AgentsHandler::AccessMode::Allow);
        approval->setProperty("selected", accessMode == AgentsHandler::AccessMode::Approval);
        blocked->setProperty("selected", accessMode == AgentsHandler::AccessMode::Blocked);
        allow->style()->unpolish(allow);
        allow->style()->polish(allow);
        approval->style()->unpolish(approval);
        approval->style()->polish(approval);
        blocked->style()->unpolish(blocked);
        blocked->style()->polish(blocked);
    };

    // Collect tool rows for filtering
    struct ToolRowInfo {
        QWidget* widget;
        QString name;
        QString description;
        McpToolCategory category;
    };
    QList<ToolRowInfo> toolRows;

    for (const auto& tool : detail->tools) {
        const QString toolName = tool.name;
        const auto accessMode = tool.access;

        auto* row = new QWidget();
        row->setProperty("uiClass", "agentsTransparent");
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 3, 0, 3);
        rowLay->setSpacing(8);

        auto* nameLabel = new QLabel(toolName);
        nameLabel->setProperty("uiClass", "agentsResourceName");
        nameLabel->setFixedWidth(200);
        rowLay->addWidget(nameLabel);

        auto* descLabel = new QLabel(tool.description);
        descLabel->setProperty("uiClass", "agentsResourceDesc");
        rowLay->addWidget(descLabel, 1);

        toolRows.append({row, toolName, tool.description, tool.category});

        auto* segWidget = new QWidget();
        segWidget->setFixedWidth(180);
        segWidget->setProperty("uiClass", "agentsTransparent");
        auto* segLay = new QHBoxLayout(segWidget);
        segLay->setContentsMargins(0, 0, 0, 0);
        segLay->setSpacing(0);

        auto* allowBtn = new QPushButton(QObject::tr("Allow"));
        auto* approvalBtn = new QPushButton(QObject::tr("Approval"));
        auto* blockedBtn = new QPushButton(QObject::tr("Blocked"));

        for (auto* btn : {allowBtn, approvalBtn, blockedBtn}) {
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(24);
            btn->setProperty("uiClass", "agentsResourceAccessBtn");
        }
        allowBtn->setProperty("segmentPos", "left");
        allowBtn->setProperty("tone", "success");
        approvalBtn->setProperty("segmentPos", "mid");
        approvalBtn->setProperty("tone", "warning");
        blockedBtn->setProperty("segmentPos", "right");
        blockedBtn->setProperty("tone", "danger");

        segLay->addWidget(allowBtn);
        segLay->addWidget(approvalBtn);
        segLay->addWidget(blockedBtn);

        applyAccessStyle(allowBtn, approvalBtn, blockedBtn, accessMode);

        const bool hasFundRisk = tool.fundRisk;
        connect(allowBtn, &QPushButton::clicked, this,
                [this, applyAccessStyle, policyId, toolName, allowBtn, approvalBtn, blockedBtn,
                 hasFundRisk]() {
                    if (hasFundRisk) {
                        const auto currentMode = AgentsHandler::accessModeFromInt(
                            m_handler.policyToolAccess(policyId, toolName));
                        if (currentMode != AgentsHandler::AccessMode::Allow &&
                            !confirmFundRiskAllow(toolName)) {
                            return;
                        }
                    }
                    m_handler.setPolicyToolAccessMode(policyId, toolName,
                                                      AgentsHandler::AccessMode::Allow);
                    applyAccessStyle(allowBtn, approvalBtn, blockedBtn,
                                     AgentsHandler::AccessMode::Allow);
                });
        connect(approvalBtn, &QPushButton::clicked, this,
                [this, applyAccessStyle, policyId, toolName, allowBtn, approvalBtn, blockedBtn]() {
                    m_handler.setPolicyToolAccessMode(policyId, toolName,
                                                      AgentsHandler::AccessMode::Approval);
                    applyAccessStyle(allowBtn, approvalBtn, blockedBtn,
                                     AgentsHandler::AccessMode::Approval);
                });
        connect(blockedBtn, &QPushButton::clicked, this,
                [this, applyAccessStyle, policyId, toolName, allowBtn, approvalBtn, blockedBtn]() {
                    m_handler.setPolicyToolAccessMode(policyId, toolName,
                                                      AgentsHandler::AccessMode::Blocked);
                    applyAccessStyle(allowBtn, approvalBtn, blockedBtn,
                                     AgentsHandler::AccessMode::Blocked);
                });

        rowLay->addWidget(segWidget);
        m_policyDetailLayout->addWidget(row);
    }

    // Wire bulk actions
    connect(allowAllBtn, &QPushButton::clicked, this, [this, policyId]() {
        const auto dangerousNames = m_handler.fundRiskToolsNeedingAllow(policyId);
        if (!dangerousNames.isEmpty() && !confirmFundRiskAllowBulk(dangerousNames)) {
            return;
        }
        m_handler.setPolicyAllToolsMode(policyId, AgentsHandler::AccessMode::Allow);
        showPolicyDetail(policyId);
    });
    connect(approvalAllBtn, &QPushButton::clicked, this, [this, policyId]() {
        m_handler.setPolicyAllToolsMode(policyId, AgentsHandler::AccessMode::Approval);
        showPolicyDetail(policyId);
    });
    connect(blockAllBtn, &QPushButton::clicked, this, [this, policyId]() {
        m_handler.setPolicyAllToolsMode(policyId, AgentsHandler::AccessMode::Blocked);
        showPolicyDetail(policyId);
    });

    // Wire search + filter
    auto applyFilter = [toolRows, resSearchInput, resCategoryFilter]() {
        const QString query = resSearchInput->text().trimmed().toLower();
        const QString catText = resCategoryFilter->currentText();
        const int catFilter = (catText == "Read")    ? static_cast<int>(McpToolCategory::Read)
                              : (catText == "Write") ? static_cast<int>(McpToolCategory::Write)
                                                     : -1;
        for (const auto& info : toolRows) {
            bool matchesSearch = query.isEmpty() || info.name.toLower().contains(query) ||
                                 info.description.toLower().contains(query);
            bool matchesCat = catFilter < 0 || static_cast<int>(info.category) == catFilter;
            info.widget->setVisible(matchesSearch && matchesCat);
        }
    };
    connect(resSearchInput, &QLineEdit::textChanged, this, applyFilter);
    connect(resCategoryFilter, &Dropdown::itemSelected, this, applyFilter);

    // ── Bottom actions ───────────────────────────────────────────

    m_policyDetailLayout->addStretch();
    showPolicyStep(PolicyStep::Detail);
    m_tabs->hide();
}

void AgentsPage::showPolicyList() {
    refreshPolicyList();
    showPolicyStep(PolicyStep::List);
    m_tabs->show();
}

void AgentsPage::showPolicyStep(PolicyStep step) {
    m_policyStack->setCurrentIndex(static_cast<int>(step));
}

// ── Policy List ──────────────────────────────────────────────────

void AgentsPage::refreshPolicyList() {
    clearLayout(m_policyListLayout);

    auto policies = m_handler.policyCards();

    // Update tab text with policy count
    if (m_tabs) {
        int count = policies.size();
        m_tabs->setTabText(0, count > 0 ? tr("Access Policies (%1)").arg(count)
                                        : tr("Access Policies"));
    }

    if (policies.isEmpty()) {
        auto* empty = new QLabel(tr("No access policies created yet. Create a policy to generate "
                                    "an API key for AI agents."));
        empty->setWordWrap(true);
        empty->setObjectName("agentsEmptyState");
        m_policyListLayout->addWidget(empty);
        return;
    }

    QHBoxLayout* currentRow = nullptr;
    int colIndex = 0;

    for (const auto& policy : policies) {
        int id = policy.id;
        QString name = policy.name;
        QString apiKey = policy.apiKey;
        qint64 createdTs = policy.createdAt;
        int walletCount = policy.walletCount;
        int apiCalls = policy.apiCalls;
        int pending = policy.pending;

        // 2-column grid
        if (colIndex % 2 == 0) {
            currentRow = new QHBoxLayout();
            currentRow->setSpacing(16);
            m_policyListLayout->addLayout(currentRow);
        }

        auto* card = new QWidget();
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setCursor(Qt::PointingHandCursor);
        card->setProperty("policyCard", true);
        card->setProperty("policyId", id);
        card->setProperty("hovered", false);
        card->installEventFilter(this);

        auto* cardOuter = new QHBoxLayout(card);
        cardOuter->setContentsMargins(24, 20, 20, 20);
        cardOuter->setSpacing(0);

        auto* cardContent = new QWidget();
        cardContent->setProperty("uiClass", "agentsTransparent");
        auto* cardLay = new QVBoxLayout(cardContent);
        cardLay->setContentsMargins(0, 0, 0, 0);
        cardLay->setSpacing(0);

        // Policy name
        auto* labelW = new QLabel(name);
        labelW->setProperty("uiClass", "agentsPolicyName");
        cardLay->addWidget(labelW);

        cardLay->addSpacing(4);

        // Truncated API key
        QString maskedKey = apiKey.isEmpty()
                                ? tr("Stored securely")
                                : apiKey.left(10) + QStringLiteral("...") + apiKey.right(6);
        auto* keyW = new QLabel(maskedKey);
        keyW->setProperty("uiClass", "agentsPolicyKey");
        cardLay->addWidget(keyW);

        // Separator
        cardLay->addSpacing(14);
        auto* sep = new QFrame();
        sep->setObjectName("agentsSeparatorSoft");
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        cardLay->addWidget(sep);
        cardLay->addSpacing(14);

        // 2-column stats grid
        auto* gridRow1 = new QHBoxLayout();
        gridRow1->setSpacing(0);

        auto* createdCol = new QVBoxLayout();
        createdCol->setSpacing(2);
        auto* createdLabel = new QLabel(tr("Created"));
        createdLabel->setProperty("uiClass", "agentsPolicyStatLabel");
        createdCol->addWidget(createdLabel);
        auto* createdValue = new QLabel(
            createdTs > 0 ? QDateTime::fromSecsSinceEpoch(createdTs).toString("MMM d, yyyy")
                          : QStringLiteral("--"));
        createdValue->setProperty("uiClass", "agentsPolicyStatValue");
        createdCol->addWidget(createdValue);
        gridRow1->addLayout(createdCol, 1);

        auto* walletsCol = new QVBoxLayout();
        walletsCol->setSpacing(2);
        auto* walletsLabel = new QLabel(tr("Wallets"));
        walletsLabel->setProperty("uiClass", "agentsPolicyStatLabel");
        walletsCol->addWidget(walletsLabel);
        auto* walletsValue = new QLabel(QString::number(walletCount));
        walletsValue->setProperty("uiClass", "agentsPolicyStatValue");
        walletsCol->addWidget(walletsValue);
        gridRow1->addLayout(walletsCol, 1);

        cardLay->addLayout(gridRow1);
        cardLay->addSpacing(12);

        auto* gridRow2 = new QHBoxLayout();
        gridRow2->setSpacing(0);

        auto* apiCol = new QVBoxLayout();
        apiCol->setSpacing(2);
        auto* apiLabel = new QLabel(tr("API Calls"));
        apiLabel->setProperty("uiClass", "agentsPolicyStatLabel");
        apiCol->addWidget(apiLabel);
        auto* apiValue = new QLabel(QString::number(apiCalls));
        apiValue->setProperty("uiClass", "agentsPolicyStatValue");
        apiCol->addWidget(apiValue);
        gridRow2->addLayout(apiCol, 1);

        auto* pendingCol = new QVBoxLayout();
        pendingCol->setSpacing(2);
        auto* pendingLabel = new QLabel(tr("Pending"));
        pendingLabel->setProperty("uiClass", "agentsPolicyStatLabel");
        pendingCol->addWidget(pendingLabel);
        auto* pendingValue = new QLabel(QString::number(pending));
        pendingValue->setProperty("uiClass", "agentsPolicyStatValue");
        pendingValue->setProperty("pendingHot", pending > 0);
        pendingCol->addWidget(pendingValue);
        gridRow2->addLayout(pendingCol, 1);

        cardLay->addLayout(gridRow2);

        cardOuter->addWidget(cardContent, 1);

        // Chevron arrow
        auto* chevron = new QLabel(QStringLiteral("\u203A"));
        chevron->setProperty("uiClass", "agentsPolicyChevron");
        chevron->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
        cardOuter->addWidget(chevron, 0, Qt::AlignVCenter);

        currentRow->addWidget(card, 1);
        ++colIndex;
    }

    // Pad last row if odd
    if (colIndex > 0 && colIndex % 2 != 0) {
        currentRow->addStretch(1);
    }
}

void AgentsPage::onCreatePolicy() {
    const QString name = m_createNameInput ? m_createNameInput->text().trimmed() : QString();
    int id = m_handler.createPolicy(name);
    if (id > 0) {
        if (m_createNameInput) {
            m_createNameInput->clear();
        }
        rebuildConfigSnippets();
        showPolicyDetail(id);
    }
}

void AgentsPage::onDeletePolicy(int policyId) {
    m_handler.deletePolicy(policyId);
    showPolicyList();
    rebuildConfigSnippets();
}

void AgentsPage::onRegenerateKey(int policyId) {
    m_handler.regeneratePolicyKey(policyId);
    showPolicyDetail(policyId);
    rebuildConfigSnippets();
}

// ── Tab 1: Access Policies ───────────────────────────────────────

QWidget* AgentsPage::buildPoliciesTab() {
    auto* tab = new QWidget();
    tab->setProperty("uiClass", "agentsTransparent");
    auto* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(0);

    m_policyStack = new QStackedWidget();

    // ── Page 0: Policy list ──────────────────────────────────────
    auto* listPage = new QWidget();
    listPage->setProperty("uiClass", "agentsTransparent");
    auto* listLay = new QVBoxLayout(listPage);
    listLay->setContentsMargins(0, 0, 0, 0);
    listLay->setSpacing(12);

    listLay->addWidget(sectionDesc(
        tr("Create access policies with API keys. Each policy controls which wallets an agent "
           "can access and what tool permissions it has.")));

    // Server status row
    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);
    auto* statusLabel = new QLabel(tr("Server:"));
    statusLabel->setObjectName("agentsServerLabel");
    statusRow->addWidget(statusLabel);

    m_serverStatus = new QLabel();
    m_serverStatus->setObjectName("agentsServerStatus");
    bool binaryExists = QFileInfo::exists(m_mcpBinaryPath);
    if (binaryExists) {
        m_serverStatus->setText(tr("Ready"));
        m_serverStatus->setProperty("statusType", "success");
    } else {
        m_serverStatus->setText(tr("Binary not found"));
        m_serverStatus->setProperty("statusType", "error");
    }
    statusRow->addWidget(m_serverStatus);

    auto* pathLabel = new QLabel(QStringLiteral("\u2022"));
    pathLabel->setObjectName("agentsPathBullet");
    statusRow->addWidget(pathLabel);

    m_serverPath = new QLabel(m_mcpBinaryPath);
    m_serverPath->setObjectName("agentsServerPath");
    m_serverPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusRow->addWidget(m_serverPath);
    statusRow->addStretch();

    // Test button
    m_testBtn = new QPushButton(tr("Test Connection"));
    m_testBtn->setCursor(Qt::PointingHandCursor);
    m_testBtn->setFixedHeight(32);
    m_testBtn->setFixedWidth(150);
    m_testBtn->setProperty("uiClass", "agentsNeutralBtn");
    connect(m_testBtn, &QPushButton::clicked, this, &AgentsPage::onTestClicked);
    statusRow->addWidget(m_testBtn);

    listLay->addLayout(statusRow);

    // Test result
    m_testResult = new QLabel();
    m_testResult->setWordWrap(true);
    m_testResult->setObjectName("agentsTestResult");
    m_testResult->setProperty("statusType", "neutral");
    listLay->addWidget(m_testResult);

    // Create policy form
    auto* createRow = new QHBoxLayout();
    createRow->setSpacing(8);

    m_createNameInput = new QLineEdit();
    m_createNameInput->setPlaceholderText(tr("Policy name (e.g. Trading Bot)..."));
    m_createNameInput->setMinimumHeight(38);
    m_createNameInput->setMaximumHeight(38);
    m_createNameInput->setProperty("uiClass", "agentsCreateInput");
    QPalette pal = m_createNameInput->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_createNameInput->setPalette(pal);
    createRow->addWidget(m_createNameInput, 1);

    auto* createBtn = new QPushButton(tr("Create Policy"));
    createBtn->setCursor(Qt::PointingHandCursor);
    createBtn->setFixedHeight(38);
    createBtn->setFixedWidth(140);
    createBtn->setProperty("uiClass", "agentsCreateBtn");
    connect(createBtn, &QPushButton::clicked, this, &AgentsPage::onCreatePolicy);
    createRow->addWidget(createBtn);

    listLay->addLayout(createRow);

    // Policy cards container
    auto* policyGrid = new QWidget();
    policyGrid->setProperty("uiClass", "agentsTransparent");
    m_policyListLayout = new QVBoxLayout(policyGrid);
    m_policyListLayout->setContentsMargins(0, 0, 0, 0);
    m_policyListLayout->setSpacing(16);
    listLay->addWidget(policyGrid);

    // Populate
    refreshPolicyList();

    listLay->addStretch();
    m_policyStack->addWidget(listPage);

    // ── Page 1: Policy detail (populated dynamically) ────────────
    auto* detailPage = new QWidget();
    detailPage->setProperty("uiClass", "agentsTransparent");
    m_policyDetailLayout = new QVBoxLayout(detailPage);
    m_policyDetailLayout->setContentsMargins(0, 0, 0, 0);
    m_policyDetailLayout->setSpacing(12);
    m_policyStack->addWidget(detailPage);

    lay->addWidget(m_policyStack, 1);
    return tab;
}

// ── Tab 2: Approvals ─────────────────────────────────────────────

QWidget* AgentsPage::buildApprovalsTab() {
    auto* tab = new QWidget();
    tab->setProperty("uiClass", "agentsTransparent");
    auto* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(12);

    lay->addWidget(sectionDesc(
        tr("When an agent calls a write tool (send, swap, stake), it appears here for your "
           "approval. Nothing executes without your explicit consent.")));

    m_approvalsLayout = new QVBoxLayout();
    m_approvalsLayout->setSpacing(16);

    auto* empty = new QLabel(tr("No pending approvals."));
    empty->setObjectName("agentsEmptyState");
    m_approvalsLayout->addWidget(empty);

    lay->addLayout(m_approvalsLayout);
    lay->addStretch();
    return tab;
}

void AgentsPage::refreshApprovals() {
    Database::checkpoint();
    auto approvals = m_handler.pendingApprovals();

    // Notify for new approvals we haven't seen yet
    for (const auto& approval : approvals) {
        if (!m_notifiedApprovalIds.contains(approval.id)) {
            m_notifiedApprovalIds.insert(approval.id);
            QString pName = m_handler.policyNameById(approval.policyId);
            QString title =
                pName.isEmpty() ? tr("Agent approval requested") : tr("Agent: %1").arg(pName);
            NotificationDb::insertNotification("agent_approval", title, approval.description);
            emit notificationAdded();
        }
    }

    while (m_approvalsLayout->count() > 0) {
        QLayoutItem* item = m_approvalsLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    int count = approvals.size();
    m_tabs->setTabText(1, count > 0 ? tr("Approvals (%1)").arg(count) : tr("Approvals"));

    if (approvals.isEmpty()) {
        auto* empty = new QLabel(tr("No pending approvals."));
        empty->setObjectName("agentsEmptyState");
        m_approvalsLayout->addWidget(empty);
        return;
    }

    for (const auto& approval : approvals) {
        auto* card = new QWidget();
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setProperty("uiClass", "agentsApprovalCard");

        auto* cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(20, 16, 20, 16);
        cardLay->setSpacing(8);

        // Policy name
        int pId = approval.policyId;
        QString pName = m_handler.policyNameById(pId);
        QString titleText = pName.isEmpty()
                                ? tr("Request: %1").arg(approval.description)
                                : tr("%1 wants to: %2").arg(pName, approval.description);
        auto* titleLabel = new QLabel(titleText);
        titleLabel->setWordWrap(true);
        titleLabel->setProperty("uiClass", "agentsApprovalTitle");
        cardLay->addWidget(titleLabel);

        auto* resLabel = new QLabel(tr("Tool: %1").arg(approval.toolName));
        resLabel->setProperty("uiClass", "agentsApprovalMeta");
        cardLay->addWidget(resLabel);

        // Show full wallet address
        QString walletAddr = approval.walletAddress;
        if (!walletAddr.isEmpty()) {
            auto* walletLabel = new QLabel(tr("From: %1").arg(walletAddr));
            walletLabel->setProperty("uiClass", "agentsApprovalMeta");
            walletLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            cardLay->addWidget(walletLabel);
        }

        // JSON arguments — formatted code block with copy button
        QString argsRaw = approval.arguments;
        QJsonDocument argsDoc = QJsonDocument::fromJson(argsRaw.toUtf8());
        QString prettyJson = argsDoc.isNull() ? argsRaw : argsDoc.toJson(QJsonDocument::Indented);

        auto* argsBlock = new QWidget();
        argsBlock->setProperty("uiClass", "agentsCodeBlock");
        auto* argsBlockLay = new QVBoxLayout(argsBlock);
        argsBlockLay->setContentsMargins(0, 0, 0, 0);
        argsBlockLay->setSpacing(0);

        // Top bar with "json" label + copy button
        auto* argsTopBar = new QWidget();
        argsTopBar->setFixedHeight(28);
        argsTopBar->setProperty("uiClass", "agentsCodeTopBar");
        auto* argsTopLay = new QHBoxLayout(argsTopBar);
        argsTopLay->setContentsMargins(10, 0, 6, 0);

        auto* jsonLabel = new QLabel(QStringLiteral("json"));
        jsonLabel->setProperty("uiClass", "agentsCodeLang");
        argsTopLay->addWidget(jsonLabel);
        argsTopLay->addStretch();

        auto* argsCopyBtn = new QPushButton();
        argsCopyBtn->setCursor(Qt::PointingHandCursor);
        argsCopyBtn->setFixedSize(24, 24);
        argsCopyBtn->setToolTip(tr("Copy to clipboard"));
        argsCopyBtn->setProperty("uiClass", "agentsCopyIconBtn");
        argsCopyBtn->setProperty("copied", false);
        argsCopyBtn->setProperty("copySize", "13");
        CopyButton::applyIcon(argsCopyBtn);
        connect(argsCopyBtn, &QPushButton::clicked, argsBlock, [prettyJson, argsCopyBtn]() {
            QApplication::clipboard()->setText(prettyJson);
            argsCopyBtn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
            argsCopyBtn->setProperty("copied", true);
            argsCopyBtn->style()->unpolish(argsCopyBtn);
            argsCopyBtn->style()->polish(argsCopyBtn);
            QTimer::singleShot(1500, argsCopyBtn, [argsCopyBtn]() {
                CopyButton::applyIcon(argsCopyBtn);
                argsCopyBtn->setProperty("copied", false);
                argsCopyBtn->style()->unpolish(argsCopyBtn);
                argsCopyBtn->style()->polish(argsCopyBtn);
            });
        });
        argsTopLay->addWidget(argsCopyBtn);
        argsBlockLay->addWidget(argsTopBar);

        // Syntax-colored JSON content with preserved indentation
        QString coloredJson;
        for (const QString& line : prettyJson.split('\n')) {
            // Preserve leading whitespace as &nbsp;
            int indent = 0;
            while (indent < line.size() && line[indent] == ' ') {
                ++indent;
            }
            QString prefix;
            for (int s = 0; s < indent; ++s) {
                prefix += QStringLiteral("&nbsp;");
            }

            QString trimmed = line.mid(indent).toHtmlEscaped();
            // Color string values (after colon)
            trimmed.replace(
                QRegularExpression(R"(: &quot;(.*?)&quot;)"),
                QStringLiteral(": <span style='color:#a5d6a7;'>&quot;\\1&quot;</span>"));
            // Color keys
            trimmed.replace(QRegularExpression(R"(&quot;(\w+)&quot;:)"),
                            QStringLiteral("<span style='color:#82b1ff;'>&quot;\\1&quot;</span>:"));
            // Color numbers
            trimmed.replace(QRegularExpression(R"(: (\d+\.?\d*))"),
                            QStringLiteral(": <span style='color:#f48fb1;'>\\1</span>"));
            coloredJson += prefix + trimmed + QStringLiteral("<br>");
        }

        auto* argsCode = new QLabel(coloredJson);
        argsCode->setTextFormat(Qt::RichText);
        argsCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
        argsCode->setWordWrap(true);
        argsCode->setProperty("uiClass", "agentsCodeContent");
        argsBlockLay->addWidget(argsCode);
        cardLay->addWidget(argsBlock);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);

        QString approvalId = approval.id;

        auto* approveBtn = new QPushButton(tr("Approve"));
        approveBtn->setCursor(Qt::PointingHandCursor);
        approveBtn->setFixedHeight(36);
        approveBtn->setProperty("uiClass", "agentsApproveBtn");
        connect(approveBtn, &QPushButton::clicked, this,
                [this, approvalId]() { onApprove(approvalId); });
        btnRow->addWidget(approveBtn);

        auto* rejectBtn = new QPushButton(tr("Reject"));
        rejectBtn->setCursor(Qt::PointingHandCursor);
        rejectBtn->setFixedHeight(36);
        rejectBtn->setProperty("uiClass", "agentsDangerBtn");
        connect(rejectBtn, &QPushButton::clicked, this,
                [this, approvalId]() { onReject(approvalId); });
        btnRow->addWidget(rejectBtn);

        btnRow->addStretch();
        cardLay->addLayout(btnRow);
        m_approvalsLayout->addWidget(card);
    }
}

void AgentsPage::onApprove(const QString& id) {
    m_handler.approve(id);
    refreshApprovals();
    refreshPolicyList();
}

void AgentsPage::onReject(const QString& id) {
    m_handler.reject(id);
    refreshApprovals();
    refreshPolicyList();
}

// ── Tab 3: Configuration ─────────────────────────────────────────

QWidget* AgentsPage::buildConfigurationTab() {
    auto* tab = new QWidget();
    tab->setProperty("uiClass", "agentsTransparent");
    auto* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(16);

    // Inline row: "Showing configuration for" [Dropdown]
    auto* policyRow = new QHBoxLayout();
    policyRow->setContentsMargins(0, 0, 0, 0);
    policyRow->setSpacing(10);

    auto* configLabel = new QLabel(tr("Showing configuration for"));
    configLabel->setObjectName("agentsSectionDesc");
    policyRow->addWidget(configLabel);

    m_configPolicyDropdown = new Dropdown(tab);
    m_configPolicyDropdown->setFixedWidth(220);
    policyRow->addWidget(m_configPolicyDropdown);

    policyRow->addStretch();
    lay->addLayout(policyRow);

    connect(m_configPolicyDropdown, &Dropdown::itemSelected, this, [this](const QString& text) {
        Q_UNUSED(text);
        rebuildConfigSnippets();
    });

    // Populate dropdown + set API keys on providers
    m_configPolicies = m_handler.policyCards();
    for (const auto& p : m_configPolicies) {
        m_configPolicyDropdown->addItem(p.name.isEmpty() ? tr("Policy %1").arg(p.id) : p.name);
    }
    if (!m_configPolicies.isEmpty()) {
        const auto& first = m_configPolicies.first();
        m_configPolicyDropdown->setCurrentItem(first.name.isEmpty() ? tr("Policy %1").arg(first.id)
                                                                    : first.name);
        for (auto* provider : m_providers) {
            provider->setPolicyApiKey(first.apiKey);
        }
    }

    for (auto* provider : m_providers) {
        lay->addWidget(buildProviderCard(provider));
    }

    lay->addStretch();
    return tab;
}

QWidget* AgentsPage::buildProviderCard(ModelProvider* provider) {
    auto* card = new QWidget();
    card->setProperty("uiClass", "agentsProviderCard");

    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(20, 16, 20, 16);
    cardLay->setSpacing(10);

    auto* name = new QLabel(provider->displayName());
    name->setProperty("uiClass", "agentsProviderName");
    cardLay->addWidget(name);

    auto* configLabel =
        new QLabel(tr("Add to your %1 configuration:").arg(provider->displayName()));
    configLabel->setProperty("uiClass", "agentsProviderConfigLabel");
    cardLay->addWidget(configLabel);

    auto* codeBlock = buildCodeBlock(provider->configSnippet(), provider->configFormat());
    m_codeBlocks[provider->id()] = codeBlock;
    cardLay->addWidget(codeBlock);

    return card;
}

void AgentsPage::rebuildConfigSnippets() {
    // Refresh policy list and dropdown
    m_configPolicies = m_handler.policyCards();

    if (m_configPolicyDropdown) {
        QString prevSelection = m_configPolicyDropdown->currentText();
        m_configPolicyDropdown->clear();

        for (const auto& p : m_configPolicies) {
            QString label = p.name.isEmpty() ? tr("Policy %1").arg(p.id) : p.name;
            m_configPolicyDropdown->addItem(label);
        }

        // Restore previous selection if still valid, else select first
        bool restored = false;
        for (const auto& p : m_configPolicies) {
            QString label = p.name.isEmpty() ? tr("Policy %1").arg(p.id) : p.name;
            if (label == prevSelection) {
                m_configPolicyDropdown->setCurrentItem(label);
                restored = true;
                break;
            }
        }
        if (!restored && !m_configPolicies.isEmpty()) {
            const auto& first = m_configPolicies.first();
            m_configPolicyDropdown->setCurrentItem(
                first.name.isEmpty() ? tr("Policy %1").arg(first.id) : first.name);
        }
    }

    // Find API key for the currently selected policy
    QString selectedApiKey;
    if (m_configPolicyDropdown) {
        QString sel = m_configPolicyDropdown->currentText();
        for (const auto& p : m_configPolicies) {
            QString label = p.name.isEmpty() ? tr("Policy %1").arg(p.id) : p.name;
            if (label == sel) {
                selectedApiKey = p.apiKey;
                break;
            }
        }
    }

    // Update all providers with the selected policy's API key
    for (auto* provider : m_providers) {
        provider->setPolicyApiKey(selectedApiKey);
    }

    // Rebuild code blocks
    for (auto* provider : m_providers) {
        QWidget* block = m_codeBlocks.value(provider->id());
        if (!block) {
            continue;
        }

        auto* parent = block->parentWidget();
        auto* parentLay = qobject_cast<QVBoxLayout*>(parent ? parent->layout() : nullptr);
        if (!parentLay) {
            continue;
        }

        int idx = parentLay->indexOf(block);
        if (idx < 0) {
            continue;
        }

        parentLay->removeWidget(block);
        block->deleteLater();

        auto* newBlock = buildCodeBlock(provider->configSnippet(), provider->configFormat());
        m_codeBlocks[provider->id()] = newBlock;
        parentLay->insertWidget(idx, newBlock);
    }
}

// ── Tab 4: Activity ──────────────────────────────────────────────

QWidget* AgentsPage::buildActivityTab() {
    auto* tab = new QWidget();
    tab->setProperty("uiClass", "agentsTransparent");
    auto* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(8);

    auto* headerRow = new QHBoxLayout();
    headerRow->addWidget(sectionDesc(
        tr("Every MCP tool invocation is logged here with timestamp, duration, and status.")));
    headerRow->addStretch();

    m_activityClearBtn = new QPushButton(tr("Clear Log"));
    m_activityClearBtn->setCursor(Qt::PointingHandCursor);
    m_activityClearBtn->setFixedHeight(32);
    m_activityClearBtn->setProperty("uiClass", "agentsDangerBtn");
    connect(m_activityClearBtn, &QPushButton::clicked, this, [this]() {
        m_handler.clearActivityLog();
        m_activityCache.clear();
        m_activityTotalCount = 0;
        m_activityAllLoaded = true;
        for (auto it = m_activeActivityRows.begin(); it != m_activeActivityRows.end(); ++it) {
            it.value()->setVisible(false);
        }
        m_activeActivityRows.clear();
        m_activityContainer->setMinimumHeight(1);
        if (m_activityTableHeader) {
            m_activityTableHeader->hide();
        }
        if (m_activityBodyStack) {
            m_activityBodyStack->setCurrentIndex(0);
        }
        if (m_activityClearBtn) {
            m_activityClearBtn->setEnabled(false);
        }
    });
    headerRow->addWidget(m_activityClearBtn);
    lay->addLayout(headerRow);

    m_activityTableHeader = new QWidget();
    m_activityTableHeader->setProperty("uiClass", "agentsTransparent");
    auto* tableHeaderLayout = new QVBoxLayout(m_activityTableHeader);
    tableHeaderLayout->setContentsMargins(0, 0, 0, 0);
    tableHeaderLayout->setSpacing(0);

    // Column headers
    auto* colRow = new QHBoxLayout();
    colRow->setContentsMargins(0, 4, 0, 0);
    colRow->setSpacing(12);

    auto makeColHeader = [](const QString& text, int width) {
        auto* label = new QLabel(text);
        label->setFixedWidth(width);
        label->setProperty("uiClass", "agentsColHeader");
        return label;
    };

    colRow->addWidget(makeColHeader(tr("Time"), 70));
    colRow->addWidget(makeColHeader(tr(""), 16));
    colRow->addWidget(makeColHeader(tr("Policy"), 100));
    colRow->addWidget(makeColHeader(tr("Tool"), 180));
    colRow->addWidget(makeColHeader(tr("Wallet"), 80));
    colRow->addWidget(makeColHeader(tr("Duration"), 60));
    colRow->addWidget(makeColHeader(tr("Result"), 80));
    colRow->addStretch();
    tableHeaderLayout->addLayout(colRow);

    auto* sep = new QFrame();
    sep->setObjectName("agentsSeparator");
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    tableHeaderLayout->addWidget(sep);
    lay->addWidget(m_activityTableHeader);

    m_activityBodyStack = new QStackedWidget();
    m_activityBodyStack->setProperty("uiClass", "agentsTransparent");
    lay->addWidget(m_activityBodyStack, 1);

    auto* emptyPage = new QWidget();
    emptyPage->setProperty("uiClass", "agentsTransparent");
    auto* emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->addStretch();
    m_activityEmpty =
        new QLabel(tr("No MCP activity yet. Connect an AI assistant to get started."));
    m_activityEmpty->setObjectName("agentsEmptyState");
    m_activityEmpty->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_activityEmpty, 0, Qt::AlignCenter);
    emptyLayout->addStretch();
    m_activityBodyStack->addWidget(emptyPage);

    // Virtualized scroll area
    m_activityScroll = new QScrollArea();
    m_activityScroll->setWidgetResizable(true);
    m_activityScroll->setFrameShape(QFrame::NoFrame);
    m_activityScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_activityScroll->viewport()->setProperty("uiClass", "agentsActivityBg");
    m_activityScroll->viewport()->setAttribute(Qt::WA_StyledBackground, true);
    m_activityScroll->viewport()->installEventFilter(this);

    m_activityContainer = new QWidget();
    m_activityContainer->setProperty("uiClass", "agentsActivityBg");
    m_activityContainer->setAttribute(Qt::WA_StyledBackground, true);
    m_activityScroll->setWidget(m_activityContainer);

    // Pre-create pool rows
    for (int i = 0; i < ACTIVITY_POOL_SIZE; ++i) {
        QWidget* row = createActivityPoolRow();
        row->setParent(m_activityContainer);
        row->setVisible(false);
        m_activityRowPool.append(row);
    }

    connect(m_activityScroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &AgentsPage::relayoutActivityRows);

    m_activityBodyStack->addWidget(m_activityScroll);

    // Initial load
    QTimer::singleShot(100, this, &AgentsPage::loadInitialActivity);

    return tab;
}

// ── Activity Log — Virtualized Pool ──────────────────────────────

QWidget* AgentsPage::createActivityPoolRow() {
    auto* row = new QWidget();
    row->setProperty("uiClass", "agentsTransparent");
    row->setFixedHeight(ACTIVITY_ROW_H);
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 2, 0, 2);
    rowLay->setSpacing(12);

    auto makeLabel = [&](const QString& name, int width, const QString& uiClass) {
        auto* label = new QLabel();
        label->setObjectName(name);
        label->setFixedWidth(width);
        label->setProperty("uiClass", uiClass);
        rowLay->addWidget(label);
    };

    makeLabel(QStringLiteral("actTime"), 70, QStringLiteral("agentsActivitySecondary"));
    makeLabel(QStringLiteral("actStatus"), 16, QStringLiteral("agentsActivityStatus"));
    makeLabel(QStringLiteral("actPolicy"), 100, QStringLiteral("agentsActivityPrimary"));
    makeLabel(QStringLiteral("actResource"), 180, QStringLiteral("agentsActivityResource"));
    makeLabel(QStringLiteral("actWallet"), 80, QStringLiteral("agentsActivitySecondary"));
    makeLabel(QStringLiteral("actDuration"), 60, QStringLiteral("agentsActivitySecondary"));

    auto* resultLink = new QPushButton(tr("View Result"));
    resultLink->setObjectName(QStringLiteral("actResult"));
    resultLink->setFixedWidth(80);
    resultLink->setCursor(Qt::PointingHandCursor);
    resultLink->setProperty("uiClass", QStringLiteral("agentsActivityResultLink"));
    resultLink->setVisible(false);
    rowLay->addWidget(resultLink);

    rowLay->addStretch();
    return row;
}

void AgentsPage::bindActivityRow(QWidget* row, int cacheIndex) {
    const McpActivityRecord& activity = m_activityCache[cacheIndex];

    qint64 ts = activity.createdAt;
    row->findChild<QLabel*>(QStringLiteral("actTime"))
        ->setText(QDateTime::fromSecsSinceEpoch(ts).toString("hh:mm:ss"));

    bool success = activity.success;
    auto* statusDot = row->findChild<QLabel*>(QStringLiteral("actStatus"));
    statusDot->setText(success ? QStringLiteral("\u2022") : QStringLiteral("\u2716"));
    statusDot->setProperty("statusOk", success);
    statusDot->style()->unpolish(statusDot);
    statusDot->style()->polish(statusDot);

    int pId = activity.policyId;
    QString pName = (pId > 0 && m_policyNameCache.contains(pId)) ? m_policyNameCache[pId]
                                                                 : QStringLiteral("\u2014");
    row->findChild<QLabel*>(QStringLiteral("actPolicy"))->setText(pName);

    row->findChild<QLabel*>(QStringLiteral("actResource"))->setText(activity.toolName);

    QString walletAddr = activity.walletAddress;
    row->findChild<QLabel*>(QStringLiteral("actWallet"))
        ->setText(walletAddr.isEmpty()
                      ? QStringLiteral("\u2014")
                      : walletAddr.left(4) + QStringLiteral("..") + walletAddr.right(4));

    row->findChild<QLabel*>(QStringLiteral("actDuration"))
        ->setText(QStringLiteral("%1ms").arg(activity.durationMs));

    auto* resultLink = row->findChild<QPushButton*>(QStringLiteral("actResult"));
    if (resultLink) {
        resultLink->disconnect();
        bool hasResult = !activity.result.isEmpty();
        resultLink->setVisible(hasResult);
        if (hasResult) {
            QString resultJson = activity.result;
            connect(resultLink, &QPushButton::clicked, this,
                    [this, resultJson]() { showResultDialog(resultJson); });
        }
    }

    row->move(0, cacheIndex * ACTIVITY_ROW_H);
    row->resize(m_activityScroll->viewport()->width(), ACTIVITY_ROW_H);
    row->setVisible(true);
}

void AgentsPage::relayoutActivityRows() {
    int totalRows = m_activityCache.size();
    if (totalRows == 0) {
        for (auto it = m_activeActivityRows.begin(); it != m_activeActivityRows.end(); ++it) {
            it.value()->setVisible(false);
        }
        m_activeActivityRows.clear();
        return;
    }

    int scrollY = m_activityScroll->verticalScrollBar()->value();
    int viewportH = m_activityScroll->viewport()->height();

    int firstVisible = scrollY / ACTIVITY_ROW_H;
    int lastVisible = qMin((scrollY + viewportH) / ACTIVITY_ROW_H, totalRows - 1);
    int firstBuffered = qMax(0, firstVisible - ACTIVITY_BUFFER);
    int lastBuffered = qMin(totalRows - 1, lastVisible + ACTIVITY_BUFFER);

    // Fetch more if near bottom of loaded data
    if (!m_activityAllLoaded && lastBuffered >= totalRows - ACTIVITY_BUFFER) {
        loadMoreActivity();
        totalRows = m_activityCache.size();
        lastBuffered = qMin(totalRows - 1, lastVisible + ACTIVITY_BUFFER);
    }

    // Recycle rows outside the buffered range
    QList<int> toRemove;
    for (auto it = m_activeActivityRows.begin(); it != m_activeActivityRows.end(); ++it) {
        if (it.key() < firstBuffered || it.key() > lastBuffered) {
            it.value()->setVisible(false);
            toRemove.append(it.key());
        }
    }
    for (int idx : toRemove) {
        m_activeActivityRows.remove(idx);
    }

    // Build free pool
    QSet<QWidget*> used;
    for (auto it = m_activeActivityRows.constBegin(); it != m_activeActivityRows.constEnd(); ++it) {
        used.insert(it.value());
    }

    QList<QWidget*> freeRows;
    for (QWidget* w : std::as_const(m_activityRowPool)) {
        if (!used.contains(w)) {
            freeRows.append(w);
        }
    }

    int freeIdx = 0;
    int containerW = m_activityScroll->viewport()->width();

    for (int i = firstBuffered; i <= lastBuffered; ++i) {
        if (i >= totalRows) {
            break;
        }
        if (m_activeActivityRows.contains(i)) {
            QWidget* row = m_activeActivityRows[i];
            row->move(0, i * ACTIVITY_ROW_H);
            row->resize(containerW, ACTIVITY_ROW_H);
            continue;
        }
        if (freeIdx >= freeRows.size()) {
            break;
        }
        QWidget* row = freeRows[freeIdx++];
        bindActivityRow(row, i);
        m_activeActivityRows[i] = row;
    }
}

void AgentsPage::loadInitialActivity() {
    refreshPolicyNameCache();

    m_activityTotalCount = m_handler.activityCount();
    m_activityCache = m_handler.activityPage(-1, ACTIVITY_PAGE_SIZE);
    m_activityAllLoaded = (m_activityCache.size() >= m_activityTotalCount);

    int totalH = m_activityTotalCount * ACTIVITY_ROW_H;
    m_activityContainer->setMinimumHeight(qMax(totalH, 1));

    if (m_activityTableHeader) {
        m_activityTableHeader->setVisible(m_activityTotalCount > 0);
    }
    if (m_activityBodyStack) {
        m_activityBodyStack->setCurrentIndex(m_activityTotalCount > 0 ? 1 : 0);
    }
    if (m_activityClearBtn) {
        m_activityClearBtn->setEnabled(m_activityTotalCount > 0);
    }

    relayoutActivityRows();
}

void AgentsPage::loadMoreActivity() {
    if (m_activityAllLoaded || m_activityCache.isEmpty()) {
        return;
    }

    qint64 lastId = m_activityCache.last().id;
    auto more = m_handler.activityPage(lastId - 1, ACTIVITY_PAGE_SIZE);

    if (more.isEmpty()) {
        m_activityAllLoaded = true;
        return;
    }

    m_activityCache.append(more);
    if (m_activityCache.size() >= m_activityTotalCount) {
        m_activityAllLoaded = true;
    }
}

void AgentsPage::pollActivityUpdates() {
    int newCount = m_handler.activityCount();
    if (newCount == m_activityTotalCount) {
        return;
    }

    int delta = newCount - m_activityTotalCount;
    if (delta > 0) {
        auto newRows = m_handler.activityPage(-1, delta);
        for (int i = newRows.size() - 1; i >= 0; --i) {
            m_activityCache.prepend(newRows[i]);
        }
    }

    m_activityTotalCount = newCount;
    m_activityContainer->setMinimumHeight(m_activityTotalCount * ACTIVITY_ROW_H);

    // Indices shifted — clear active rows and rebind
    for (auto it = m_activeActivityRows.begin(); it != m_activeActivityRows.end(); ++it) {
        it.value()->setVisible(false);
    }
    m_activeActivityRows.clear();

    if (m_activityTableHeader) {
        m_activityTableHeader->setVisible(m_activityTotalCount > 0);
    }
    if (m_activityBodyStack) {
        m_activityBodyStack->setCurrentIndex(m_activityTotalCount > 0 ? 1 : 0);
    }
    if (m_activityClearBtn) {
        m_activityClearBtn->setEnabled(m_activityTotalCount > 0);
    }

    relayoutActivityRows();
}

void AgentsPage::refreshPolicyNameCache() { m_policyNameCache = m_handler.policyNameCache(); }

// ── MCP Test Handlers ────────────────────────────────────────────

void AgentsPage::onTestClicked() {
    m_testBtn->setEnabled(false);
    m_testBtn->setText(tr("Testing..."));
    m_testResult->setText(tr("Spawning MCP server..."));
    m_testResult->setProperty("statusType", "neutral");
    m_testResult->style()->unpolish(m_testResult);
    m_testResult->style()->polish(m_testResult);
    m_mcpClient->runSelfTest(m_mcpBinaryPath);
}

void AgentsPage::onTestComplete(bool success, const QString& summary) {
    m_testBtn->setEnabled(true);
    m_testBtn->setText(tr("Test Connection"));

    if (success) {
        m_testResult->setText(summary);
        m_testResult->setProperty("statusType", "success");
        m_testResult->style()->unpolish(m_testResult);
        m_testResult->style()->polish(m_testResult);
        m_serverStatus->setText(tr("Verified"));
        m_serverStatus->setProperty("statusType", "success");
        m_serverStatus->style()->unpolish(m_serverStatus);
        m_serverStatus->style()->polish(m_serverStatus);
    } else {
        m_testResult->setText(summary);
        m_testResult->setProperty("statusType", "error");
        m_testResult->style()->unpolish(m_testResult);
        m_testResult->style()->polish(m_testResult);
        m_serverStatus->setText(tr("Error"));
        m_serverStatus->setProperty("statusType", "error");
        m_serverStatus->style()->unpolish(m_serverStatus);
        m_serverStatus->style()->polish(m_serverStatus);
    }
}

// ── Fund-Risk Confirmation Modals ─────────────────────────────────

bool AgentsPage::confirmFundRiskAllow(const QString& toolName) {
    auto* dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    dialog->setFixedSize(480, 230);

    // Inner container paints the actual background + rounded border
    auto* outer = new QVBoxLayout(dialog);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(dialog);
    card->setProperty("uiClass", "agentsConfirmCard");
    outer->addWidget(card);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(16);

    auto* title = new QLabel(tr("Allow Direct Access?"));
    title->setProperty("uiClass", "agentsConfirmTitle");
    lay->addWidget(title);

    auto* desc = new QLabel(
        tr("Allowing <b>%1</b> gives this policy permission to spend funds from your wallet "
           "without requiring approval.")
            .arg(toolName));
    desc->setProperty("uiClass", "agentsConfirmDesc");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    lay->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setProperty("uiClass", "agentsConfirmCancelBtn");
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto* confirmBtn = new QPushButton(tr("Yes, Allow"));
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setProperty("uiClass", "agentsConfirmDangerBtn");
    connect(confirmBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    btnRow->addWidget(confirmBtn);

    lay->addLayout(btnRow);

    bool accepted = dialog->exec() == QDialog::Accepted;
    dialog->deleteLater();
    return accepted;
}

bool AgentsPage::confirmFundRiskAllowBulk(const QStringList& toolNames) {
    auto* dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    dialog->setFixedSize(480, 300);

    auto* outer = new QVBoxLayout(dialog);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(dialog);
    card->setProperty("uiClass", "agentsConfirmCard");
    outer->addWidget(card);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(12);

    auto* title = new QLabel(tr("Allow Direct Access?"));
    title->setProperty("uiClass", "agentsConfirmTitle");
    lay->addWidget(title);

    auto* desc =
        new QLabel(tr("The following tools will allow the agent to spend funds without approval:"));
    desc->setProperty("uiClass", "agentsConfirmDesc");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    QString listHtml;
    for (const auto& name : toolNames) {
        listHtml += QStringLiteral("  \u2022  ") + name + QStringLiteral("\n");
    }
    auto* listLabel = new QLabel(listHtml.trimmed());
    listLabel->setProperty("uiClass", "agentsConfirmList");
    lay->addWidget(listLabel);

    lay->addStretch();

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setProperty("uiClass", "agentsConfirmCancelBtn");
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    auto* confirmBtn = new QPushButton(tr("Yes, Allow All"));
    confirmBtn->setCursor(Qt::PointingHandCursor);
    confirmBtn->setProperty("uiClass", "agentsConfirmDangerBtn");
    connect(confirmBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    btnRow->addWidget(confirmBtn);

    lay->addLayout(btnRow);

    bool accepted = dialog->exec() == QDialog::Accepted;
    dialog->deleteLater();
    return accepted;
}

void AgentsPage::showResultDialog(const QString& jsonResult) {
    // Prettify JSON
    QJsonDocument doc = QJsonDocument::fromJson(jsonResult.toUtf8());
    QString pretty =
        doc.isNull() ? jsonResult : QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

    auto* dialog = new QDialog(this);
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    dialog->setFixedSize(676, 494);

    auto* outer = new QVBoxLayout(dialog);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(dialog);
    card->setProperty("uiClass", "agentsConfirmCard");
    outer->addWidget(card);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(16);

    auto* title = new QLabel(tr("Result"));
    title->setProperty("uiClass", "agentsConfirmTitle");
    lay->addWidget(title);

    auto* textEdit = new QPlainTextEdit(pretty);
    textEdit->setReadOnly(true);
    textEdit->setProperty("uiClass", "agentsResultViewer");
    lay->addWidget(textEdit, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* copyBtn = new QPushButton(tr("Copy"));
    copyBtn->setCursor(Qt::PointingHandCursor);
    copyBtn->setFixedHeight(32);
    copyBtn->setProperty("uiClass", "agentsNeutralBtn");
    connect(copyBtn, &QPushButton::clicked, this, [pretty, copyBtn]() {
        QApplication::clipboard()->setText(pretty);
        copyBtn->setText(QObject::tr("Copied!"));
        QTimer::singleShot(1500, copyBtn, [copyBtn]() { copyBtn->setText(QObject::tr("Copy")); });
    });
    btnRow->addWidget(copyBtn);

    auto* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedHeight(32);
    closeBtn->setProperty("uiClass", "agentsNeutralBtn");
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    lay->addLayout(btnRow);

    dialog->exec();
    dialog->deleteLater();
}
