#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "crypto/Keypair.h"
#include "features/sendreceive/CreateTokenHandler.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "widgets/StyledCheckbox.h"
#include "widgets/UploadWidget.h"

#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTextEdit>
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

    constexpr int kDetailRowVerticalPaddingPx = 8;
    constexpr int kDetailsCardMarginHorizontalPx = 16;
    constexpr int kDetailsCardMarginVerticalPx = 14;
    constexpr int kDetailsTitleSpacingPx = 4;
    constexpr int kImageRowVerticalPaddingPx = 6;
    constexpr int kImageThumbSizePx = 40;
    constexpr int kDescriptionPreviewMaxChars = 100;

    constexpr int kExtensionsRowSpacingPx = 6;
    constexpr int kExtensionsCardMarginHorizontalPx = 16;
    constexpr int kExtensionsCardMarginVerticalPx = 10;
    constexpr int kInitialMintCardMarginHorizontalPx = 16;
    constexpr int kInitialMintCardMarginVerticalPx = 12;

    constexpr int kCostCardMarginHorizontalPx = 16;
    constexpr int kCostCardMarginVerticalPx = 14;
    constexpr int kCostCardSpacingPx = 8;
    constexpr double kLamportsPerSol = 1'000'000'000.0;
    constexpr double kCreateTokenNetworkFeeSol = 0.000005;
    constexpr int kCreateTokenConfirmButtonMinHeightPx = 48;

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

            const int lineX = kTreeConnectorLineXPx;
            const int midY = height() / 2;
            p.drawLine(lineX, 0, lineX, m_isLast ? midY : height());
            p.drawLine(lineX, midY, width() - kTreeConnectorTailInsetPx, midY);
        }

      private:
        bool m_isLast = false;
        QColor m_color;
    };
} // namespace

void SendReceivePage::populateCreateTokenReview() {
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

    // Back button → return to create token form
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        m_isCreateTokenReview = false;
        setCurrentPage(StackPage::CreateToken);
    });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Review: Create Token"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    layout->addSpacing(kSmallSpacingPx);

    const SendReceiveCreateTokenFormState formState =
        m_createTokenHandler->parseCreateTokenFormState(collectCreateTokenBuildInput());
    const QString tokenName = formState.name;
    const QString tokenSymbol = formState.symbol;
    const QString tokenUri = formState.uri;
    const int decimals = formState.decimals;
    const QString mintAddress = m_mintKeypair.address();
    const bool hasFreezeAuth = formState.freezeAuthorityEnabled;
    QString description = m_ctDescription->toPlainText().trimmed();
    QString imagePath = m_ctImageUpload ? m_ctImageUpload->imagePath() : QString();
    const double initialSupply = formState.initialSupply;
    const bool hasTransferFee = formState.hasTransferFee;
    const int feeBps = formState.feeBasisPoints;
    const double feeMax = formState.feeMax;
    const bool hasNonTransferable = formState.hasNonTransferable;
    const bool hasMintClose = formState.hasMintClose;
    const bool hasPermDelegate = formState.hasPermanentDelegate;

    // ── Helper: detail row (for use inside a container card) ──
    auto makeDetailRow = [](const QString& label, const QString& value,
                            bool mono = false) -> QWidget* {
        QWidget* row = new QWidget();
        row->setObjectName("srTransparentRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0, kDetailRowVerticalPaddingPx, 0, kDetailRowVerticalPaddingPx);

        QLabel* lbl = new QLabel(label);
        lbl->setObjectName("srReviewLabel14");
        h->addWidget(lbl);

        h->addStretch();

        QLabel* val = new QLabel(value);
        val->setObjectName(mono ? "srReviewValueMono14" : "srReviewValueStrong14");
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        h->addWidget(val);

        return row;
    };

    auto makeDetailSep = []() -> QFrame* {
        QFrame* sep = new QFrame();
        sep->setFrameShape(QFrame::HLine);
        sep->setObjectName("srReviewSep");
        return sep;
    };

    // ── Token Details card ───────────────────────────────────
    QWidget* detailsCard = new QWidget();
    detailsCard->setObjectName("srReviewSummary");
    QVBoxLayout* detailsLay = new QVBoxLayout(detailsCard);
    detailsLay->setContentsMargins(kDetailsCardMarginHorizontalPx, kDetailsCardMarginVerticalPx,
                                   kDetailsCardMarginHorizontalPx, kDetailsCardMarginVerticalPx);
    detailsLay->setSpacing(0);

    QLabel* detailsTitle = new QLabel(tr("Token Details"));
    detailsTitle->setObjectName("srCapsTitle12");
    detailsLay->addWidget(detailsTitle);
    detailsLay->addSpacing(kDetailsTitleSpacingPx);

    detailsLay->addWidget(makeDetailRow(tr("Token Name"), tokenName));
    detailsLay->addWidget(makeDetailSep());
    detailsLay->addWidget(makeDetailRow(tr("Symbol"), tokenSymbol));
    detailsLay->addWidget(makeDetailSep());
    detailsLay->addWidget(makeDetailRow(tr("Decimals"), QString::number(decimals)));
    if (!tokenUri.isEmpty()) {
        detailsLay->addWidget(makeDetailSep());
        detailsLay->addWidget(makeDetailRow(tr("Metadata URI"), tokenUri));
    }
    detailsLay->addWidget(makeDetailSep());
    detailsLay->addWidget(makeDetailRow(tr("Mint Address"), mintAddress, true));
    detailsLay->addWidget(makeDetailSep());
    detailsLay->addWidget(
        makeDetailRow(tr("Freeze Authority"), hasFreezeAuth ? tr("Enabled") : tr("Disabled")));

    if (!description.isEmpty()) {
        QString displayDesc = description.length() > kDescriptionPreviewMaxChars
                                  ? description.left(kDescriptionPreviewMaxChars) + "..."
                                  : description;
        detailsLay->addWidget(makeDetailSep());
        detailsLay->addWidget(makeDetailRow(tr("Description"), displayDesc));
    }

    // Image preview row (if selected)
    if (!imagePath.isEmpty()) {
        detailsLay->addWidget(makeDetailSep());

        QWidget* imgRow = new QWidget();
        imgRow->setObjectName("srTransparentRow");
        QHBoxLayout* imgH = new QHBoxLayout(imgRow);
        imgH->setContentsMargins(0, kImageRowVerticalPaddingPx, 0, kImageRowVerticalPaddingPx);

        QLabel* imgLbl = new QLabel(tr("Token Logo"));
        imgLbl->setObjectName("srReviewLabel14");
        imgH->addWidget(imgLbl);

        imgH->addStretch();

        QPixmap px(imagePath);
        if (!px.isNull()) {
            QLabel* thumb = new QLabel();
            thumb->setObjectName("srTransparentRow");
            qreal dpr = devicePixelRatioF();
            QPixmap scaled = px.scaled(static_cast<int>(kImageThumbSizePx * dpr),
                                       static_cast<int>(kImageThumbSizePx * dpr),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(dpr);
            thumb->setPixmap(scaled);
            thumb->setFixedSize(kImageThumbSizePx, kImageThumbSizePx);
            imgH->addWidget(thumb);
            imgH->addSpacing(kSmallSpacingPx);
        }

        QLabel* fname = new QLabel(QFileInfo(imagePath).fileName());
        fname->setObjectName("srReviewValue14");
        imgH->addWidget(fname);

        detailsLay->addWidget(imgRow);
    }

    layout->addWidget(detailsCard);

    // Extensions section
    QStringList extensions = m_createTokenHandler->buildExtensionSummary(
        hasTransferFee, feeBps, feeMax, hasNonTransferable, hasMintClose, hasPermDelegate);

    if (!extensions.isEmpty()) {
        layout->addSpacing(kTinySpacingPx);
        QLabel* extTitle = new QLabel(tr("Extensions"));
        extTitle->setObjectName("srExtTitle15");
        layout->addWidget(extTitle);

        // Container so the vertical tree lines connect across rows
        QWidget* extContainer = new QWidget();
        extContainer->setObjectName("srTransparentRow");
        QVBoxLayout* extVLay = new QVBoxLayout(extContainer);
        extVLay->setContentsMargins(0, 0, 0, 0);
        extVLay->setSpacing(kExtensionsRowSpacingPx);

        QColor treeColor(16, 185, 129, 80);

        for (int i = 0; i < extensions.size(); ++i) {
            bool isLast = (i == extensions.size() - 1);

            QHBoxLayout* rowH = new QHBoxLayout();
            rowH->setContentsMargins(0, 0, 0, 0);
            rowH->setSpacing(kExtensionsRowSpacingPx);

            auto* connector = new TreeConnector(isLast, extContainer, treeColor);
            rowH->addWidget(connector);

            QWidget* extRow = new QWidget();
            extRow->setObjectName("srExtensionRow");
            QHBoxLayout* eh = new QHBoxLayout(extRow);
            eh->setContentsMargins(
                kExtensionsCardMarginHorizontalPx, kExtensionsCardMarginVerticalPx,
                kExtensionsCardMarginHorizontalPx, kExtensionsCardMarginVerticalPx);

            QLabel* checkMark = new QLabel(QString::fromUtf8("\xe2\x9c\x93"));
            checkMark->setObjectName("srExtensionCheck");
            eh->addWidget(checkMark);

            QLabel* extLabel = new QLabel(extensions[i]);
            extLabel->setObjectName("srExtensionLabel");
            eh->addWidget(extLabel, 1);

            rowH->addWidget(extRow, 1);
            extVLay->addLayout(rowH);
        }

        layout->addWidget(extContainer);
    }

    // Initial supply
    if (initialSupply > 0.0) {
        layout->addSpacing(kTinySpacingPx);
        QWidget* mintRow = new QWidget();
        mintRow->setObjectName("srReviewSummary");
        QHBoxLayout* mintH = new QHBoxLayout(mintRow);
        mintH->setContentsMargins(
            kInitialMintCardMarginHorizontalPx, kInitialMintCardMarginVerticalPx,
            kInitialMintCardMarginHorizontalPx, kInitialMintCardMarginVerticalPx);
        QLabel* mintLbl = new QLabel(tr("Initial Mint"));
        mintLbl->setObjectName("srMutedLabel13");
        mintH->addWidget(mintLbl);
        mintH->addStretch();
        QLabel* mintVal =
            new QLabel(m_createTokenHandler->formatCryptoAmount(initialSupply) + " " + tokenSymbol);
        mintVal->setObjectName("srValueStrong13");
        mintH->addWidget(mintVal);
        layout->addWidget(mintRow);
    }

    layout->addSpacing(kSmallSpacingPx);

    // ── Cost summary ─────────────────────────────────────────

    QWidget* costBar = new QWidget();
    costBar->setObjectName("srReviewSummary");
    QVBoxLayout* costLay = new QVBoxLayout(costBar);
    costLay->setContentsMargins(kCostCardMarginHorizontalPx, kCostCardMarginVerticalPx,
                                kCostCardMarginHorizontalPx, kCostCardMarginVerticalPx);
    costLay->setSpacing(kCostCardSpacingPx);

    QLabel* costTitle = new QLabel(tr("Estimated Cost"));
    costTitle->setObjectName("srCapsTitle12");
    costLay->addWidget(costTitle);

    QLabel* rentLabel = new QLabel(tr("Calculating rent..."));
    rentLabel->setObjectName("ctRentLabel");
    rentLabel->setObjectName("srReviewValueStrong14");
    costLay->addWidget(rentLabel);

    // Upload cost line (only when upload is enabled)
    bool wantsUpload = m_ctUploadCheck && m_ctUploadCheck->isChecked() && m_ctUploadLamports > 0;
    QLabel* uploadCostLabel = nullptr;
    if (wantsUpload) {
        double uploadSol = static_cast<double>(m_ctUploadLamports) / kLamportsPerSol;
        uploadCostLabel =
            new QLabel(tr("+ image upload: %1 SOL").arg(QString::number(uploadSol, 'f', 9)));
        uploadCostLabel->setObjectName("srSubtleCost12");
        costLay->addWidget(uploadCostLabel);
    }

    QLabel* feeLabel = new QLabel(
        tr("+ network fee (~%1 SOL)").arg(QString::number(kCreateTokenNetworkFeeSol, 'f', 6)));
    feeLabel->setObjectName("srFeeLabel12");
    costLay->addWidget(feeLabel);

    // Separator + total (populated after rent is fetched)
    QFrame* costSep = new QFrame();
    costSep->setFrameShape(QFrame::HLine);
    costSep->setObjectName("srCostSep");
    costSep->setVisible(false);
    costLay->addWidget(costSep);

    QLabel* totalLabel = new QLabel();
    totalLabel->setObjectName("srTotalStrong15");
    totalLabel->setVisible(false);
    costLay->addWidget(totalLabel);

    // Balance warning (hidden until we can compute total)
    QLabel* balanceWarn = new QLabel();
    balanceWarn->setObjectName("srErrorStrong13");
    balanceWarn->setWordWrap(true);
    balanceWarn->setVisible(false);
    costLay->addWidget(balanceWarn);

    layout->addWidget(costBar);

    const quint64 uploadLamports = wantsUpload ? m_ctUploadLamports : 0;
    CreateTokenHandler::ReviewCostCallbacks costCallbacks;
    costCallbacks.onReady = [this, rentLabel, totalLabel, costSep,
                             balanceWarn](const SendReceiveCreateTokenCostSummary& costSummary) {
        rentLabel->setText(costSummary.rentText);
        totalLabel->setText(costSummary.totalText);
        totalLabel->setVisible(true);
        costSep->setVisible(true);
        if (costSummary.insufficientSol) {
            balanceWarn->setText(costSummary.insufficientSolText);
            balanceWarn->setVisible(true);
            if (m_createTokenConfirmBtn) {
                m_createTokenConfirmBtn->setEnabled(false);
                m_createTokenConfirmBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
            }
        }
    };
    costCallbacks.onFailed = [rentLabel](const QString& errorText) {
        rentLabel->setText(errorText);
    };
    m_createTokenHandler->fetchReviewCostSummary(m_walletAddress, computeMintAccountSize(),
                                                 uploadLamports, m_solanaApi, this, costCallbacks);

    layout->addSpacing(kSmallSpacingPx);

    // ── Confirm button ───────────────────────────────────────

    m_createTokenConfirmBtn = new QPushButton(tr("Confirm & Create Token"));
    m_createTokenConfirmBtn->setCursor(Qt::PointingHandCursor);
    m_createTokenConfirmBtn->setMinimumHeight(kCreateTokenConfirmButtonMinHeightPx);
    m_createTokenConfirmBtn->setStyleSheet(Theme::primaryBtnStyle);
    connect(m_createTokenConfirmBtn, &QPushButton::clicked, this,
            &SendReceivePage::executeCreateToken);
    layout->addWidget(m_createTokenConfirmBtn);

    m_createTokenStatusLabel = new QLabel();
    initializeStatusLabel(m_createTokenStatusLabel);
    m_createTokenStatusLabel->setWordWrap(true);
    m_createTokenStatusLabel->setVisible(false);
    layout->addWidget(m_createTokenStatusLabel);

    layout->addStretch();

    m_reviewScroll->setWidget(content);
}

// ── Create Token: Execute ───────────────────────────────────────

void SendReceivePage::executeCreateToken() {
    m_createTokenConfirmBtn->setEnabled(false);
    m_createTokenConfirmBtn->setText(tr("Creating token..."));
    m_createTokenConfirmBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    m_createTokenStatusLabel->setVisible(false);

    auto resetButton = [this]() {
        m_createTokenConfirmBtn->setEnabled(true);
        m_createTokenConfirmBtn->setText(tr("Confirm & Create Token"));
        m_createTokenConfirmBtn->setStyleSheet(Theme::primaryBtnStyle);
    };
    auto setStatus = [this](const QString& text, bool isError) {
        setStatusLabelState(m_createTokenStatusLabel, text, isError);
    };

    const SendReceiveCreateTokenFormState formState =
        m_createTokenHandler->parseCreateTokenFormState(collectCreateTokenBuildInput());
    const SendReceiveCreateTokenValidationResult validation =
        m_createTokenHandler->validateCreateTokenForm(formState);
    if (!validation.ok) {
        if (validation.errorCode == "missing_required_fields") {
            setStatus(tr("Name and symbol are required."), true);
        } else if (validation.errorCode == "invalid_transfer_fee") {
            setStatus(tr("Transfer fee must be greater than 0 bps."), true);
        } else {
            setStatus(tr("Please fix the form errors and try again."), true);
        }
        resetButton();
        return;
    }
    const SendReceiveCreateTokenRequest request =
        m_createTokenHandler->buildCreateTokenRequest(formState);

    SendReceiveHandler::CreateTokenCallbacks callbacks;
    callbacks.onStatus = [setStatus](const QString& text, bool isError) {
        setStatus(text, isError);
    };
    callbacks.onSuccess = [this, request](const QString& signature) {
        emit transactionSent(signature);

        SendReceiveSuccessPageInfo info;
        info.title = tr("Token Created");
        info.amount = request.name + " (" + request.symbol + ")";
        info.sender = m_walletAddress;
        info.signature = signature;
        info.mintAddress = request.mintAddress;
        showSuccessPage(info);
        startConfirmationPolling(signature);
    };
    callbacks.onFinished = [resetButton]() { resetButton(); };

    m_createTokenHandler->executeCreateTokenFlow(request, m_mintKeypair, m_solanaApi, m_signer,
                                                 this, callbacks);
}
