#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "services/SolanaApi.h"
#include "util/ContactResolver.h"
#include "widgets/AddressLink.h"
#include "widgets/TxStatusAnimationWidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
    constexpr int kPageMarginHPx = 40;
    constexpr int kPageMarginTopPx = 20;
    constexpr int kPageMarginBottomPx = 40;
    constexpr int kCardPaddingHPx = 16;
    constexpr int kCardPaddingVPx = 14;
    constexpr int kRowPaddingVPx = 10;
    constexpr int kBtnMinHeightPx = 48;
    constexpr int kContentMaxWidthPx = 800;
    constexpr int kLabelColWidthPx = 140;

    void refreshWidgetStyle(QWidget* widget) {
        if (!widget) {
            return;
        }
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    QFrame* makeSeparator() {
        auto* s = new QFrame();
        s->setFrameShape(QFrame::HLine);
        s->setObjectName("srSuccessSep");
        return s;
    }

    void applySuccessTone(QWidget* badge, QLabel* label, const char* tone) {
        if (!badge || !label) {
            return;
        }
        badge->setProperty("tone", tone);
        label->setProperty("tone", tone);
        refreshWidgetStyle(badge);
        refreshWidgetStyle(label);
    }
} // namespace

QWidget* SendReceivePage::buildSuccessPage() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    auto* content = new QWidget();
    content->setObjectName("sendReceiveContent");
    content->setProperty("uiClass", "content");
    auto* outerLay = new QVBoxLayout(content);
    outerLay->setContentsMargins(kPageMarginHPx, kPageMarginTopPx, kPageMarginHPx,
                                 kPageMarginBottomPx);
    outerLay->setSpacing(0);

    outerLay->addStretch(1);

    auto* centerBox = new QWidget();
    centerBox->setMaximumWidth(kContentMaxWidthPx);
    centerBox->setObjectName("srSuccessCenterBox");
    auto* layout = new QVBoxLayout(centerBox);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Animated status icon ────────────────────────────────
    m_txStatusAnim = new TxStatusAnimationWidget();
    layout->addWidget(m_txStatusAnim, 0, Qt::AlignCenter);
    layout->addSpacing(20);

    // ── Title ───────────────────────────────────────────────
    m_successTitle = new QLabel();
    m_successTitle->setAlignment(Qt::AlignCenter);
    m_successTitle->setObjectName("srSuccessTitle");
    layout->addWidget(m_successTitle);
    layout->addSpacing(6);

    // ── Amount subtitle ─────────────────────────────────────
    m_successAmount = new QLabel();
    m_successAmount->setAlignment(Qt::AlignCenter);
    m_successAmount->setObjectName("srSuccessAmount");
    layout->addWidget(m_successAmount);
    layout->addSpacing(24);

    // ── Detail card ─────────────────────────────────────────
    auto* detailCard = new QWidget();
    detailCard->setObjectName("srSuccessDetailCard");
    auto* detailLay = new QVBoxLayout(detailCard);
    detailLay->setContentsMargins(kCardPaddingHPx, kCardPaddingVPx, kCardPaddingHPx,
                                  kCardPaddingVPx);
    detailLay->setSpacing(0);

    // Helper: plain text row
    const auto addTextRow = [&](const QString& labelText, QLabel*& valueLabel) -> QWidget* {
        auto* row = new QWidget();
        row->setObjectName("srSuccessRow");
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, kRowPaddingVPx, 0, kRowPaddingVPx);
        auto* lbl = new QLabel(labelText);
        lbl->setObjectName("srSuccessLabel");
        lbl->setFixedWidth(kLabelColWidthPx);
        h->addWidget(lbl);
        valueLabel = new QLabel();
        valueLabel->setObjectName("srSuccessValue");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(valueLabel, 1);
        detailLay->addWidget(row);
        return row;
    };

    // Helper: AddressLink row
    const auto addAddressRow = [&](const QString& labelText, AddressLink*& link) -> QWidget* {
        auto* row = new QWidget();
        row->setObjectName("srSuccessRow");
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, kRowPaddingVPx, 0, kRowPaddingVPx);
        auto* lbl = new QLabel(labelText);
        lbl->setObjectName("srSuccessLabel");
        lbl->setFixedWidth(kLabelColWidthPx);
        h->addWidget(lbl);
        h->addStretch(1);
        link = new AddressLink({});
        link->setMaxDisplayChars(75);
        h->addWidget(link);
        detailLay->addWidget(row);
        return row;
    };

    // Row 1: Network Fee
    m_successNetworkFeeRow = addTextRow(tr("Network Fee"), m_successNetworkFeeVal);
    detailLay->addWidget(makeSeparator());

    // Row 2: Transaction Version
    addTextRow(tr("Transaction Version"), m_successTxVersionVal);
    detailLay->addWidget(makeSeparator());

    // Row 3: Result (badge with spinner + text)
    {
        auto* row = new QWidget();
        row->setObjectName("srSuccessRow");
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, kRowPaddingVPx, 0, kRowPaddingVPx);
        auto* lbl = new QLabel(tr("Result"));
        lbl->setObjectName("srSuccessLabel");
        lbl->setFixedWidth(kLabelColWidthPx);
        h->addWidget(lbl);
        h->addStretch();

        // Badge container (text label + spinner)
        m_confirmBadge = new QWidget();
        m_confirmBadge->setAttribute(Qt::WA_StyledBackground, true);
        m_confirmBadge->setFixedHeight(22);
        m_confirmBadge->setProperty("uiClass", "srSuccessBadge");
        m_confirmBadge->setProperty("tone", "confirming");
        auto* badgeLay = new QHBoxLayout(m_confirmBadge);
        badgeLay->setContentsMargins(8, 0, 6, 0);
        badgeLay->setSpacing(4);

        m_successResultVal = new QLabel();
        m_successResultVal->setProperty("uiClass", "srSuccessResultText");
        m_successResultVal->setProperty("tone", "confirming");
        badgeLay->addWidget(m_successResultVal, 0, Qt::AlignVCenter);

        m_confirmSpinner = new QWidget();
        m_confirmSpinner->setFixedSize(10, 10);
        m_confirmSpinner->setObjectName("srSuccessSpinner");
        m_confirmSpinner->installEventFilter(this);
        m_confirmSpinner->setVisible(false);
        badgeLay->addWidget(m_confirmSpinner, 0, Qt::AlignVCenter);

        h->addWidget(m_confirmBadge);
        detailLay->addWidget(row);
    }
    detailLay->addWidget(makeSeparator());

    // Row 4: Signer
    addAddressRow(tr("Signer"), m_successSignerLink);
    detailLay->addWidget(makeSeparator());

    // Row 5: Recipient (hidden when empty)
    m_successRecipientRow = addAddressRow(tr("Recipient"), m_successRecipientLink);
    auto* recipientSep = makeSeparator();
    detailLay->addWidget(recipientSep);
    m_successRecipientRow->setProperty("_sep",
                                       QVariant::fromValue(static_cast<QObject*>(recipientSep)));

    // Row 6: Transaction Hash
    addAddressRow(tr("Transaction Hash"), m_successSignatureLink);

    // Row 7: Block (hidden until confirmed)
    auto* blockSep = makeSeparator();
    detailLay->addWidget(blockSep);
    m_successBlockRow = addTextRow(tr("Block"), m_successBlockVal);
    m_successBlockRow->setVisible(false);
    blockSep->setVisible(false);
    m_successBlockRow->setProperty("_sep", QVariant::fromValue(static_cast<QObject*>(blockSep)));

    // Row 8: Timestamp (hidden until confirmed)
    auto* timestampSep = makeSeparator();
    detailLay->addWidget(timestampSep);
    m_successTimestampRow = addTextRow(tr("Timestamp"), m_successTimestampVal);
    m_successTimestampRow->setVisible(false);
    timestampSep->setVisible(false);
    m_successTimestampRow->setProperty("_sep",
                                       QVariant::fromValue(static_cast<QObject*>(timestampSep)));

    // Row 9: Recent Blockhash (hidden until confirmed)
    auto* blockhashSep = makeSeparator();
    detailLay->addWidget(blockhashSep);
    m_successBlockhashRow = addAddressRow(tr("Blockhash"), m_successBlockhashLink);
    m_successBlockhashRow->setVisible(false);
    blockhashSep->setVisible(false);
    m_successBlockhashRow->setProperty("_sep",
                                       QVariant::fromValue(static_cast<QObject*>(blockhashSep)));

    // Row 10: Mint Address (hidden by default)
    auto* mintSep = makeSeparator();
    detailLay->addWidget(mintSep);
    m_successMintRow = addAddressRow(tr("Mint Address"), m_successMintLink);
    m_successMintRow->setVisible(false);
    mintSep->setVisible(false);
    m_successMintRow->setProperty("_sep", QVariant::fromValue(static_cast<QObject*>(mintSep)));

    layout->addWidget(detailCard);
    layout->addSpacing(20);

    // ── Button row (View on Solscan + Done) ─────────────────
    auto* btnRow = new QWidget();
    btnRow->setObjectName("srSuccessBtnRow");
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(12);

    m_successSolscanBtn = new QPushButton(tr("View on Solscan"));
    m_successSolscanBtn->setCursor(Qt::PointingHandCursor);
    m_successSolscanBtn->setMinimumHeight(kBtnMinHeightPx);
    m_successSolscanBtn->setObjectName("srSuccessSecondaryBtn");
    connect(m_successSolscanBtn, &QPushButton::clicked, this, [this]() {
        if (!m_successSignature.isEmpty()) {
            QDesktopServices::openUrl(
                QUrl(QStringLiteral("https://solscan.io/tx/%1").arg(m_successSignature)));
        }
    });
    btnLay->addWidget(m_successSolscanBtn, 1);

    m_successDoneBtn = new QPushButton(tr("Done"));
    m_successDoneBtn->setCursor(Qt::PointingHandCursor);
    m_successDoneBtn->setMinimumHeight(kBtnMinHeightPx);
    m_successDoneBtn->setStyleSheet(Theme::primaryBtnStyle);
    connect(m_successDoneBtn, &QPushButton::clicked, this, [this]() {
        stopConfirmDots();
        if (m_confirmGuard) {
            delete m_confirmGuard;
            m_confirmGuard = nullptr;
        }
        resetForm();
        setCurrentPage(StackPage::CardGrid);
        refreshBalances();
    });
    btnLay->addWidget(m_successDoneBtn, 1);

    layout->addWidget(btnRow);

    outerLay->addWidget(centerBox, 0, Qt::AlignHCenter);
    outerLay->addStretch(1);

    scroll->setWidget(content);
    return scroll;
}

void SendReceivePage::showSuccessPage(const SendReceiveSuccessPageInfo& info) {
    // Cancel any existing polling
    stopConfirmDots();
    if (m_confirmGuard) {
        delete m_confirmGuard;
        m_confirmGuard = nullptr;
    }

    m_successTitle->setText(info.title);

    m_successAmount->setText(info.amount);
    m_successAmount->setVisible(!info.amount.isEmpty());

    // Network Fee
    const bool hasFee = !info.networkFee.isEmpty();
    m_successNetworkFeeRow->setVisible(hasFee);
    if (hasFee) {
        m_successNetworkFeeVal->setText(info.networkFee);
    }

    // Transaction Version
    m_successTxVersionVal->setText(info.txVersion.isEmpty() ? tr("Legacy") : info.txVersion);

    // Reset animation to confirming (spinning arc)
    m_txStatusAnim->reset();

    // Result — Phase 1: amber CONFIRMING badge with spinner
    m_successResultVal->setText(tr("CONFIRMING"));
    applySuccessTone(m_confirmBadge, m_successResultVal, "confirming");
    m_confirmSpinner->setVisible(true);

    // Signer
    if (!info.sender.isEmpty()) {
        m_successSignerLink->setAddress(info.sender);
        ContactResolver::resolveAddressLink(m_successSignerLink);
    }

    // Recipient row
    const bool hasRecipient = !info.recipient.isEmpty();
    m_successRecipientRow->setVisible(hasRecipient);
    if (auto* sep =
            qobject_cast<QFrame*>(m_successRecipientRow->property("_sep").value<QObject*>())) {
        sep->setVisible(hasRecipient);
    }
    if (hasRecipient) {
        m_successRecipientLink->setAddress(info.recipient);
        ContactResolver::resolveAddressLink(m_successRecipientLink);
    }

    // Signature
    m_successSignature = info.signature;
    m_successSignatureLink->setAddress(info.signature);

    // Mint row
    const bool hasMint = !info.mintAddress.isEmpty();
    m_successMintRow->setVisible(hasMint);
    if (auto* sep = qobject_cast<QFrame*>(m_successMintRow->property("_sep").value<QObject*>())) {
        sep->setVisible(hasMint);
    }
    if (hasMint) {
        m_successMintLink->setAddress(info.mintAddress);
    }

    // Hide confirmed-data rows (shown after polling confirms)
    const auto hideRowWithSep = [](QWidget* row) {
        row->setVisible(false);
        if (auto* sep = qobject_cast<QFrame*>(row->property("_sep").value<QObject*>())) {
            sep->setVisible(false);
        }
    };
    hideRowWithSep(m_successBlockRow);
    hideRowWithSep(m_successTimestampRow);
    hideRowWithSep(m_successBlockhashRow);

    setCurrentPage(StackPage::Success);
}

// ── Confirmation polling ──────────────────────────────────────────

void SendReceivePage::startConfirmationPolling(const QString& signature) {
    if (!m_solanaApi || !m_handler || signature.isEmpty()) {
        return;
    }

    // Create guard object — deleting it cancels all polling connections
    m_confirmGuard = new QObject(this);

    // Start spinning arc animation
    startConfirmDots();

    SendReceiveHandler::PollTransactionCallbacks callbacks;
    callbacks.onStatusConfirmed = [this]() {
        m_txStatusAnim->setState(TxStatusAnimationWidget::State::Confirmed);
        m_successResultVal->setText(tr("CONFIRMED"));
        applySuccessTone(m_confirmBadge, m_successResultVal, "confirmed");
    };
    callbacks.onStatusFinalized = [this]() {
        m_txStatusAnim->setState(TxStatusAnimationWidget::State::Finalized);
        stopConfirmDots();
        m_confirmSpinner->setVisible(false);
        m_successResultVal->setText(tr("FINALIZED"));
        applySuccessTone(m_confirmBadge, m_successResultVal, "finalized");
    };
    callbacks.onDetailsReady = [this](const TransactionResponse& tx) { updateStatusConfirmed(tx); };
    callbacks.onFailed = [this](const TransactionResponse& tx) { updateStatusFailed(tx); };
    callbacks.onTimeout = [this](const QString&) { updateStatusTimeout(); };

    m_handler->pollTransactionConfirmationFlow(signature, m_solanaApi, m_confirmGuard, callbacks);
}

void SendReceivePage::updateStatusConfirmed(const TransactionResponse& tx) {
    // Backfill detail rows from getTransaction response (badge already set by status callbacks)

    // Update Network Fee with actual fee from chain
    if (tx.meta.fee > 0) {
        const double feeSol = static_cast<double>(tx.meta.fee) / 1e9;
        m_successNetworkFeeVal->setText(QString::number(feeSol, 'f', 9)
                                            .remove(QRegularExpression("0+$"))
                                            .remove(QRegularExpression("\\.$")) +
                                        QStringLiteral(" SOL"));
        m_successNetworkFeeRow->setVisible(true);
    }

    // Update Tx Version from response
    if (!tx.version.isEmpty()) {
        m_successTxVersionVal->setText(tx.version == QStringLiteral("legacy") ? tr("Legacy")
                                       : tx.version == QStringLiteral("0")    ? tr("v0")
                                                                              : tx.version);
    }

    const auto showRowWithSep = [](QWidget* row) {
        row->setVisible(true);
        if (auto* sep = qobject_cast<QFrame*>(row->property("_sep").value<QObject*>())) {
            sep->setVisible(true);
        }
    };

    // Block row
    if (tx.slot > 0) {
        m_successBlockVal->setText(QString::number(tx.slot));
        showRowWithSep(m_successBlockRow);
    }

    // Timestamp row
    if (tx.blockTime > 0) {
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(tx.blockTime, QTimeZone::utc());
        m_successTimestampVal->setText(dt.toString("MMM d, yyyy h:mm:ss AP"));
        showRowWithSep(m_successTimestampRow);
    }

    // Blockhash row
    if (!tx.message.recentBlockhash.isEmpty()) {
        m_successBlockhashLink->setAddress(tx.message.recentBlockhash);
        showRowWithSep(m_successBlockhashRow);
    }
}

void SendReceivePage::updateStatusFailed(const TransactionResponse& tx) {
    m_txStatusAnim->setState(TxStatusAnimationWidget::State::Failed);
    stopConfirmDots();

    // Show on-chain data first
    updateStatusConfirmed(tx);

    // Override with red FAILED badge
    m_confirmSpinner->setVisible(false);
    m_successResultVal->setText(tr("FAILED"));
    applySuccessTone(m_confirmBadge, m_successResultVal, "failed");
}

void SendReceivePage::updateStatusTimeout() {
    stopConfirmDots();

    // Result → amber UNCONFIRMED badge (no spinner)
    m_confirmSpinner->setVisible(false);
    m_successResultVal->setText(tr("UNCONFIRMED"));
    applySuccessTone(m_confirmBadge, m_successResultVal, "confirming");
}

void SendReceivePage::startConfirmDots() {
    stopConfirmDots();
    m_confirmAngle = 0;
    m_confirmSpinner->setVisible(true);
    m_confirmSpinTimer = new QTimer(this);
    connect(m_confirmSpinTimer, &QTimer::timeout, this, [this]() {
        m_confirmAngle = (m_confirmAngle + 6) % 360;
        m_confirmSpinner->update();
    });
    m_confirmSpinTimer->start(30);
}

void SendReceivePage::stopConfirmDots() {
    if (m_confirmSpinTimer) {
        m_confirmSpinTimer->stop();
        m_confirmSpinTimer->deleteLater();
        m_confirmSpinTimer = nullptr;
    }
    m_confirmAngle = 0;
}
