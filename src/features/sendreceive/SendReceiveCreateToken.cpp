#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "crypto/Keypair.h"
#include "crypto/Signer.h"
#include "features/sendreceive/CreateTokenHandler.h"
#include "services/SolanaApi.h"
#include "tx/ProgramIds.h"
#include "widgets/StyledCheckbox.h"
#include "widgets/UploadWidget.h"

#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

namespace {
    constexpr int kTreeConnectorWidthPx = 28;
    constexpr qreal kTreeConnectorPenWidth = 1.5;
    constexpr int kTreeConnectorLineXPx = 10;
    constexpr int kTreeConnectorTailInsetPx = 4;
    constexpr int kExtCardRadiusPx = 10;
    constexpr qreal kExtCardBorderInset = 0.5;

    constexpr int kPageMarginHorizontalPx = 40;
    constexpr int kPageMarginTopPx = 20;
    constexpr int kPageMarginBottomPx = 30;
    constexpr int kPageSpacingPx = 12;
    constexpr int kSmallSpacingPx = 4;
    constexpr int kMediumSpacingPx = 8;

    constexpr int kSectionMarginHorizontalPx = 18;
    constexpr int kSectionMarginVerticalPx = 16;
    constexpr int kSectionSpacingPx = 10;
    constexpr int kRowSpacingPx = 12;
    constexpr int kFieldHintSpacingPx = 4;
    constexpr int kLogoRowSpacingPx = 14;
    constexpr int kLogoInfoSpacingPx = 6;
    constexpr int kExtCardMarginHorizontalPx = 14;
    constexpr int kExtCardMarginVerticalPx = 12;

    constexpr int kNameMaxChars = 32;
    constexpr int kSymbolMaxChars = 10;
    constexpr int kDescriptionMaxChars = 500;
    constexpr int kDescriptionHeightPx = 90;
    constexpr int kUriMaxChars = 200;

    constexpr int kUploadPreviewSizePx = 80;
    constexpr int kUploadMaxResolutionPx = 512;
    constexpr int kFeeBpsMin = 1;
    constexpr int kFeeBpsMax = 10000;
    constexpr int kFeeBpsDefault = 100;
    constexpr int kDecimalsMin = 0;
    constexpr int kDecimalsMax = 9;
    constexpr int kDecimalsDefault = 9;
    constexpr int kSpinMinHeightPx = 42;
    constexpr int kActionButtonMinHeightPx = 48;
    constexpr double kSupplyMaxValue = 1e18;

    constexpr int kImmediateUiDelayMs = 0;

    static const QString reviewSummaryBarStyle = "QWidget#reviewSummary {"
                                                 "  background: rgba(25, 28, 50, 0.6);"
                                                 "  border: 1px solid rgba(100, 100, 150, 0.15);"
                                                 "  border-radius: 10px;"
                                                 "}"
                                                 "QWidget#reviewSummary QLabel {"
                                                 "  background: transparent;"
                                                 "  border: none;"
                                                 "}";

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
        bool m_isLast = false;
        QColor m_color;
    };
} // namespace

// ── Create Token Page (index 6) ─────────────────────────────────

// Custom-painted card that avoids QSS cascade flicker on toggle
class ExtCardWidget : public QWidget {
  public:
    explicit ExtCardWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setCursor(Qt::PointingHandCursor);
    }
    void setChecked(bool c) {
        if (m_checked == c) {
            return;
        }
        m_checked = c;
        update(); // only repaints this widget, no child cascade
    }

  protected:
    void mousePressEvent(QMouseEvent*) override {
        // Toggle the child StyledCheckbox when clicking anywhere on the card
        if (auto* cb = findChild<QCheckBox*>()) {
            cb->setChecked(!cb->isChecked());
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = QRectF(rect()).adjusted(kExtCardBorderInset, kExtCardBorderInset,
                                           -kExtCardBorderInset, -kExtCardBorderInset);
        if (m_checked) {
            p.setBrush(QColor(16, 185, 129, 20));
            p.setPen(QPen(QColor(16, 185, 129, 77), 1));
        } else {
            p.setBrush(QColor(22, 24, 42, 128));
            p.setPen(QPen(QColor(100, 100, 150, 38), 1));
        }
        p.drawRoundedRect(r, kExtCardRadiusPx, kExtCardRadiusPx);
    }

  private:
    bool m_checked = false;
};

QWidget* SendReceivePage::buildCreateTokenPage() {
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
    connect(backBtn, &QPushButton::clicked, this,
            [this]() { setCurrentPage(StackPage::CardGrid); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Create Token"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Subtitle
    QLabel* desc = new QLabel(tr("Create a new Token-2022 token with on-chain metadata."));
    desc->setObjectName("srSubtleDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(kSmallSpacingPx);
    layout->addWidget(buildCreateTokenIdentityCard(scroll));

    layout->addWidget(buildCreateTokenSupplyCard());
    layout->addWidget(buildCreateTokenAuthoritiesCard());
    layout->addWidget(buildCreateTokenExtensionsCard(scroll));
    layout->addSpacing(kMediumSpacingPx);
    layout->addWidget(buildCreateTokenReviewButton());

    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

QWidget* SendReceivePage::buildCreateTokenIdentityCard(QScrollArea* scroll) {
    auto makeLabel = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("txFormLabel");
        return lbl;
    };
    auto makeHint = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("ctHint11");
        lbl->setWordWrap(true);
        return lbl;
    };

    QWidget* identityCard = new QWidget();
    identityCard->setObjectName("ctSection");
    QVBoxLayout* idLay = new QVBoxLayout(identityCard);
    idLay->setContentsMargins(kSectionMarginHorizontalPx, kSectionMarginVerticalPx,
                              kSectionMarginHorizontalPx, kSectionMarginVerticalPx);
    idLay->setSpacing(kSectionSpacingPx);

    QLabel* idTitle = new QLabel(tr("Token Identity"));
    idTitle->setObjectName("ctSectionTitle16");
    idLay->addWidget(idTitle);

    QHBoxLayout* nameSymRow = new QHBoxLayout();
    nameSymRow->setSpacing(kRowSpacingPx);

    QVBoxLayout* nameCol = new QVBoxLayout();
    nameCol->setSpacing(kFieldHintSpacingPx);
    nameCol->addWidget(makeLabel(tr("Token Name")));
    m_ctName = new QLineEdit();
    m_ctName->setPlaceholderText(tr("e.g. My Token"));
    m_ctName->setMaxLength(kNameMaxChars);
    m_ctName->setObjectName("ctInput");
    nameCol->addWidget(m_ctName);
    connect(m_ctName, &QLineEdit::textChanged, this,
            &SendReceivePage::updateCreateTokenReviewButtonState);
    nameCol->addWidget(makeHint(tr("Max 32 characters")));
    nameSymRow->addLayout(nameCol, 2);

    QVBoxLayout* symCol = new QVBoxLayout();
    symCol->setSpacing(kFieldHintSpacingPx);
    symCol->addWidget(makeLabel(tr("Symbol")));
    m_ctSymbol = new QLineEdit();
    m_ctSymbol->setPlaceholderText(tr("e.g. MTK"));
    m_ctSymbol->setMaxLength(kSymbolMaxChars);
    m_ctSymbol->setObjectName("ctInput");
    symCol->addWidget(m_ctSymbol);
    connect(m_ctSymbol, &QLineEdit::textChanged, this,
            &SendReceivePage::updateCreateTokenReviewButtonState);
    symCol->addWidget(makeHint(tr("Max 10 characters")));
    nameSymRow->addLayout(symCol, 1);
    idLay->addLayout(nameSymRow);

    idLay->addWidget(makeLabel(tr("Description (optional)")));
    m_ctDescription = new QTextEdit();
    m_ctDescription->setPlaceholderText(tr("Describe your token's purpose and utility..."));
    m_ctDescription->setFixedHeight(kDescriptionHeightPx);
    m_ctDescription->setObjectName("ctDescriptionInput");
    idLay->addWidget(m_ctDescription);

    m_ctDescCounter = new QLabel(QString("0 / %1").arg(kDescriptionMaxChars));
    m_ctDescCounter->setObjectName("ctHint11");
    m_ctDescCounter->setAlignment(Qt::AlignRight);
    idLay->addWidget(m_ctDescCounter);

    connect(m_ctDescription, &QTextEdit::textChanged, this, [this]() {
        QString text = m_ctDescription->toPlainText();
        if (text.length() > kDescriptionMaxChars) {
            m_ctDescription->blockSignals(true);
            QTextCursor cursor = m_ctDescription->textCursor();
            int pos = cursor.position();
            m_ctDescription->setPlainText(text.left(kDescriptionMaxChars));
            cursor.setPosition(qMin(pos, kDescriptionMaxChars));
            m_ctDescription->setTextCursor(cursor);
            m_ctDescription->blockSignals(false);
            text = m_ctDescription->toPlainText();
        }
        m_ctDescCounter->setText(QString("%1 / %2").arg(text.length()).arg(kDescriptionMaxChars));
    });

    m_ctUploadCheck = new StyledCheckbox(tr("Include token image"));
    m_ctUploadCheck->setProperty("uiClass", "ctUploadCheck");
    idLay->addWidget(m_ctUploadCheck);
    idLay->addWidget(
        makeHint(tr("Upload to permanent storage via Arweave. A small SOL fee applies.")));

    QWidget* imageSection = new QWidget();
    imageSection->setVisible(false);
    QVBoxLayout* imageSectionLay = new QVBoxLayout(imageSection);
    imageSectionLay->setContentsMargins(0, kMediumSpacingPx, 0, 0);
    imageSectionLay->setSpacing(kMediumSpacingPx);

    QHBoxLayout* logoRow = new QHBoxLayout();
    logoRow->setSpacing(kLogoRowSpacingPx);

    m_ctImageUpload = new UploadWidget(UploadWidget::RoundedRect, kUploadPreviewSizePx, this);
    m_ctImageUpload->setMaxResolution(kUploadMaxResolutionPx);
    m_ctImageUpload->setPlaceholderText("+");
    logoRow->addWidget(m_ctImageUpload, 0, Qt::AlignTop);

    QVBoxLayout* logoInfoCol = new QVBoxLayout();
    logoInfoCol->setSpacing(kLogoInfoSpacingPx);
    logoInfoCol->addWidget(makeHint(tr("Click to select image. 512x512 recommended. PNG or JPG.")));
    logoInfoCol->addStretch();
    logoRow->addLayout(logoInfoCol, 1);
    imageSectionLay->addLayout(logoRow);

    m_ctUploadInfo = new QWidget();
    m_ctUploadInfo->setVisible(false);
    QVBoxLayout* uploadInfoLay = new QVBoxLayout(m_ctUploadInfo);
    uploadInfoLay->setContentsMargins(0, kSmallSpacingPx, 0, 0);
    uploadInfoLay->setSpacing(kSmallSpacingPx);

    m_ctUploadCost = new QLabel();
    m_ctUploadCost->setTextFormat(Qt::RichText);
    m_ctUploadCost->setObjectName("ctSubtleInfo13");
    uploadInfoLay->addWidget(m_ctUploadCost);

    m_ctUploadWarn = new QLabel();
    m_ctUploadWarn->setProperty("uiClass", "ctUploadWarn");
    m_ctUploadWarn->setVisible(false);
    m_ctUploadWarn->setWordWrap(true);
    uploadInfoLay->addWidget(m_ctUploadWarn);
    imageSectionLay->addWidget(m_ctUploadInfo);
    idLay->addWidget(imageSection);

    auto fetchIrysPrice = [this]() {
        if (!m_ctImageUpload->hasImage() || m_ctImageUpload->imagePath().isEmpty()) {
            m_ctUploadInfo->setVisible(false);
            return;
        }
        const qint64 fileSize = QFile(m_ctImageUpload->imagePath()).size();
        if (fileSize <= 0) {
            m_ctUploadInfo->setVisible(false);
            return;
        }
        m_ctUploadCost->setText(tr("Fetching costs..."));
        m_ctUploadWarn->setVisible(false);
        m_ctUploadInfo->setVisible(true);
        CreateTokenHandler::UploadCostCallbacks callbacks;
        callbacks.onUpdated = [this](const QString& html, const QString& warningText,
                                     bool showWarning, quint64 storageLamports) {
            m_ctUploadCost->setText(html);
            m_ctUploadLamports = storageLamports;
            m_ctUploadWarn->setVisible(showWarning);
            if (showWarning) {
                m_ctUploadWarn->setText(warningText);
            }
        };

        m_createTokenHandler->fetchUploadAndRentCosts(m_ctImageUpload->imagePath(), m_walletAddress,
                                                      computeMintAccountSize(), m_solanaApi, this,
                                                      callbacks);
    };

    connect(m_ctImageUpload, &UploadWidget::imageSelected, this,
            [fetchIrysPrice]() { fetchIrysPrice(); });
    connect(m_ctImageUpload, &UploadWidget::imageCleared, this, [this]() {
        m_ctUploadInfo->setVisible(false);
        m_ctUploadLamports = 0;
    });

    connect(m_ctUploadCheck, &QCheckBox::toggled, this, [this, imageSection, scroll](bool checked) {
        scroll->setUpdatesEnabled(false);
        imageSection->setVisible(checked);
        if (!checked) {
            m_ctImageUpload->clear();
            m_ctUploadInfo->setVisible(false);
            m_ctUploadLamports = 0;
        }

        const bool showUri = !checked;
        if (m_ctUriLabel) {
            m_ctUriLabel->setVisible(showUri);
        }
        if (m_ctUri) {
            m_ctUri->setVisible(showUri);
        }
        if (m_ctUriHint) {
            m_ctUriHint->setVisible(showUri);
        }

        QTimer::singleShot(kImmediateUiDelayMs, this, [scroll]() {
            QTimer::singleShot(kImmediateUiDelayMs, scroll,
                               [scroll]() { scroll->setUpdatesEnabled(true); });
        });
    });

    m_ctUriLabel = makeLabel(tr("Metadata URI (optional)"));
    idLay->addWidget(m_ctUriLabel);
    m_ctUri = new QLineEdit();
    m_ctUri->setPlaceholderText(tr("e.g. https://arweave.net/..."));
    m_ctUri->setMaxLength(kUriMaxChars);
    m_ctUri->setObjectName("ctInput");
    idLay->addWidget(m_ctUri);
    m_ctUriHint = makeHint(tr("Max 200 characters. Points to off-chain JSON metadata."));
    idLay->addWidget(m_ctUriHint);

    return identityCard;
}

QWidget* SendReceivePage::buildCreateTokenSupplyCard() {
    auto makeLabel = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("txFormLabel");
        return lbl;
    };
    auto makeHint = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("ctHint11");
        lbl->setWordWrap(true);
        return lbl;
    };

    QWidget* supplyCard = new QWidget();
    supplyCard->setObjectName("ctSection");
    QVBoxLayout* supLay = new QVBoxLayout(supplyCard);
    supLay->setContentsMargins(kSectionMarginHorizontalPx, kSectionMarginVerticalPx,
                               kSectionMarginHorizontalPx, kSectionMarginVerticalPx);
    supLay->setSpacing(kSectionSpacingPx);

    QLabel* supTitle = new QLabel(tr("Supply & Decimals"));
    supTitle->setObjectName("ctSectionTitle16");
    supLay->addWidget(supTitle);

    QHBoxLayout* decSupRow = new QHBoxLayout();
    decSupRow->setSpacing(kRowSpacingPx);

    QVBoxLayout* decCol = new QVBoxLayout();
    decCol->setSpacing(kFieldHintSpacingPx);
    decCol->addWidget(makeLabel(tr("Decimals")));
    m_ctDecimals = new QSpinBox();
    m_ctDecimals->setRange(kDecimalsMin, kDecimalsMax);
    m_ctDecimals->setValue(kDecimalsDefault);
    m_ctDecimals->setObjectName("ctSpin");
    m_ctDecimals->setMinimumHeight(kSpinMinHeightPx);
    decCol->addWidget(m_ctDecimals);
    decCol->addWidget(makeHint(tr("Precision (0-9)")));
    decSupRow->addLayout(decCol, 1);

    QVBoxLayout* supplyCol = new QVBoxLayout();
    supplyCol->setSpacing(kFieldHintSpacingPx);
    supplyCol->addWidget(makeLabel(tr("Initial Supply (optional)")));
    m_ctSupply = new QLineEdit();
    m_ctSupply->setPlaceholderText(tr("0"));
    m_ctSupply->setValidator(
        new QDoubleValidator(0, kSupplyMaxValue, kDecimalsDefault, m_ctSupply));
    m_ctSupply->setObjectName("ctInput");
    supplyCol->addWidget(m_ctSupply);
    supplyCol->addWidget(makeHint(tr("Minted to your wallet. 0 to skip.")));
    decSupRow->addLayout(supplyCol, 2);
    supLay->addLayout(decSupRow);
    return supplyCard;
}

QWidget* SendReceivePage::buildCreateTokenAuthoritiesCard() {
    auto makeHint = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("ctHint11");
        lbl->setWordWrap(true);
        return lbl;
    };

    QWidget* authCard = new QWidget();
    authCard->setObjectName("ctSection");
    QVBoxLayout* authLay = new QVBoxLayout(authCard);
    authLay->setContentsMargins(kSectionMarginHorizontalPx, kSectionMarginVerticalPx,
                                kSectionMarginHorizontalPx, kSectionMarginVerticalPx);
    authLay->setSpacing(kSectionSpacingPx);

    QLabel* authTitle = new QLabel(tr("Authorities"));
    authTitle->setObjectName("ctSectionTitle16");
    authLay->addWidget(authTitle);

    QHBoxLayout* authRow = new QHBoxLayout();
    authRow->setSpacing(kRowSpacingPx);

    QVBoxLayout* freezeCol = new QVBoxLayout();
    freezeCol->setSpacing(kFieldHintSpacingPx);
    m_ctFreezeAuth = new StyledCheckbox(tr("Freeze Authority"));
    m_ctFreezeAuth->setChecked(true);
    m_ctFreezeAuth->setProperty("uiClass", "check13");
    freezeCol->addWidget(m_ctFreezeAuth);
    freezeCol->addWidget(makeHint(tr("Can freeze token accounts")));
    authRow->addLayout(freezeCol, 1);

    QVBoxLayout* mintCloseCol = new QVBoxLayout();
    mintCloseCol->setSpacing(kFieldHintSpacingPx);
    m_ctMintClose = new StyledCheckbox(tr("Mint Close Authority"));
    m_ctMintClose->setProperty("uiClass", "check13");
    mintCloseCol->addWidget(m_ctMintClose);
    mintCloseCol->addWidget(makeHint(tr("Can close mint & reclaim rent")));
    authRow->addLayout(mintCloseCol, 1);
    authLay->addLayout(authRow);
    return authCard;
}

QWidget* SendReceivePage::buildCreateTokenExtensionsCard(QScrollArea* scroll) {
    auto makeLabel = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("txFormLabel");
        return lbl;
    };
    auto makeHint = [](const QString& text) -> QLabel* {
        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("ctHint11");
        lbl->setWordWrap(true);
        return lbl;
    };

    QWidget* extCard = new QWidget();
    extCard->setObjectName("ctSection");
    QVBoxLayout* extLay = new QVBoxLayout(extCard);
    extLay->setContentsMargins(kSectionMarginHorizontalPx, kSectionMarginVerticalPx,
                               kSectionMarginHorizontalPx, kSectionMarginVerticalPx);
    extLay->setSpacing(kSectionSpacingPx);

    QLabel* extTitle = new QLabel(tr("Token-2022 Extensions"));
    extTitle->setObjectName("ctSectionTitle16");
    extLay->addWidget(extTitle);

    QLabel* extDesc = new QLabel(tr("Set at creation. Cannot be changed later."));
    extDesc->setObjectName("ctHint12");
    extLay->addWidget(extDesc);

    auto makeExtCard = [&](StyledCheckbox*& cb, const QString& label,
                           const QString& hint) -> QWidget* {
        ExtCardWidget* card = new ExtCardWidget();
        QVBoxLayout* lay = new QVBoxLayout(card);
        lay->setContentsMargins(kExtCardMarginHorizontalPx, kExtCardMarginVerticalPx,
                                kExtCardMarginHorizontalPx, kExtCardMarginVerticalPx);
        lay->setSpacing(kFieldHintSpacingPx);

        cb = new StyledCheckbox(label);
        cb->setProperty("uiClass", "check13Strong");
        lay->addWidget(cb);

        QLabel* h = new QLabel(hint);
        h->setObjectName("ctHint11Dim");
        h->setWordWrap(true);
        lay->addWidget(h);
        connect(cb, &QCheckBox::toggled, card, &ExtCardWidget::setChecked);
        return card;
    };

    QHBoxLayout* extGridRow1 = new QHBoxLayout();
    extGridRow1->setSpacing(kSectionSpacingPx);
    extGridRow1->addWidget(
        makeExtCard(m_ctTransferFee, tr("Transfer Fee"), tr("Charge fee on transfers")), 1);
    extGridRow1->addWidget(
        makeExtCard(m_ctNonTransferable, tr("Non-Transferable"), tr("Soulbound tokens")), 1);
    extLay->addLayout(extGridRow1);

    QHBoxLayout* extGridRow2 = new QHBoxLayout();
    extGridRow2->setSpacing(kSectionSpacingPx);
    extGridRow2->addWidget(
        makeExtCard(m_ctPermDelegate, tr("Permanent Delegate"), tr("Delegate over all accounts")),
        1);
    QWidget* extSpacer = new QWidget();
    extSpacer->setObjectName("srTransparentRow");
    extGridRow2->addWidget(extSpacer, 1);
    extLay->addLayout(extGridRow2);

    m_ctFeeDetails = new QWidget();
    m_ctFeeDetails->setObjectName("srTransparentRow");
    QVBoxLayout* feeDetailLay = new QVBoxLayout(m_ctFeeDetails);
    feeDetailLay->setContentsMargins(0, kSmallSpacingPx, 0, 0);
    feeDetailLay->setSpacing(kMediumSpacingPx);

    QFrame* feeSep = new QFrame();
    feeSep->setFrameShape(QFrame::HLine);
    feeSep->setObjectName("ctFeeSep");
    feeDetailLay->addWidget(feeSep);

    QLabel* feeConfigTitle = new QLabel(tr("Transfer Fee Configuration"));
    feeConfigTitle->setObjectName("ctSubtleInfo13");
    feeDetailLay->addWidget(feeConfigTitle);

    QHBoxLayout* feeRow = new QHBoxLayout();
    feeRow->setSpacing(kRowSpacingPx);
    QVBoxLayout* bpsCol = new QVBoxLayout();
    bpsCol->setSpacing(kFieldHintSpacingPx);
    bpsCol->addWidget(makeLabel(tr("Fee Rate (basis points)")));
    m_ctFeeBps = new QSpinBox();
    m_ctFeeBps->setRange(kFeeBpsMin, kFeeBpsMax);
    m_ctFeeBps->setValue(kFeeBpsDefault);
    m_ctFeeBps->setSuffix(tr(" bps"));
    m_ctFeeBps->setObjectName("ctSpin");
    m_ctFeeBps->setMinimumHeight(kSpinMinHeightPx);
    bpsCol->addWidget(m_ctFeeBps);
    bpsCol->addWidget(makeHint(tr("100 bps = 1%")));
    feeRow->addLayout(bpsCol, 1);

    QVBoxLayout* maxFeeCol = new QVBoxLayout();
    maxFeeCol->setSpacing(kFieldHintSpacingPx);
    maxFeeCol->addWidget(makeLabel(tr("Max Fee (token units)")));
    m_ctFeeMax = new QLineEdit();
    m_ctFeeMax->setPlaceholderText(tr("e.g. 1000"));
    m_ctFeeMax->setValidator(
        new QDoubleValidator(0, kSupplyMaxValue, kDecimalsDefault, m_ctFeeMax));
    m_ctFeeMax->setObjectName("ctInput");
    maxFeeCol->addWidget(m_ctFeeMax);
    maxFeeCol->addWidget(makeHint(tr("Cap per transfer")));
    feeRow->addLayout(maxFeeCol, 1);
    feeDetailLay->addLayout(feeRow);

    m_ctFeeDetails->setVisible(false);
    extLay->addWidget(m_ctFeeDetails);

    connect(m_ctTransferFee, &QCheckBox::toggled, this, [this, scroll](bool checked) {
        scroll->setUpdatesEnabled(false);
        m_ctFeeDetails->setVisible(checked);
        QTimer::singleShot(kImmediateUiDelayMs, this, [this, scroll]() {
            QTimer::singleShot(kImmediateUiDelayMs, this,
                               [scroll]() { scroll->setUpdatesEnabled(true); });
        });
    });

    return extCard;
}

QPushButton* SendReceivePage::buildCreateTokenReviewButton() {
    m_ctReviewBtn = new QPushButton(tr("Review Create Token"));
    m_ctReviewBtn->setCursor(Qt::PointingHandCursor);
    m_ctReviewBtn->setMinimumHeight(kActionButtonMinHeightPx);
    m_ctReviewBtn->setEnabled(false);
    m_ctReviewBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(m_ctReviewBtn, &QPushButton::clicked, this, [this]() {
        const SendReceiveCreateTokenFormState formState =
            m_createTokenHandler->parseCreateTokenFormState(collectCreateTokenBuildInput());
        const SendReceiveCreateTokenValidationResult validation =
            m_createTokenHandler->validateCreateTokenForm(formState);
        if (!validation.ok) {
            return;
        }
        m_mintKeypair = Keypair::generate();
        m_isCreateTokenReview = true;
        populateCreateTokenReview();
        setCurrentPage(StackPage::Review);
    });
    updateCreateTokenReviewButtonState();
    return m_ctReviewBtn;
}

SendReceiveCreateTokenBuildInput SendReceivePage::collectCreateTokenBuildInput() const {
    return {m_walletAddress,
            m_mintKeypair.address(),
            m_ctName ? m_ctName->text() : QString(),
            m_ctSymbol ? m_ctSymbol->text() : QString(),
            m_ctUri ? m_ctUri->text() : QString(),
            m_ctDecimals ? m_ctDecimals->value() : 0,
            m_ctSupply ? m_ctSupply->text() : QString(),
            m_ctFreezeAuth ? m_ctFreezeAuth->isChecked() : false,
            m_ctTransferFee ? m_ctTransferFee->isChecked() : false,
            m_ctFeeBps ? m_ctFeeBps->value() : 0,
            m_ctFeeMax ? m_ctFeeMax->text() : QString(),
            m_ctNonTransferable ? m_ctNonTransferable->isChecked() : false,
            m_ctMintClose ? m_ctMintClose->isChecked() : false,
            m_ctPermDelegate ? m_ctPermDelegate->isChecked() : false,
            computeMintAccountSize()};
}

void SendReceivePage::updateCreateTokenReviewButtonState() {
    if (!m_ctReviewBtn) {
        return;
    }

    const SendReceiveCreateTokenFormState formState =
        m_createTokenHandler->parseCreateTokenFormState(collectCreateTokenBuildInput());
    const SendReceiveCreateTokenValidationResult validation =
        m_createTokenHandler->validateCreateTokenForm(formState);

    m_ctReviewBtn->setEnabled(validation.ok);
    m_ctReviewBtn->setStyleSheet(validation.ok ? Theme::primaryBtnStyle
                                               : Theme::primaryBtnDisabledStyle);
}

// ── Create Token: Mint Account Size ─────────────────────────────

quint64 SendReceivePage::computeMintAccountSize() const {
    const SendReceiveMintSizeInput input{
        m_ctName->text(),
        m_ctSymbol->text(),
        m_ctUri->text(),
        m_ctTransferFee->isChecked(),
        m_ctNonTransferable->isChecked(),
        m_ctMintClose->isChecked(),
        m_ctPermDelegate->isChecked(),
    };
    return m_createTokenHandler->computeMintAccountSize(input);
}

// ── Create Token: Review Page ───────────────────────────────────
