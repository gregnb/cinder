#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "db/TokenAccountDb.h"
#include "features/sendreceive/CloseTokenAccountsHandler.h"
#include "widgets/StyledCheckbox.h"

#include <QCheckBox>
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
    constexpr int kPageSpacingPx = 12;
    constexpr int kSmallSpacingPx = 8;
    constexpr int kTinySpacingPx = 4;
    constexpr int kHeaderMarginHorizontalPx = 20;
    constexpr int kHeaderMarginVerticalPx = 6;
    constexpr int kCheckboxWidthPx = 30;
    constexpr int kRowMarginVerticalPx = 14;
    constexpr int kActionButtonMinHeightPx = 48;
} // namespace

QWidget* SendReceivePage::buildCloseAccountsPage() {
    m_closeAccountsScroll = new QScrollArea();
    m_closeAccountsScroll->setWidgetResizable(true);
    m_closeAccountsScroll->setFrameShape(QFrame::NoFrame);
    m_closeAccountsScroll->viewport()->setProperty("uiClass", "contentViewport");
    return m_closeAccountsScroll;
}

void SendReceivePage::populateCloseAccountsPage() {
    if (m_closeAccountsScroll->widget()) {
        delete m_closeAccountsScroll->takeWidget();
    }
    m_closeAccountCheckboxes.clear();
    m_closeAccountEntries.clear();

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

    QLabel* title = new QLabel(tr("Close Token Accounts"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    QLabel* desc = new QLabel(tr("Select empty token accounts to close and recover their rent "
                                 "deposit (~0.002 SOL each)."));
    desc->setObjectName("srSubtleDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(kTinySpacingPx);

    m_closeAccountEntries = m_closeTokenAccountsHandler->loadAccountEntries(m_walletAddress);

    if (m_closeAccountEntries.isEmpty()) {
        QLabel* emptyLabel = new QLabel(tr("No empty token accounts found."));
        emptyLabel->setObjectName("srSubtleDesc14");
        layout->addWidget(emptyLabel);
        layout->addStretch();
        m_closeAccountsScroll->setWidget(content);
        return;
    }

    QWidget* headerRow = new QWidget();
    headerRow->setObjectName("srTransparentRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(kHeaderMarginHorizontalPx, kHeaderMarginVerticalPx,
                                     kHeaderMarginHorizontalPx, kHeaderMarginVerticalPx);

    m_selectAllCheckbox = new StyledCheckbox();
    m_selectAllCheckbox->setFixedWidth(kCheckboxWidthPx);
    headerLayout->addWidget(m_selectAllCheckbox);

    auto makeHeaderLabel = [](const QString& text) {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("srCapsHeader12");
        return lbl;
    };

    headerLayout->addWidget(makeHeaderLabel(tr("Token")), 1);
    headerLayout->addWidget(makeHeaderLabel(tr("Account Address")), 3);

    QLabel* hBal = makeHeaderLabel(tr("Balance"));
    hBal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(hBal, 1);

    layout->addWidget(headerRow);

    for (const auto& acct : m_closeAccountEntries) {
        QWidget* row = new QWidget();
        row->setObjectName("closeAccountRow");
        row->setCursor(Qt::PointingHandCursor);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(kHeaderMarginHorizontalPx, kRowMarginVerticalPx,
                                      kHeaderMarginHorizontalPx, kRowMarginVerticalPx);

        StyledCheckbox* cb = new StyledCheckbox();
        cb->setFixedWidth(kCheckboxWidthPx);
        m_closeAccountCheckboxes.append(cb);
        rowLayout->addWidget(cb);

        QLabel* tokenLabel = new QLabel(acct.symbol);
        tokenLabel->setObjectName("srCloseTokenName");
        tokenLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        rowLayout->addWidget(tokenLabel, 1);

        QLabel* addrLabel = new QLabel(acct.accountAddress);
        addrLabel->setObjectName("srCloseAddress");
        addrLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        rowLayout->addWidget(addrLabel, 3);

        QLabel* balLabel = new QLabel(acct.balance);
        balLabel->setObjectName("srCloseBalance");
        balLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        balLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        rowLayout->addWidget(balLabel, 1);

        row->installEventFilter(this);

        connect(cb, &QCheckBox::toggled, this, [this]() {
            updateCloseAccountsSummary();
            int checked = 0;
            for (auto* c : m_closeAccountCheckboxes) {
                if (c->isChecked()) {
                    checked++;
                }
            }
            m_selectAllCheckbox->blockSignals(true);
            m_selectAllCheckbox->setChecked(checked == m_closeAccountCheckboxes.size());
            m_selectAllCheckbox->blockSignals(false);
        });

        layout->addWidget(row);
    }

    connect(m_selectAllCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        for (auto* cb : m_closeAccountCheckboxes) {
            cb->blockSignals(true);
            cb->setChecked(checked);
            cb->blockSignals(false);
        }
        updateCloseAccountsSummary();
    });

    layout->addSpacing(kSmallSpacingPx);

    m_closeAccountsSummary = new QLabel(tr("Select accounts to close"));
    m_closeAccountsSummary->setObjectName("srSubtleDesc14");
    layout->addWidget(m_closeAccountsSummary);

    m_closeAccountsStatusLabel = new QLabel();
    m_closeAccountsStatusLabel->setObjectName("srStatusNeutral13");
    initializeStatusLabel(m_closeAccountsStatusLabel);
    m_closeAccountsStatusLabel->setVisible(false);
    m_closeAccountsStatusLabel->setWordWrap(true);
    layout->addWidget(m_closeAccountsStatusLabel);

    m_closeAccountsBtn = new QPushButton(tr("Close Selected Accounts"));
    m_closeAccountsBtn->setCursor(Qt::PointingHandCursor);
    m_closeAccountsBtn->setMinimumHeight(kActionButtonMinHeightPx);
    m_closeAccountsBtn->setEnabled(false);
    m_closeAccountsBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(m_closeAccountsBtn, &QPushButton::clicked, this,
            &SendReceivePage::executeCloseAccounts);
    layout->addWidget(m_closeAccountsBtn);

    layout->addStretch();

    m_closeAccountsScroll->setWidget(content);
}

void SendReceivePage::updateCloseAccountsSummary() {
    int selected = 0;
    for (auto* cb : m_closeAccountCheckboxes) {
        if (cb->isChecked()) {
            selected++;
        }
    }

    m_closeAccountsSummary->setText(m_closeTokenAccountsHandler->summaryText(selected));
    const bool hasSelection = m_closeTokenAccountsHandler->hasSelection(selected);
    m_closeAccountsBtn->setEnabled(hasSelection);
    m_closeAccountsBtn->setStyleSheet(hasSelection ? Theme::primaryBtnStyle
                                                   : Theme::primaryBtnDisabledStyle);
}

void SendReceivePage::executeCloseAccounts() {
    if (!m_solanaApi || !m_signer) {
        setStatusLabelState(m_closeAccountsStatusLabel,
                            tr("Error: wallet not available for signing."), true);
        return;
    }

    // Collect selected entries
    QList<CloseTokenAccountEntry> selected;
    for (int i = 0; i < m_closeAccountCheckboxes.size() && i < m_closeAccountEntries.size(); ++i) {
        if (m_closeAccountCheckboxes[i]->isChecked()) {
            selected.append(m_closeAccountEntries[i]);
        }
    }

    if (selected.isEmpty()) {
        return;
    }

    // Cap at max per tx
    if (selected.size() > CloseTokenAccountsHandler::kMaxClosePerTx) {
        selected = selected.mid(0, CloseTokenAccountsHandler::kMaxClosePerTx);
    }

    m_closeAccountsBtn->setEnabled(false);
    m_closeAccountsBtn->setText(tr("Closing..."));
    m_closeAccountsBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    m_closeAccountsStatusLabel->setVisible(false);

    auto resetBtn = [this]() {
        m_closeAccountsBtn->setEnabled(true);
        m_closeAccountsBtn->setText(tr("Close Selected Accounts"));
        m_closeAccountsBtn->setStyleSheet(Theme::primaryBtnStyle);
    };

    const int count = selected.size();

    SendReceiveHandler::ExecuteSendCallbacks callbacks;
    callbacks.onStatus = [this](const QString& text, bool isError) {
        setStatusLabelState(m_closeAccountsStatusLabel, text, isError);
    };
    callbacks.onSuccess = [this, count, selected](const QString& txSig) {
        // Remove closed accounts from local DB so they don't reappear
        for (const auto& entry : selected) {
            TokenAccountDb::deleteAccount(entry.accountAddress);
        }

        emit transactionSent(txSig);

        SendReceiveSuccessPageInfo info;
        info.title = tr("Accounts Closed");
        info.amount = tr("%1 account%2 closed").arg(count).arg(count == 1 ? "" : "s");
        info.sender = m_walletAddress;
        info.signature = txSig;
        showSuccessPage(info);
        startConfirmationPolling(txSig);
    };
    callbacks.onFinished = [resetBtn]() { resetBtn(); };

    m_closeTokenAccountsHandler->executeCloseFlow(m_walletAddress, selected, m_solanaApi, m_signer,
                                                  this, callbacks);
}
