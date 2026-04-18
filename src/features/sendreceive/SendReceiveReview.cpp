#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "services/AvatarCache.h"
#include "services/SolanaApi.h"
#include "tx/ProgramIds.h"
#include "util/ContactResolver.h"
#include "widgets/AddressInput.h"
#include "widgets/AddressLink.h"
#include "widgets/AmountInput.h"
#include "widgets/Dropdown.h"
#include "widgets/PillButtonGroup.h"
#include "widgets/TokenDropdown.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
    constexpr int kTreeConnectorWidthPx = 28;
    constexpr qreal kTreeConnectorPenWidth = 1.5;
    constexpr int kTreeConnectorLineXPx = 10;
    constexpr int kTreeConnectorTailInsetPx = 4;

    constexpr int kPageMarginHorizontalPx = 40;
    constexpr int kPageMarginTopPx = 20;
    constexpr int kPageMarginBottomPx = 30;
    constexpr int kPageSpacingPx = 16;
    constexpr int kSmallSpacingPx = 8;
    constexpr int kTinySpacingPx = 4;

    constexpr int kExportDropdownWidthPx = 130;
    constexpr int kDefaultTokenIconSizePx = 14;
    constexpr int kSmallTokenIconSizePx = 12;
    constexpr int kInlineSpacingPx = 4;
    constexpr int kWarningIconSizePx = 20;
    constexpr int kWarningBlockSpacingPx = 10;
    constexpr int kWarningMarginHorizontalPx = 14;
    constexpr int kWarningMarginVerticalPx = 12;
    constexpr int kTextColumnSpacingPx = 6;
    constexpr int kIndentedLeftMarginPx = 10;
    constexpr int kNestedLeftMarginPx = 6;
    constexpr int kFromBlockSpacingPx = 5;
    constexpr int kNonceValueRowSpacingPx = 1;
    constexpr int kRecipientRowSpacingPx = 8;
    constexpr int kRecipientValueVerticalMarginPx = 12;
    constexpr int kRecipientRowVerticalMarginPx = 8;
    constexpr int kRecipientNameSpacingPx = 2;
    constexpr int kSummaryMarginHorizontalPx = 20;
    constexpr int kSummaryMarginVerticalPx = 14;
    constexpr int kSummaryInlineSpacingPx = 2;
    constexpr int kDetailRowVerticalMarginPx = 10;
    constexpr int kDetailLabelWidthPx = 160;
    constexpr int kHairlineHeightPx = 1;
    constexpr int kOverviewMarginHorizontalPx = 20;
    constexpr int kOverviewMarginVerticalPx = 4;
    constexpr int kDetailBlockSpacingPx = 12;
    constexpr int kDetailCardMarginHorizontalPx = 16;
    constexpr int kDetailCardMarginVerticalPx = 12;
    constexpr int kDetailCardSpacingPx = 6;
    constexpr int kInstructionIndentPx = 16;
    constexpr int kSendConfirmButtonHeightPx = 48;
    constexpr double kNetworkFeePerTxSol = 0.000005;

    class TreeConnector : public QWidget {
      public:
        TreeConnector(bool isLast, QWidget* parent = nullptr,
                      const QColor& color = QColor(100, 100, 150, 90))
            : QWidget(parent), m_isLast(isLast), m_color(color) {
            setFixedWidth(kTreeConnectorWidthPx);
        }

      protected:
        void paintEvent(QPaintEvent*) override {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            QPen pen(m_color, kTreeConnectorPenWidth);
            p.setPen(pen);

            int lineX = kTreeConnectorLineXPx;
            int midY = height() / 2;

            p.drawLine(lineX, 0, lineX, m_isLast ? midY : height());
            p.drawLine(lineX, midY, width() - kTreeConnectorTailInsetPx, midY);
        }

      private:
        bool m_isLast;
        QColor m_color;
    };
} // namespace

QWidget* SendReceivePage::buildReviewPage() {
    m_reviewScroll = new QScrollArea();
    m_reviewScroll->setWidgetResizable(true);
    m_reviewScroll->setFrameShape(QFrame::NoFrame);
    m_reviewScroll->viewport()->setProperty("uiClass", "contentViewport");
    return m_reviewScroll;
}

void SendReceivePage::populateReviewPage() {
    // Clear previous content
    if (m_reviewScroll->widget()) {
        delete m_reviewScroll->takeWidget();
    }

    QWidget* content = new QWidget();
    content->setObjectName("sendReceiveContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kPageMarginHorizontalPx, kPageMarginTopPx, kPageMarginHorizontalPx,
                               kPageMarginBottomPx);
    layout->setSpacing(kPageSpacingPx);

    // Header row: Back (left) + Export PDF (right)
    QWidget* headerRow = new QWidget();
    headerRow->setObjectName("reviewHeaderRow");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this,
            [this]() { setCurrentPage(StackPage::SendForm); });
    headerLayout->addWidget(backBtn);

    headerLayout->addStretch();

    Dropdown* exportDropdown = new Dropdown();
    exportDropdown->setCurrentItem(tr("Export"));
    exportDropdown->addItem(QIcon(":/icons/ui/export-pdf.svg"), tr("PDF"));
    exportDropdown->addItem(QIcon(":/icons/ui/export-csv.svg"), tr("CSV"));
    exportDropdown->setFixedWidth(kExportDropdownWidthPx);
    connect(exportDropdown, &Dropdown::itemSelected, this, [this](const QString& text) {
        if (text == tr("PDF")) {
            exportReviewPdf();
        } else if (text == tr("CSV")) {
            exportReviewCsv();
        }
    });
    headerLayout->addWidget(exportDropdown);

    layout->addWidget(headerRow);

    // Title
    QLabel* title = new QLabel(tr("Review Send"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    layout->addSpacing(kSmallSpacingPx);

    QList<SendReceiveRecipientInput> recipientInputs;
    recipientInputs.reserve(m_recipientRows.size());
    for (const auto& row : m_recipientRows) {
        recipientInputs.append({row.addressInput->address(), row.amountInput->text()});
    }
    const SendReceiveReviewData reviewData = m_handler->buildReviewData(
        {m_tokenDropdown->currentText(), m_tokenDropdown->currentIconPath(), m_tokenMeta,
         recipientInputs});
    const QString tokenSymbol = reviewData.tokenSymbol;
    const QList<SendReceiveRecipientReview>& recipients = reviewData.recipients;
    const double totalAmount = reviewData.totalAmount;
    const int totalRecipients = reviewData.totalRecipients;
    const int numBatches = reviewData.numBatches;

    // Token icon path for inline icon usage
    const QString tokenIcon = reviewData.tokenIcon;

    // Helper: create a small inline token icon QLabel (14x14)
    auto makeTokenIcon = [&](int sz = kDefaultTokenIconSizePx) -> QLabel* {
        QLabel* iconLbl = new QLabel();
        QPixmap px(tokenIcon);
        if (!px.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            iconLbl->setPixmap(AvatarCache::roundedRectClip(px, sz, dpr));
        }
        iconLbl->setFixedSize(sz, sz);
        iconLbl->setObjectName("srTransparentRow");
        return iconLbl;
    };

    // Helper: create "amount [icon] SYMBOL" widget
    auto makeAmountWithIcon = [&](const QString& amount, const QString& symbol) -> QWidget* {
        QWidget* w = new QWidget();
        w->setObjectName("srTransparentRow");
        QHBoxLayout* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(kInlineSpacingPx);
        QLabel* amtLbl = new QLabel(amount);
        amtLbl->setObjectName("srRecipientAmount14");
        h->addWidget(amtLbl);
        h->addWidget(makeTokenIcon());
        QLabel* symLbl = new QLabel(symbol);
        symLbl->setObjectName("srRecipientAmount14");
        h->addWidget(symLbl);
        return w;
    };

    // Token metadata (needed for transfer fee check + detail table)
    const SendReceiveTokenMeta meta = reviewData.meta;
    const bool isSol = reviewData.isSol;

    // Reset transfer fee state
    m_transferFeeBasisPoints = 0;
    m_transferFeeMax = 0;

    // ── Transfer fee warning (Token-2022) — placed above From ────
    QWidget* transferFeeWarning = nullptr;
    if (!isSol && SolanaPrograms::isToken2022(meta.tokenProgram) && m_solanaApi) {
        transferFeeWarning = new QWidget();
        transferFeeWarning->setObjectName("srTransferFeeWarning");

        QHBoxLayout* warnH = new QHBoxLayout(transferFeeWarning);
        warnH->setContentsMargins(kWarningMarginHorizontalPx, kWarningMarginVerticalPx,
                                  kWarningMarginHorizontalPx, kWarningMarginVerticalPx);
        warnH->setSpacing(kWarningBlockSpacingPx);

        // Caution icon
        QLabel* cautionIcon = new QLabel();
        QPixmap cautionPx(":/icons/ui/caution.svg");
        if (!cautionPx.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            QPixmap scaled = cautionPx.scaled(static_cast<int>(kWarningIconSizePx * dpr),
                                              static_cast<int>(kWarningIconSizePx * dpr),
                                              Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(dpr);
            cautionIcon->setPixmap(scaled);
        }
        cautionIcon->setFixedSize(kWarningIconSizePx, kWarningIconSizePx);
        cautionIcon->setObjectName("srTransparentRow");
        warnH->addWidget(cautionIcon, 0, Qt::AlignTop);

        // Right side: title + body stacked
        QWidget* textCol = new QWidget();
        textCol->setObjectName("srTransparentRow");
        QVBoxLayout* textLayout = new QVBoxLayout(textCol);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(kTextColumnSpacingPx);

        // Title — gets updated async with the percentage
        QLabel* titleLabel = new QLabel(tr("Checking for transfer fees..."));
        titleLabel->setObjectName("srWarningTitle14");
        textLayout->addWidget(titleLabel);

        // Body — hidden until data arrives
        QLabel* bodyLabel = new QLabel();
        bodyLabel->setWordWrap(true);
        bodyLabel->setObjectName("srWarningBody12");
        bodyLabel->setVisible(false);
        textLayout->addWidget(bodyLabel);

        warnH->addWidget(textCol, 1);

        layout->addWidget(transferFeeWarning);
        layout->addSpacing(kTinySpacingPx);

        SendReceiveHandler::FetchTransferFeeCallbacks callbacks;
        callbacks.onReady = [this, transferFeeWarning, titleLabel, bodyLabel,
                             tokenSymbol](const SendReceiveTransferFeeInfo& feeInfo) {
            if (!feeInfo.found) {
                transferFeeWarning->setVisible(false);
                return;
            }

            m_transferFeeBasisPoints = feeInfo.basisPoints;
            m_transferFeeMax = feeInfo.maxRaw;

            const double pct = feeInfo.basisPoints / 100.0;
            titleLabel->setText(
                tr("Transfer Fee: %1% on every transfer").arg(QString::number(pct, 'f', 2)));
            bodyLabel->setText(tr("~%1 %2 will be deducted as a transfer fee. "
                                  "Recipients receive the amount minus "
                                  "the fee.\nMax fee per transfer: %3 %2")
                                   .arg(m_handler->formatCryptoAmount(feeInfo.estimatedFee))
                                   .arg(tokenSymbol)
                                   .arg(m_handler->formatCryptoAmount(feeInfo.maxHuman)));
            bodyLabel->setVisible(true);
        };
        callbacks.onFailed = [titleLabel](const QString& errorText) {
            titleLabel->setText(errorText);
        };
        m_handler->fetchTransferFeeInfo(meta, totalAmount, m_solanaApi, this, callbacks);
    }

    // From section — sender address with balance
    {
        QWidget* fromBlock = new QWidget();
        fromBlock->setObjectName("srTransparentRow");
        QVBoxLayout* fromCol = new QVBoxLayout(fromBlock);
        fromCol->setContentsMargins(kIndentedLeftMarginPx, 0, 0, 0);
        fromCol->setSpacing(kFromBlockSpacingPx);

        QLabel* fromLabel = new QLabel(tr("From"));
        fromLabel->setObjectName("srSectionTitle14");
        fromCol->addWidget(fromLabel);

        QWidget* fromIndent = new QWidget();
        fromIndent->setObjectName("srTransparentRow");
        QVBoxLayout* indentCol = new QVBoxLayout(fromIndent);
        indentCol->setContentsMargins(kNestedLeftMarginPx, 0, 0, 0);
        indentCol->setSpacing(kNonceValueRowSpacingPx);

        AddressLink* fromAddr = new AddressLink(m_walletAddress);
        indentCol->addWidget(fromAddr);

        QString balanceText = m_tokenDropdown->currentBalanceText();
        QWidget* balRow = new QWidget();
        balRow->setObjectName("srTransparentRow");
        QHBoxLayout* balH = new QHBoxLayout(balRow);
        balH->setContentsMargins(0, 0, 0, 0);
        balH->setSpacing(kInlineSpacingPx);
        QLabel* balPre = new QLabel(tr("Balance: %1").arg(balanceText));
        balPre->setObjectName("srMutedLabel12");
        balH->addWidget(balPre);
        balH->addWidget(makeTokenIcon(kSmallTokenIconSizePx));
        QLabel* balSym = new QLabel(tokenSymbol);
        balSym->setObjectName("srMutedLabel12");
        balH->addWidget(balSym);
        balH->addStretch();
        indentCol->addWidget(balRow);

        fromCol->addWidget(fromIndent);
        layout->addWidget(fromBlock);
        layout->addSpacing(kTinySpacingPx);
    }

    // Build recipient display — tree hierarchy for all cases
    {
        for (int batch = 0; batch < numBatches; ++batch) {
            int startIdx = batch * SendReceiveHandler::RECIPIENTS_PER_TX;
            int endIdx = qMin(startIdx + SendReceiveHandler::RECIPIENTS_PER_TX, totalRecipients);
            int batchSize = endIdx - startIdx;

            // Batch header (tree root)
            QString batchText;
            if (numBatches > 1) {
                batchText = tr("Transaction %1 of %2 (%3 recipients)")
                                .arg(batch + 1)
                                .arg(numBatches)
                                .arg(batchSize);
            } else {
                batchText =
                    (batchSize == 1) ? tr("1 recipient") : tr("%1 recipients").arg(batchSize);
            }

            QLabel* batchLabel = new QLabel(batchText);
            batchLabel->setObjectName("srSectionTitle14");
            batchLabel->setContentsMargins(kIndentedLeftMarginPx, 0, 0, 0);
            layout->addWidget(batchLabel);

            // Tree container — zero spacing so connector lines stay continuous
            QWidget* treeContainer = new QWidget();
            treeContainer->setObjectName("srTransparentRow");
            QVBoxLayout* treeLayout = new QVBoxLayout(treeContainer);
            treeLayout->setContentsMargins(kNestedLeftMarginPx, 0, 0, 0);
            treeLayout->setSpacing(0);

            for (int i = startIdx; i < endIdx; ++i) {
                const auto& r = recipients[i];
                bool isLast = (i == endIdx - 1);

                QWidget* treeRow = new QWidget();
                treeRow->setObjectName("srTransparentRow");
                QHBoxLayout* treeRowLayout = new QHBoxLayout(treeRow);
                treeRowLayout->setContentsMargins(0, 0, 0, 0);
                treeRowLayout->setSpacing(kRecipientRowSpacingPx);

                // Tree connector (painted vertical + horizontal lines)
                TreeConnector* connector = new TreeConnector(isLast);
                treeRowLayout->addWidget(connector);

                // Recipient info
                QWidget* recipientInfo = new QWidget();
                recipientInfo->setObjectName("srTransparentRow");
                QVBoxLayout* recipientCol = new QVBoxLayout(recipientInfo);
                recipientCol->setContentsMargins(0, kRecipientRowVerticalMarginPx, 0,
                                                 kRecipientRowVerticalMarginPx);
                recipientCol->setSpacing(kRecipientNameSpacingPx);

                if (!r.name.isEmpty()) {
                    QLabel* nameLabel = new QLabel(r.name);
                    nameLabel->setObjectName("srRecipientName14");
                    recipientCol->addWidget(nameLabel);
                }

                AddressLink* addrLink = new AddressLink(r.address);
                recipientCol->addWidget(addrLink);

                treeRowLayout->addWidget(recipientInfo);

                treeRowLayout->addStretch();

                QWidget* amtWidget = makeAmountWithIcon(r.amountText, tokenSymbol);
                amtWidget->setContentsMargins(0, kRecipientValueVerticalMarginPx, 0,
                                              kRecipientValueVerticalMarginPx);
                treeRowLayout->addWidget(amtWidget);

                treeLayout->addWidget(treeRow);
            }

            layout->addWidget(treeContainer);
        }
    }

    layout->addSpacing(kSmallSpacingPx);

    // Summary bar
    QWidget* summaryBar = new QWidget();
    summaryBar->setObjectName("srReviewSummary");
    QHBoxLayout* summaryLayout = new QHBoxLayout(summaryBar);
    summaryLayout->setContentsMargins(kSummaryMarginHorizontalPx, kSummaryMarginVerticalPx,
                                      kSummaryMarginHorizontalPx, kSummaryMarginVerticalPx);

    QLabel* totalPre = new QLabel(tr("Total: %1 recipients  \xc2\xb7  %2")
                                      .arg(totalRecipients)
                                      .arg(m_handler->formatCryptoAmount(totalAmount)));
    totalPre->setObjectName("srSummaryStrong14");
    summaryLayout->addWidget(totalPre);
    summaryLayout->addSpacing(kSummaryInlineSpacingPx);
    summaryLayout->addWidget(makeTokenIcon());
    QLabel* totalSym = new QLabel(tokenSymbol);
    totalSym->setObjectName("srSummaryStrong14");
    summaryLayout->addWidget(totalSym);

    layout->addWidget(summaryBar);

    // ── Transaction Details — key/value overview table ──────────
    layout->addSpacing(kSmallSpacingPx);

    QLabel* detailTitle = new QLabel(tr("Transaction Details"));
    detailTitle->setObjectName("srSectionTitle14");
    layout->addWidget(detailTitle);

    int detailRowCount = 0;
    auto addDetailRow = [&](QVBoxLayout* target, const QString& label, QWidget* valueWidget) {
        // Separator before each row except the first (avoids trailing line on last row)
        if (detailRowCount++ > 0) {
            QFrame* sep = new QFrame();
            sep->setFrameShape(QFrame::HLine);
            sep->setObjectName("srKvSep");
            sep->setFixedHeight(kHairlineHeightPx);
            target->addWidget(sep);
        }

        QWidget* row = new QWidget();
        row->setObjectName("srTransparentRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0, kDetailRowVerticalMarginPx, 0, kDetailRowVerticalMarginPx);
        QLabel* lbl = new QLabel(label);
        lbl->setObjectName("srKvLabel13");
        lbl->setFixedWidth(kDetailLabelWidthPx);
        h->addWidget(lbl);
        h->addWidget(valueWidget, 1);
        target->addWidget(row);
    };

    auto makeValueLabel = [&](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("srKvValue13");
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return lbl;
    };

    QWidget* overviewBox = new QWidget();
    overviewBox->setObjectName("srTxOverviewBox");
    QVBoxLayout* overviewLayout = new QVBoxLayout(overviewBox);
    overviewLayout->setContentsMargins(kOverviewMarginHorizontalPx, kOverviewMarginVerticalPx,
                                       kOverviewMarginHorizontalPx, kOverviewMarginVerticalPx);
    overviewLayout->setSpacing(0);

    // Fee Payer (signer)
    AddressLink* payerAddr = new AddressLink(m_walletAddress);
    ContactResolver::resolveAddressLink(payerAddr);
    addDetailRow(overviewLayout, tr("Fee Payer"), payerAddr);

    // Token
    if (!isSol) {
        QWidget* tokenRow = new QWidget();
        tokenRow->setObjectName("srTransparentRow");
        QHBoxLayout* tokenH = new QHBoxLayout(tokenRow);
        tokenH->setContentsMargins(0, 0, 0, 0);
        tokenH->setSpacing(kRecipientRowSpacingPx);
        QLabel* symbolLabel = new QLabel(tokenSymbol);
        symbolLabel->setObjectName("srKvValue13");
        AddressLink* mintAddr = new AddressLink(meta.mint);
        ContactResolver::resolveAddressLink(mintAddr);
        tokenH->addWidget(symbolLabel);
        tokenH->addWidget(mintAddr, 1);
        addDetailRow(overviewLayout, tr("Token Mint"), tokenRow);

        addDetailRow(overviewLayout, tr("Token Program"),
                     makeValueLabel(SolanaPrograms::isToken2022(meta.tokenProgram)
                                        ? tr("Token-2022")
                                        : tr("Token Program")));
    }

    // Network Fee (always SOL)
    double feePerTx = kNetworkFeePerTxSol;
    double totalNetworkFee = feePerTx * numBatches;
    {
        QString feeAmount = m_handler->formatCryptoAmount(totalNetworkFee);
        QString feeSuffix;
        if (numBatches > 1) {
            feeSuffix = tr("  (%1 transactions)").arg(numBatches);
        }
        QWidget* feeRow = new QWidget();
        feeRow->setObjectName("srTransparentRow");
        QHBoxLayout* feeH = new QHBoxLayout(feeRow);
        feeH->setContentsMargins(0, 0, 0, 0);
        feeH->setSpacing(kInlineSpacingPx);
        QLabel* feeAmtLbl = new QLabel(feeAmount);
        feeAmtLbl->setObjectName("srKvValue13");
        feeH->addWidget(feeAmtLbl);
        // SOL icon for fee (always SOL regardless of token being sent)
        QLabel* solFeeIcon = new QLabel();
        QPixmap solPx(":/icons/tokens/sol.png");
        if (!solPx.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            solFeeIcon->setPixmap(
                AvatarCache::roundedRectClip(solPx, kDefaultTokenIconSizePx, dpr));
        }
        solFeeIcon->setFixedSize(kDefaultTokenIconSizePx, kDefaultTokenIconSizePx);
        solFeeIcon->setObjectName("srTransparentRow");
        feeH->addWidget(solFeeIcon);
        QLabel* feeSolLbl = new QLabel("SOL" + feeSuffix);
        feeSolLbl->setObjectName("srKvValue13");
        feeH->addWidget(feeSolLbl);
        feeH->addStretch();
        addDetailRow(overviewLayout, tr("Network Fee"), feeRow);
    }

    // Priority Fee
    {
        const int speedIndex = m_speedSelector ? m_speedSelector->activeIndex() : 0;
        QString feeText = (speedIndex == 0) ? tr("Auto") : m_customFeeInput->text() + " SOL";
        addDetailRow(overviewLayout, tr("Priority Fee"), makeValueLabel(feeText));
    }

    // Transaction Version
    addDetailRow(overviewLayout, tr("Transaction Version"), makeValueLabel(tr("legacy")));

    // Durable Nonce (when enabled)
    if (m_nonceEnabled && !m_nonceAddress.isEmpty()) {
        AddressLink* nonceAddr = new AddressLink(m_nonceAddress);
        ContactResolver::resolveAddressLink(nonceAddr);
        addDetailRow(overviewLayout, tr("Durable Nonce"), nonceAddr);

        QLabel* nonceValLabel = new QLabel(m_nonceValue);
        nonceValLabel->setObjectName("srNonceValue13");
        nonceValLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        nonceValLabel->setWordWrap(true);
        addDetailRow(overviewLayout, tr("Nonce Value"), nonceValLabel);
    }

    layout->addWidget(overviewBox);

    // ── Instruction breakdown ──────────────────────────────────
    layout->addSpacing(kDetailBlockSpacingPx);

    QLabel* ixTitle = new QLabel(tr("Instructions"));
    ixTitle->setObjectName("srSectionTitle14");
    layout->addWidget(ixTitle);

    QWidget* detailBox = new QWidget();
    detailBox->setObjectName("srTxDetailBox");
    QVBoxLayout* detailLayout = new QVBoxLayout(detailBox);
    detailLayout->setContentsMargins(kDetailCardMarginHorizontalPx, kDetailCardMarginVerticalPx,
                                     kDetailCardMarginHorizontalPx, kDetailCardMarginVerticalPx);
    detailLayout->setSpacing(kDetailCardSpacingPx);

    int ixNum = 0;

    // Nonce advance instruction (if enabled)
    if (m_nonceEnabled && !m_nonceAddress.isEmpty()) {
        ++ixNum;
        QLabel* nonceIxLabel = new QLabel(tr("%1. Advance Nonce").arg(ixNum));
        nonceIxLabel->setObjectName("srInstructionLabel13");
        detailLayout->addWidget(nonceIxLabel);

        AddressLink* nonceIxAddr = new AddressLink(m_nonceAddress);
        ContactResolver::resolveAddressLink(nonceIxAddr);
        nonceIxAddr->setContentsMargins(kInstructionIndentPx, 0, 0, 0);
        detailLayout->addWidget(nonceIxAddr);
    }

    // Per-recipient instructions
    for (const auto& r : recipients) {
        if (isSol) {
            ++ixNum;
            QLabel* ixLabel = new QLabel(tr("%1. System Program: Transfer %2 SOL")
                                             .arg(ixNum)
                                             .arg(m_handler->formatCryptoAmount(r.amount)));
            ixLabel->setObjectName("srInstructionLabel13");
            detailLayout->addWidget(ixLabel);

            AddressLink* destAddr = new AddressLink(r.address);
            ContactResolver::resolveAddressLink(destAddr);
            destAddr->setContentsMargins(kInstructionIndentPx, 0, 0, 0);
            detailLayout->addWidget(destAddr);
        } else {
            // Create ATA (idempotent)
            ++ixNum;
            QLabel* ataLabel = new QLabel(tr("%1. Create Associated Token Account").arg(ixNum));
            ataLabel->setObjectName("srInstructionMuted13");
            detailLayout->addWidget(ataLabel);

            AddressLink* ataAddr = new AddressLink(r.address);
            ContactResolver::resolveAddressLink(ataAddr);
            ataAddr->setContentsMargins(kInstructionIndentPx, 0, 0, 0);
            detailLayout->addWidget(ataAddr);

            // TransferChecked
            ++ixNum;
            QString programLabel = SolanaPrograms::isToken2022(meta.tokenProgram)
                                       ? tr("Token-2022")
                                       : tr("Token Program");
            QLabel* txLabel = new QLabel(tr("%1. %2: TransferChecked %3 %4")
                                             .arg(ixNum)
                                             .arg(programLabel)
                                             .arg(m_handler->formatCryptoAmount(r.amount))
                                             .arg(tokenSymbol));
            txLabel->setObjectName("srInstructionLabel13");
            detailLayout->addWidget(txLabel);

            AddressLink* txAddr = new AddressLink(r.address);
            ContactResolver::resolveAddressLink(txAddr);
            txAddr->setContentsMargins(kInstructionIndentPx, 0, 0, 0);
            detailLayout->addWidget(txAddr);
        }
    }

    // Total instruction count
    QLabel* totalIx = new QLabel(tr("%1 instruction(s) total").arg(ixNum));
    totalIx->setObjectName("srInstructionTotal12");
    detailLayout->addWidget(totalIx);

    layout->addWidget(detailBox);

    layout->addSpacing(kTinySpacingPx);

    // Confirm & Send button
    m_sendConfirmBtn = new QPushButton(tr("Confirm & Send"));
    m_sendConfirmBtn->setCursor(Qt::PointingHandCursor);
    m_sendConfirmBtn->setMinimumHeight(kSendConfirmButtonHeightPx);
    m_sendConfirmBtn->setStyleSheet(Theme::primaryBtnStyle);
    connect(m_sendConfirmBtn, &QPushButton::clicked, this, &SendReceivePage::executeSend);
    layout->addWidget(m_sendConfirmBtn);

    // Status label (for send progress / success / error)
    m_sendStatusLabel = new QLabel();
    m_sendStatusLabel->setWordWrap(true);
    m_sendStatusLabel->setVisible(false);
    m_sendStatusLabel->setObjectName("srStatusNeutral13");
    initializeStatusLabel(m_sendStatusLabel);
    layout->addWidget(m_sendStatusLabel);

    layout->addStretch();

    m_reviewScroll->setWidget(content);
}

// ── Event Filter ────────────────────────────────────────────────
