#include "features/setup/SetupPage.h"
#include "Theme.h"
#include "crypto/HardwareWalletPlugin.h"
#include "crypto/bip39_wordlist.h"
#include "widgets/AddressLink.h"
#include "widgets/PaintedPanel.h"
#include "widgets/StyledCheckbox.h"
#include "widgets/StyledLineEdit.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCompleter>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSequentialAnimationGroup>
#include <QStackedWidget>
#include <QStringListModel>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <sodium.h>

static const int CONFETTI_PIECE_COUNT = 50;

static QStringList bip39WordList() {
    static QStringList words;
    if (words.isEmpty()) {
        words.reserve(Bip39::WORDLIST_SIZE);
        for (int i = 0; i < Bip39::WORDLIST_SIZE; ++i) {
            words.append(QString::fromLatin1(Bip39::WORDLIST[i]));
        }
    }
    return words;
}

static void attachBip39Completer(QLineEdit* input) {
    QCompleter* completer = new QCompleter(bip39WordList(), input);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchStartsWith);
    completer->setMaxVisibleItems(6);

    QAbstractItemView* popup = completer->popup();
    popup->setProperty("uiClass", "setupBip39CompleterPopup");

    input->setCompleter(completer);
}

static void refreshWidgetStyle(QWidget* widget) {
    if (!widget) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

static QWidget* createSetupWordCell(int index, QLineEdit** inputSlot,
                                    const QString& placeholderText = QString()) {
    QWidget* cell = new QWidget();
    cell->setProperty("uiClass", "setupWordCell");
    QHBoxLayout* cellLayout = new QHBoxLayout(cell);
    cellLayout->setContentsMargins(18, 14, 18, 14);
    cellLayout->setSpacing(6);

    QLabel* num = new QLabel(QString("%1.").arg(index + 1));
    num->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cellLayout->addWidget(num);

    QLineEdit* input = new QLineEdit();
    input->setPlaceholderText(placeholderText.isEmpty() ? QObject::tr("Word %1").arg(index + 1)
                                                        : placeholderText);
    if (inputSlot) {
        *inputSlot = input;
    }
    cellLayout->addWidget(input, 1);
    return cell;
}

static QWidget* createHardwareStepRow(const QString& accent, int num, const QString& text) {
    QWidget* row = new QWidget();
    row->setProperty("uiClass", "setupHardwareStepRow");
    QHBoxLayout* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(14);

    QLabel* circle = new QLabel(QString::number(num));
    circle->setFixedSize(32, 32);
    circle->setAlignment(Qt::AlignCenter);
    circle->setProperty("uiClass", "setupHardwareStepCircle");
    circle->setProperty("accent", accent);
    hl->addWidget(circle);

    QLabel* label = new QLabel(text);
    label->setProperty("uiClass", "setupHardwareStepLabel");
    hl->addWidget(label);
    hl->addStretch();
    return row;
}

SetupPage::SetupPage(QWidget* parent) : QWidget(parent) {
    setObjectName("walletPage");

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();
    m_stack->addWidget(buildWelcomePage());        // Step::Welcome
    m_stack->addWidget(buildSetupCards());         // Step::SetupCards
    m_stack->addWidget(buildSetPasswordPage());    // Step::SetPassword
    m_stack->addWidget(buildCreateWallet());       // Step::CreateWallet
    m_stack->addWidget(buildConfirmRecovery());    // Step::ConfirmRecovery
    m_stack->addWidget(buildImportWallet());       // Step::ImportRecovery
    m_stack->addWidget(buildImportPrivateKey());   // Step::ImportPrivateKey
    m_stack->addWidget(buildSuccessPage());        // Step::Success
    m_stack->addWidget(buildSelectHardwareType()); // Step::SelectHardwareType
    m_stack->addWidget(buildLedgerDetect());       // Step::LedgerDetect
    m_stack->addWidget(buildTrezorDetect());       // Step::TrezorDetect
    m_stack->addWidget(buildLatticeDetect());      // Step::LatticeDetect

    outerLayout->addWidget(m_stack);
}

void SetupPage::showStep(Step step) { m_stack->setCurrentIndex(static_cast<int>(step)); }

// ── Welcome ─────────────────────────────────────────────────────

QWidget* SetupPage::buildWelcomePage() {
    QWidget* page = new QWidget();
    page->setObjectName("walletContent");
    page->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 0, 48, 48);
    layout->setSpacing(0);
    layout->addStretch(2);

    // Cinder flame logo — scale via QImage to avoid dark fringe from QPixmap scaling
    QLabel* logo = new QLabel();
    logo->setProperty("uiClass", "setupTransparentBox");
    {
        QImage flameImg(":/images/cinder-flame.png");
        if (!flameImg.isNull()) {
            // Convert to premultiplied alpha before scaling to prevent dark halos
            flameImg.convertTo(QImage::Format_ARGB32_Premultiplied);
            flameImg =
                flameImg.scaledToHeight(120 * logo->devicePixelRatioF(), Qt::SmoothTransformation);
            QPixmap flamePx = QPixmap::fromImage(flameImg);
            flamePx.setDevicePixelRatio(logo->devicePixelRatioF());
            logo->setPixmap(flamePx);
        }
    }
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo);
    layout->addSpacing(28);

    // Title
    QLabel* title = new QLabel(tr("Welcome to Cinder"));
    title->setAlignment(Qt::AlignCenter);
    title->setProperty("uiClass", "setupHeroTitle");
    layout->addWidget(title);
    layout->addSpacing(12);

    // Subtitle
    QLabel* subtitle = new QLabel(tr("A desktop Solana wallet"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setProperty("uiClass", "setupMutedDesc14");
    layout->addWidget(subtitle);
    layout->addStretch(2);

    // TOS checkbox row
    QHBoxLayout* tosRow = new QHBoxLayout();
    tosRow->setContentsMargins(0, 0, 0, 0);
    tosRow->setAlignment(Qt::AlignCenter);

    auto* tosCheck = new StyledCheckbox(tr("I agree to the"));
    tosRow->addWidget(tosCheck);

    QPushButton* tosLink = new QPushButton(tr("Terms of Service"));
    tosLink->setCursor(Qt::PointingHandCursor);
    tosLink->setProperty("uiClass", "setupLinkButton");
    connect(tosLink, &QPushButton::clicked, this, [this]() { showTosModal(); });
    tosRow->addWidget(tosLink);
    tosRow->addStretch();

    layout->addLayout(tosRow);
    layout->addSpacing(20);

    // Get Started button — disabled until TOS accepted
    QPushButton* btn = new QPushButton(tr("Get Started"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(54);
    btn->setEnabled(false);
    btn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(tosCheck, &QCheckBox::toggled, this, [btn](bool checked) {
        btn->setEnabled(checked);
        btn->setStyleSheet(checked ? Theme::primaryBtnStyle : Theme::primaryBtnDisabledStyle);
    });
    connect(btn, &QPushButton::clicked, this, [this]() { showStep(Step::SetupCards); });
    layout->addWidget(btn);

    layout->addStretch(1);

    return page;
}

void SetupPage::showTosModal() {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Terms of Service"));
    dialog->setMinimumSize(700, 500);
    dialog->resize(750, 600);
    dialog->setProperty("uiClass", "setupDialog");

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(32, 24, 32, 24);
    layout->setSpacing(16);

    QLabel* title = new QLabel(tr("Terms of Service"));
    title->setProperty("uiClass", "setupModalTitle");
    layout->addWidget(title);

    QTextEdit* text = new QTextEdit();
    text->setReadOnly(true);
    text->setProperty("uiClass", "setupTosText");

    // Load TOS from file or resource
    QFile tosFile(":/legal/TERMS_OF_SERVICE.md");
    if (tosFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        text->setMarkdown(QString::fromUtf8(tosFile.readAll()));
        tosFile.close();
    } else {
        // Fallback: try loading from disk
        QFile diskFile(QCoreApplication::applicationDirPath() + "/../../../TERMS_OF_SERVICE.md");
        if (diskFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            text->setMarkdown(QString::fromUtf8(diskFile.readAll()));
            diskFile.close();
        } else {
            text->setPlainText(tr("Terms of Service could not be loaded."));
        }
    }

    layout->addWidget(text);

    QPushButton* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedHeight(44);
    closeBtn->setStyleSheet(Theme::primaryBtnStyle);
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

// ── Setup Cards ─────────────────────────────────────────────────

QWidget* SetupPage::buildSetupCards() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 40);
    layout->setSpacing(20);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        if (m_importMode) {
            // Restore grid to original 2x2 layout for next time
            m_cardGrid->removeWidget(m_importRecoveryCard);
            m_cardGrid->removeWidget(m_importKeyCard);
            m_cardGrid->removeWidget(m_hardwareCard);
            m_createCard->show();
            m_cardGrid->addWidget(m_createCard, 0, 0);
            m_cardGrid->addWidget(m_importRecoveryCard, 0, 1);
            m_cardGrid->addWidget(m_importKeyCard, 1, 0);
            m_cardGrid->addWidget(m_hardwareCard, 1, 1);
            m_importMode = false;
            emit backRequested();
        } else {
            showStep(Step::Welcome);
        }
    });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Wallet Setup"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    layout->addSpacing(10);

    // 2x2 card grid in a width-constrained container
    m_cardGrid = new QGridLayout();
    m_cardGrid->setContentsMargins(60, 0, 60, 0);
    m_cardGrid->setHorizontalSpacing(80);
    m_cardGrid->setVerticalSpacing(24);

    m_createCard =
        createActionCard(":/icons/wallet/create-wallet.png", QColor(16, 185, 129), // green accent
                         tr("Create a Wallet"), tr("Generate a new wallet and seed phrase."), 140);
    m_createCard->setProperty("cardId", "createWallet");
    m_cardGrid->addWidget(m_createCard, 0, 0);

    m_importRecoveryCard = createActionCard(
        ":/icons/wallet/import-recovery.png", QColor(168, 85, 247), // purple accent
        tr("Import using Recovery Phrase"), tr("Restore your wallet using a seed phrase."), 140);
    m_importRecoveryCard->setProperty("cardId", "importWallet");
    m_cardGrid->addWidget(m_importRecoveryCard, 0, 1);

    m_importKeyCard = createActionCard(
        ":/icons/wallet/import-privatekey.png", QColor(234, 179, 8), // yellow accent
        tr("Import using Private Key"), tr("Import a wallet using a private key."), 140);
    m_importKeyCard->setProperty("cardId", "importPrivateKey");
    m_cardGrid->addWidget(m_importKeyCard, 1, 0);

    m_hardwareCard = createActionCard(
        ":/icons/wallet/import-wallet.png", QColor(59, 130, 246), // blue accent
        tr("Connect Hardware Wallet"), tr("Use a Ledger device to sign transactions."), 140);
    m_hardwareCard->setProperty("cardId", "connectHardware");
    m_cardGrid->addWidget(m_hardwareCard, 1, 1);

    layout->addLayout(m_cardGrid);
    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

// ── Set Password (index 1) ──────────────────────────────────────

QWidget* SetupPage::buildSetPasswordPage() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 24, 48, 40);
    layout->setSpacing(0);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SetupCards); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);
    layout->addSpacing(20);

    // Title
    QLabel* title = new QLabel(tr("Set Password"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);
    layout->addSpacing(12);

    // Description
    QLabel* desc = new QLabel(tr("Create a password to encrypt and protect your wallet. "
                                 "You'll use this to unlock Cinder each time you open it."));
    desc->setProperty("uiClass", "setupMutedDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);
    layout->addSpacing(32);

    // Password label
    QLabel* pwLabel = new QLabel(tr("Password"));
    pwLabel->setProperty("uiClass", "setupFieldLabel14");
    layout->addWidget(pwLabel);
    layout->addSpacing(6);

    // Password input
    m_passwordInput = new QLineEdit();
    m_passwordInput->setProperty("uiClass", "setupTextInput");
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setPlaceholderText(tr("Enter password"));
    m_passwordInput->setFixedHeight(46);
    layout->addWidget(m_passwordInput);
    layout->addSpacing(16);

    // Confirm password label
    QLabel* confirmLabel = new QLabel(tr("Confirm Password"));
    confirmLabel->setProperty("uiClass", "setupFieldLabel14");
    layout->addWidget(confirmLabel);
    layout->addSpacing(6);

    // Confirm password input
    m_confirmPasswordInput = new QLineEdit();
    m_confirmPasswordInput->setProperty("uiClass", "setupTextInput");
    m_confirmPasswordInput->setEchoMode(QLineEdit::Password);
    m_confirmPasswordInput->setPlaceholderText(tr("Confirm password"));
    m_confirmPasswordInput->setFixedHeight(46);
    layout->addWidget(m_confirmPasswordInput);
    layout->addSpacing(12);

    // Requirements label
    QLabel* reqLabel = new QLabel(tr("Must be at least 8 characters"));
    reqLabel->setProperty("uiClass", "setupHelpText13");
    reqLabel->setProperty("validState", false);
    layout->addWidget(reqLabel);
    layout->addSpacing(4);

    // Error label (hidden by default)
    m_passwordError = new QLabel();
    m_passwordError->setProperty("uiClass", "setupErrorText13");
    m_passwordError->setVisible(false);
    layout->addWidget(m_passwordError);

    layout->addStretch();

    // Continue button
    m_passwordContinueBtn = new QPushButton(tr("Continue"));
    m_passwordContinueBtn->setCursor(Qt::PointingHandCursor);
    m_passwordContinueBtn->setFixedHeight(54);
    m_passwordContinueBtn->setEnabled(false);
    m_passwordContinueBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);

    // Validation logic — updates button state in real-time, but only shows
    // mismatch error after the confirm field loses focus
    auto updateButton = [this, reqLabel]() {
        const QString pw = m_passwordInput->text();
        const QString confirm = m_confirmPasswordInput->text();
        bool meetsLength = pw.length() >= 8;
        bool matches = !confirm.isEmpty() && pw == confirm;
        bool valid = meetsLength && matches;

        // Update requirements color
        reqLabel->setProperty("validState", meetsLength);
        refreshWidgetStyle(reqLabel);

        // Clear error as soon as passwords match
        if (matches) {
            m_passwordError->setVisible(false);
        }

        m_passwordContinueBtn->setEnabled(valid);
        m_passwordContinueBtn->setStyleSheet(valid ? Theme::primaryBtnStyle
                                                   : Theme::primaryBtnDisabledStyle);
    };

    connect(m_passwordInput, &QLineEdit::textChanged, this, updateButton);
    connect(m_confirmPasswordInput, &QLineEdit::textChanged, this, updateButton);

    // Show mismatch error only when user leaves the confirm field
    connect(m_confirmPasswordInput, &QLineEdit::editingFinished, this, [this]() {
        const QString pw = m_passwordInput->text();
        const QString confirm = m_confirmPasswordInput->text();
        if (!confirm.isEmpty() && pw != confirm) {
            m_passwordError->setText(tr("Passwords do not match"));
            m_passwordError->setVisible(true);
        } else {
            m_passwordError->setVisible(false);
        }
    });

    // Continue button action — routes to whatever the user chose on SetupCards
    connect(m_passwordContinueBtn, &QPushButton::clicked, this, [this]() {
        m_pendingPassword = m_passwordInput->text();
        // Clear UI fields immediately
        m_passwordInput->clear();
        m_confirmPasswordInput->clear();
        m_passwordError->setVisible(false);

        // Route to the chosen wallet flow
        if (m_pendingWalletFlow == Step::CreateWallet) {
            generateNewMnemonic();
        }
        showStep(m_pendingWalletFlow);
    });

    layout->addWidget(m_passwordContinueBtn);

    scroll->setWidget(content);

    return scroll;
}

// ── Create Wallet (index 2) ─────────────────────────────────────

QWidget* SetupPage::buildCreateWallet() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 24, 48, 40);
    layout->setSpacing(0);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SetupCards); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);
    layout->addSpacing(20);

    // Title
    QLabel* title = new QLabel(tr("Create New Wallet"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);
    layout->addSpacing(12);

    // Warning — fixed-height container so grid starts at same Y as confirm page
    QWidget* headerSpacer = new QWidget();
    headerSpacer->setFixedHeight(48);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerSpacer);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    QLabel* warning =
        new QLabel(tr("WARNING: Your recovery phrase is the only way to recover your wallet. "
                      "Write these words down in order and store them in a safe place. "
                      "Never share your recovery phrase with anyone."));
    warning->setObjectName("walletWarning");
    warning->setWordWrap(true);
    headerLayout->addWidget(warning);
    headerLayout->addStretch();
    layout->addWidget(headerSpacer);

    // Seed phrase grid (24 words, 5 columns)
    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    for (int i = 0; i < 24; ++i) {
        int row = i / 5;
        int col = i % 5;

        QLabel* cell = new QLabel(QString("%1.  --------").arg(i + 1));
        cell->setObjectName("seedWord");
        cell->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_seedWordLabels[i] = cell;
        grid->addWidget(cell, row, col);
    }

    for (int c = 0; c < 5; ++c) {
        grid->setColumnStretch(c, 1);
    }

    layout->addLayout(grid);
    layout->addSpacing(24);

    // Passphrase row
    QHBoxLayout* ppRow = new QHBoxLayout();
    ppRow->setSpacing(12);

    PaintedPanel* passphraseBox = new PaintedPanel();
    passphraseBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    QHBoxLayout* ppBoxLayout = new QHBoxLayout(passphraseBox);
    ppBoxLayout->setContentsMargins(16, 12, 20, 12);

    StyledCheckbox* passphraseCheck = new StyledCheckbox(tr("Use passphrase?"));
    ppBoxLayout->addWidget(passphraseCheck);

    m_createPassphraseInput = new StyledLineEdit();
    m_createPassphraseInput->setPlaceholderText(tr("Enter passphrase..."));
    m_createPassphraseInput->setEchoMode(QLineEdit::Password);
    m_createPassphraseInput->setFixedHeight(46);
    m_createPassphraseInput->setVisible(false);

    connect(passphraseCheck, &QCheckBox::toggled, m_createPassphraseInput, &QLineEdit::setVisible);

    ppRow->addWidget(passphraseBox);
    ppRow->addWidget(m_createPassphraseInput, 1);
    ppRow->addStretch();

    layout->addLayout(ppRow);
    layout->addStretch();

    // Bottom row: saved checkbox + Continue button
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(28);

    StyledCheckbox* savedCheck = new StyledCheckbox(tr("I have saved my secret recovery phrase"));

    QPushButton* continueBtn = new QPushButton(tr("Continue"));
    continueBtn->setCursor(Qt::PointingHandCursor);
    continueBtn->setFixedHeight(54);
    continueBtn->setEnabled(false);
    continueBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);

    connect(savedCheck, &QCheckBox::toggled, this, [continueBtn](bool checked) {
        continueBtn->setEnabled(checked);
        continueBtn->setStyleSheet(checked ? Theme::primaryBtnStyle
                                           : Theme::primaryBtnDisabledStyle);
    });

    connect(continueBtn, &QPushButton::clicked, this, &SetupPage::showConfirmPage);

    bottomRow->addWidget(savedCheck);
    bottomRow->addWidget(continueBtn, 1);

    layout->addLayout(bottomRow);

    scroll->setWidget(content);

    return scroll;
}

// ── Import Wallet (index 3) ─────────────────────────────────────

QWidget* SetupPage::buildImportWallet() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 24, 48, 40);
    layout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SetupCards); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Import using Recovery Phrase"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Description
    QLabel* desc =
        new QLabel(tr("Enter your 12 or 24-word recovery phrase to restore your wallet."));
    desc->setProperty("uiClass", "setupMutedDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(8);

    // Seed phrase input grid (24 inputs, 5 columns — same layout as create wallet)
    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    for (int i = 0; i < 24; ++i) {
        int row = i / 5;
        int col = i % 5;

        QWidget* cell = createSetupWordCell(i, &m_importWordInputs[i]);
        QLineEdit* input = m_importWordInputs[i];
        attachBip39Completer(input);
        grid->addWidget(cell, row, col);
    }

    for (int c = 0; c < 5; ++c) {
        grid->setColumnStretch(c, 1);
    }

    layout->addLayout(grid);

    layout->addSpacing(12);

    // Passphrase row
    QHBoxLayout* ppRow = new QHBoxLayout();
    ppRow->setSpacing(12);

    PaintedPanel* passphraseBox = new PaintedPanel();
    passphraseBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    QHBoxLayout* ppBoxLayout = new QHBoxLayout(passphraseBox);
    ppBoxLayout->setContentsMargins(16, 12, 20, 12);

    StyledCheckbox* passphraseCheck = new StyledCheckbox(tr("Use passphrase?"));
    ppBoxLayout->addWidget(passphraseCheck);

    m_importPassphraseInput = new StyledLineEdit();
    m_importPassphraseInput->setPlaceholderText(tr("Enter passphrase..."));
    m_importPassphraseInput->setEchoMode(QLineEdit::Password);
    m_importPassphraseInput->setFixedHeight(46);
    m_importPassphraseInput->setVisible(false);

    connect(passphraseCheck, &QCheckBox::toggled, m_importPassphraseInput, &QLineEdit::setVisible);

    ppRow->addWidget(passphraseBox);
    ppRow->addWidget(m_importPassphraseInput, 1);
    ppRow->addStretch();

    layout->addLayout(ppRow);

    m_importError = new QLabel();
    m_importError->setProperty("uiClass", "setupErrorText14");
    m_importError->setWordWrap(true);
    m_importError->setVisible(false);
    layout->addWidget(m_importError);

    layout->addStretch();

    // Import button — starts disabled, enables when 12 or 24 words filled
    QPushButton* importBtn = new QPushButton(tr("Import Wallet"));
    importBtn->setCursor(Qt::PointingHandCursor);
    importBtn->setMinimumHeight(48);
    importBtn->setEnabled(false);
    importBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(importBtn, &QPushButton::clicked, this, &SetupPage::finalizeImportWallet);

    for (int i = 0; i < 24; ++i) {
        connect(m_importWordInputs[i], &QLineEdit::textChanged, this, [this, importBtn]() {
            // Count sequential filled inputs from the start
            int sequential = 0;
            for (int k = 0; k < 24; ++k) {
                if (!m_importWordInputs[k]->text().trimmed().isEmpty()) {
                    sequential++;
                } else {
                    break;
                }
            }
            bool ready = (sequential >= 12);
            importBtn->setEnabled(ready);
            importBtn->setStyleSheet(ready ? Theme::primaryBtnStyle
                                           : Theme::primaryBtnDisabledStyle);
        });
    }

    layout->addWidget(importBtn);

    scroll->setWidget(content);

    return scroll;
}

// ── Import Private Key (index 4) ─────────────────────────────────

QWidget* SetupPage::buildImportPrivateKey() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 24, 48, 40);
    layout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SetupCards); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Import using Private Key"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    // Description
    QLabel* desc = new QLabel(tr("Paste your Base58-encoded private key to import your wallet."));
    desc->setProperty("uiClass", "setupMutedDesc14");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    layout->addSpacing(8);

    // Private key input
    QLabel* inputLabel = new QLabel(tr("Private Key"));
    inputLabel->setProperty("uiClass", "setupFieldLabel14");
    layout->addWidget(inputLabel);

    m_privateKeyInput = new StyledLineEdit();
    m_privateKeyInput->setProperty("uiClass", "setupTextInput");
    m_privateKeyInput->setPlaceholderText(tr("Enter your Base58 private key"));
    m_privateKeyInput->setFixedHeight(46);
    layout->addWidget(m_privateKeyInput);

    m_importKeyError = new QLabel();
    m_importKeyError->setProperty("uiClass", "setupErrorText14");
    m_importKeyError->setWordWrap(true);
    m_importKeyError->setVisible(false);
    layout->addWidget(m_importKeyError);

    layout->addStretch();

    // Import button
    QPushButton* importBtn = new QPushButton(tr("Import Wallet"));
    importBtn->setCursor(Qt::PointingHandCursor);
    importBtn->setMinimumHeight(48);
    importBtn->setEnabled(false);
    importBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(importBtn, &QPushButton::clicked, this, &SetupPage::finalizeImportPrivateKey);

    connect(m_privateKeyInput, &QLineEdit::textChanged, this, [importBtn](const QString& text) {
        bool ready = text.trimmed().length() >= 32;
        importBtn->setEnabled(ready);
        importBtn->setStyleSheet(ready ? Theme::primaryBtnStyle : Theme::primaryBtnDisabledStyle);
    });

    layout->addWidget(importBtn);

    scroll->setWidget(content);

    return scroll;
}

// ── Confirm Recovery (index 2) ──────────────────────────────────

QWidget* SetupPage::buildConfirmRecovery() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 24, 48, 40);
    layout->setSpacing(0);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::CreateWallet); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);
    layout->addSpacing(20);

    // Title
    QLabel* title = new QLabel(tr("Confirm Recovery Phrase"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);
    layout->addSpacing(12);

    // Description — fixed-height container matching create page so grids align
    QWidget* headerSpacer = new QWidget();
    headerSpacer->setFixedHeight(48);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerSpacer);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);
    QLabel* desc =
        new QLabel(tr("Fill in the missing words to verify you saved your recovery phrase."));
    desc->setProperty("uiClass", "setupMutedDesc14");
    desc->setWordWrap(true);
    headerLayout->addWidget(desc);
    headerLayout->addStretch();
    layout->addWidget(headerSpacer);

    // Seed phrase grid — same 5-column layout, but some cells are inputs
    m_confirmGrid = new QGridLayout();
    m_confirmGrid->setHorizontalSpacing(12);
    m_confirmGrid->setVerticalSpacing(12);

    for (int i = 0; i < 24; ++i) {
        int row = i / 5;
        int col = i % 5;

        // Each cell is a label by default; challenge cells get swapped in showConfirmPage
        QLabel* cell = new QLabel(QString("%1.  --------").arg(i + 1));
        cell->setObjectName("seedWord");
        cell->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_confirmWordLabels[i] = cell;
        m_confirmGrid->addWidget(cell, row, col);
    }

    for (int c = 0; c < 5; ++c) {
        m_confirmGrid->setColumnStretch(c, 1);
    }

    layout->addLayout(m_confirmGrid);

    m_confirmError = new QLabel();
    m_confirmError->setProperty("uiClass", "setupErrorText16Strong");
    m_confirmError->setWordWrap(true);
    m_confirmError->setVisible(false);
    layout->addWidget(m_confirmError);

    layout->addStretch();

    // Confirm button
    m_confirmBtn = new QPushButton(tr("Confirm & Create Wallet"));
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    m_confirmBtn->setFixedHeight(54);
    m_confirmBtn->setEnabled(false);
    m_confirmBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    connect(m_confirmBtn, &QPushButton::clicked, this, &SetupPage::validateConfirmation);
    layout->addWidget(m_confirmBtn);

    scroll->setWidget(content);

    return scroll;
}

// ── Crypto Integration ──────────────────────────────────────────

void SetupPage::generateNewMnemonic() {
    m_generatedMnemonic = m_handler.generateMnemonic(24);
    QStringList words = m_generatedMnemonic.split(' ');

    for (int i = 0; i < 24 && i < words.size(); ++i) {
        m_seedWordLabels[i]->setText(QString("%1.  %2").arg(i + 1).arg(words[i]));
    }
}

void SetupPage::startCreateFlow(const QString& sessionPassword) {
    m_pendingPassword = sessionPassword;
    generateNewMnemonic();
    showStep(Step::CreateWallet);
}

void SetupPage::startImportFlow(const QString& sessionPassword) {
    m_pendingPassword = sessionPassword;
    m_importMode = true;

    // Remove all cards from the grid and re-add without the create card
    m_cardGrid->removeWidget(m_createCard);
    m_createCard->hide();
    m_cardGrid->removeWidget(m_importRecoveryCard);
    m_cardGrid->removeWidget(m_importKeyCard);
    m_cardGrid->removeWidget(m_hardwareCard);

    m_cardGrid->addWidget(m_importRecoveryCard, 0, 0);
    m_cardGrid->addWidget(m_importKeyCard, 0, 1);
    m_cardGrid->addWidget(m_hardwareCard, 1, 0);

    showStep(Step::SetupCards);
}

void SetupPage::showConfirmPage() {
    if (m_generatedMnemonic.isEmpty()) {
        return;
    }

    m_recoveryChallenge = m_handler.buildRecoveryChallenge(m_generatedMnemonic, 5);
    const QStringList words = m_recoveryChallenge.words;
    const QList<int> challenge = m_recoveryChallenge.indices;
    for (int i = 0; i < challenge.size(); ++i) {
        m_challengeIndices[i] = challenge[i];
    }

    // Remove old input cells from previous attempts (labels stay, inputs get replaced)
    for (int i = 0; i < 5; ++i) {
        if (m_confirmInputs[i]) {
            QWidget* oldCell = m_confirmInputs[i]->parentWidget();
            if (oldCell) {
                m_confirmGrid->removeWidget(oldCell);
                oldCell->deleteLater();
            }
            m_confirmInputs[i] = nullptr;
        }
    }
    m_confirmError->setVisible(false);

    // Fill the confirm grid — show words for non-challenge slots, inputs for challenge
    int inputIdx = 0;
    for (int i = 0; i < 24; ++i) {
        bool isChallenge = challenge.contains(i);

        if (isChallenge) {
            // Replace label with input cell — inline style to beat viewport cascade
            QWidget* inputCell =
                createSetupWordCell(i, &m_confirmInputs[inputIdx], tr("Enter word"));
            QLineEdit* input = m_confirmInputs[inputIdx];
            attachBip39Completer(input);

            // Enable confirm button when all 5 inputs have text
            connect(input, &QLineEdit::textChanged, this, [this](const QString&) {
                bool allFilled = true;
                for (int k = 0; k < 5; ++k) {
                    if (m_confirmInputs[k]->text().trimmed().isEmpty()) {
                        allFilled = false;
                        break;
                    }
                }
                m_confirmBtn->setEnabled(allFilled);
                m_confirmBtn->setStyleSheet(allFilled ? Theme::primaryBtnStyle
                                                      : Theme::primaryBtnDisabledStyle);
            });

            // Swap the label for the input widget in the grid
            int row = i / 5;
            int col = i % 5;
            m_confirmWordLabels[i]->hide();
            m_confirmGrid->addWidget(inputCell, row, col);

            inputIdx++;
        } else {
            m_confirmWordLabels[i]->setText(QString("%1.  %2").arg(i + 1).arg(words[i]));
            m_confirmWordLabels[i]->show();
        }
    }

    m_confirmBtn->setEnabled(false);
    m_confirmBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    showStep(Step::ConfirmRecovery);
}

void SetupPage::validateConfirmation() {
    QStringList enteredWords;
    for (int i = 0; i < 5; ++i) {
        enteredWords.append(m_confirmInputs[i]->text());
    }

    QString error;
    if (!m_handler.validateRecoveryChallenge(m_recoveryChallenge, enteredWords, &error)) {
        m_confirmError->setText(error);
        m_confirmError->setVisible(true);

        // Red border on the incorrect input's parent cell
        if (!error.isEmpty()) {
            const QRegularExpression indexPattern("(\\d+)");
            const QRegularExpressionMatch match = indexPattern.match(error);
            if (match.hasMatch()) {
                const int oneBasedIndex = match.captured(1).toInt();
                for (int i = 0; i < m_recoveryChallenge.indices.size(); ++i) {
                    if (m_recoveryChallenge.indices[i] + 1 == oneBasedIndex) {
                        // Set red border on the parent cell widget
                        QWidget* cell = m_confirmInputs[i]->parentWidget();
                        if (cell) {
                            cell->setProperty("errorState", true);
                            refreshWidgetStyle(cell);
                            // Clear red border when user types
                            connect(m_confirmInputs[i], &QLineEdit::textChanged, cell,
                                    [cell, this]() {
                                        cell->setProperty("errorState", false);
                                        refreshWidgetStyle(cell);
                                        m_confirmError->setVisible(false);
                                    });
                        }
                        m_confirmInputs[i]->setFocus();
                        break;
                    }
                }
            }
        }
        return;
    }

    finalizeCreateWallet();
}

void SetupPage::finalizeCreateWallet() {
    SetupHandler::WalletCompletion completion;
    QString error;
    const SetupHandler::CreateWalletRequest request{
        m_generatedMnemonic,
        m_createPassphraseInput->isVisible() ? m_createPassphraseInput->text() : QString(),
        m_pendingPassword,
    };
    if (!m_handler.completeCreateWallet(request, &completion, &error)) {
        m_confirmError->setText(error);
        m_confirmError->setVisible(true);
        return;
    }

    sodium_memzero(m_generatedMnemonic.data(), m_generatedMnemonic.size() * sizeof(QChar));
    m_generatedMnemonic.clear();
    handleWalletCompletion(completion);
}

void SetupPage::finalizeImportWallet() {
    QStringList words;
    for (int i = 0; i < 24; ++i) {
        QString w = m_importWordInputs[i]->text().trimmed().toLower();
        if (!w.isEmpty()) {
            words.append(w);
        }
    }

    // Support 12 or 24 word phrases
    if (words.size() != 12 && words.size() != 24) {
        m_importError->setText(tr("Please enter a valid 12 or 24-word recovery phrase."));
        m_importError->setVisible(true);
        return;
    }

    SetupHandler::WalletCompletion completion;
    QString error;
    const SetupHandler::ImportRecoveryRequest request{
        words,
        m_importPassphraseInput->isVisible() ? m_importPassphraseInput->text() : QString(),
        m_pendingPassword,
    };
    if (!m_handler.completeImportRecovery(request, &completion, &error)) {
        m_importError->setText(error);
        m_importError->setVisible(true);
        return;
    }

    handleWalletCompletion(completion);
}

void SetupPage::finalizeImportPrivateKey() {
    SetupHandler::WalletCompletion completion;
    QString error;
    const SetupHandler::ImportPrivateKeyRequest request{m_privateKeyInput->text(),
                                                        m_pendingPassword};
    if (!m_handler.completeImportPrivateKey(request, &completion, &error)) {
        m_importKeyError->setText(error);
        m_importKeyError->setVisible(true);
        return;
    }

    handleWalletCompletion(completion);
}

// ── Select Hardware Type ─────────────────────────────────────────

QWidget* SetupPage::buildSelectHardwareType() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 40);
    layout->setSpacing(20);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SetupCards); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title
    QLabel* title = new QLabel(tr("Connect Hardware Wallet"));
    title->setObjectName("newTxTitle");
    layout->addWidget(title);

    layout->addSpacing(10);

    // Card grid — 3 columns
    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(60, 0, 60, 0);
    grid->setSpacing(40);

    // Ledger card
    QWidget* ledgerCard = createActionCard(":/icons/wallet/ledger-logo.png", QColor(59, 130, 246),
                                           QString(), QString(), 240);
    ledgerCard->setProperty("cardId", "selectLedger");
    grid->addWidget(ledgerCard, 0, 0);

    // Trezor card
    QWidget* trezorCard = createActionCard(":/icons/wallet/trezor-logo.png", QColor(134, 194, 50),
                                           QString(), QString(), 240);
    trezorCard->setProperty("cardId", "selectTrezor");
    grid->addWidget(trezorCard, 0, 1);

    // Lattice1 card
    QWidget* latticeCard = createActionCard(":/icons/wallet/lattice1-logo.png",
                                            QColor(255, 107, 53), QString(), QString(), 240);
    latticeCard->setProperty("cardId", "selectLattice");
    grid->addWidget(latticeCard, 0, 2);

    layout->addLayout(grid);
    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ── Ledger Detect ───────────────────────────────────────────────

QWidget* SetupPage::buildLedgerDetect() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 40);
    layout->setSpacing(0);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SelectHardwareType); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    layout->addSpacing(20);

    // ── Hero device image (centered) ────────────────────────────
    QLabel* deviceImage = new QLabel();
    QPixmap devicePix(":/icons/wallet/ledger-device.png");
    deviceImage->setPixmap(devicePix.scaledToHeight(280, Qt::SmoothTransformation));
    deviceImage->setAlignment(Qt::AlignCenter);
    deviceImage->setProperty("uiClass", "setupTransparentBox");
    layout->addWidget(deviceImage, 0, Qt::AlignCenter);

    layout->addSpacing(24);

    // ── Title ───────────────────────────────────────────────────
    QLabel* title = new QLabel(tr("Connect Your Ledger"));
    title->setAlignment(Qt::AlignCenter);
    title->setProperty("uiClass", "setupHardwareHeroTitle");
    layout->addWidget(title);

    layout->addSpacing(6);

    // ── Tagline ─────────────────────────────────────────────────
    QLabel* tagline = new QLabel(tr("Connect and unlock your device to get started"));
    tagline->setAlignment(Qt::AlignCenter);
    tagline->setProperty("uiClass", "setupHardwareTagline");
    layout->addWidget(tagline);

    layout->addSpacing(32);

    // ── Numbered steps ──────────────────────────────────────────
    // Centered steps container
    QWidget* stepsContainer = new QWidget();
    stepsContainer->setProperty("uiClass", "setupTransparentBox");
    stepsContainer->setFixedWidth(380);
    QVBoxLayout* stepsLayout = new QVBoxLayout(stepsContainer);
    stepsLayout->setContentsMargins(0, 0, 0, 0);
    stepsLayout->setSpacing(12);
    stepsLayout->addWidget(createHardwareStepRow("ledger", 1, tr("Plug in your Ledger device")));
    stepsLayout->addWidget(createHardwareStepRow("ledger", 2, tr("Unlock it with your PIN")));
    stepsLayout->addWidget(createHardwareStepRow("ledger", 3, tr("Open the Solana app")));
    layout->addWidget(stepsContainer, 0, Qt::AlignCenter);

    layout->addSpacing(28);

    // ── Status section (centered) ───────────────────────────────
    m_ledgerStatusLabel = new QLabel(tr("Searching for Ledger devices..."));
    m_ledgerStatusLabel->setAlignment(Qt::AlignCenter);
    m_ledgerStatusLabel->setProperty("uiClass", "setupHardwareStatus");
    m_ledgerStatusLabel->setProperty("statusType", "neutral");
    layout->addWidget(m_ledgerStatusLabel);

    m_ledgerDeviceLabel = new QLabel();
    m_ledgerDeviceLabel->setAlignment(Qt::AlignCenter);
    m_ledgerDeviceLabel->setProperty("uiClass", "setupHardwareDeviceLabel");
    m_ledgerDeviceLabel->setVisible(false);
    layout->addWidget(m_ledgerDeviceLabel);

    layout->addSpacing(8);

    // ── Advanced toggle (centered) ──────────────────────────────
    QPushButton* advToggle = new QPushButton(tr("Advanced"));
    advToggle->setCursor(Qt::PointingHandCursor);
    advToggle->setFlat(true);
    advToggle->setProperty("uiClass", "setupHardwareAdvanced");
    layout->addWidget(advToggle, 0, Qt::AlignCenter);

    QWidget* advContainer = new QWidget();
    advContainer->setVisible(false);
    advContainer->setProperty("uiClass", "setupTransparentBox");
    QHBoxLayout* advLayout = new QHBoxLayout(advContainer);
    advLayout->setContentsMargins(0, 4, 0, 0);
    QLabel* derivLabel = new QLabel(tr("Derivation Path:"));
    derivLabel->setProperty("uiClass", "setupHardwareAdvancedLabel");
    advLayout->addWidget(derivLabel);
    m_derivationPathInput = new QLineEdit(QStringLiteral("m/44'/501'/0'/0'"));
    m_derivationPathInput->setFixedWidth(200);
    m_derivationPathInput->setProperty("uiClass", "setupHardwarePathInput");
    advLayout->addWidget(m_derivationPathInput);
    advLayout->addStretch();
    layout->addWidget(advContainer, 0, Qt::AlignCenter);

    connect(advToggle, &QPushButton::clicked, this,
            [advContainer]() { advContainer->setVisible(!advContainer->isVisible()); });

    layout->addSpacing(20);

    // ── Connect button (centered) ───────────────────────────────
    m_ledgerConnectBtn = new QPushButton(tr("Connect"));
    m_ledgerConnectBtn->setCursor(Qt::PointingHandCursor);
    m_ledgerConnectBtn->setFixedHeight(54);
    m_ledgerConnectBtn->setFixedWidth(300);
    m_ledgerConnectBtn->setEnabled(false);
    m_ledgerConnectBtn->setStyleSheet(
        Theme::primaryBtnStyle +
        QStringLiteral("QPushButton:disabled {"
                       "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                       "    stop:0 #2a1a4a, stop:0.5 #252050, stop:1 #1a2545);"
                       "  color: rgba(255, 255, 255, 0.35); border: 1px solid transparent;"
                       "}"));
    connect(m_ledgerConnectBtn, &QPushButton::clicked, this, [this]() {
        m_ledgerConnectBtn->setEnabled(false);
        m_ledgerConnectBtn->setText(tr("Connecting..."));
        emit ledgerConnectionRequested(m_ledgerDeviceId, m_derivationPathInput->text());
    });
    layout->addWidget(m_ledgerConnectBtn, 0, Qt::AlignCenter);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

void SetupPage::setAvailableLedgerDevices(const QList<HWDeviceInfo>& devices) {
    if (!m_ledgerStatusLabel) {
        return;
    }

    if (devices.isEmpty()) {
        m_ledgerStatusLabel->setText(tr("Searching for Ledger devices..."));
        m_ledgerStatusLabel->setProperty("statusType", "neutral");
        refreshWidgetStyle(m_ledgerStatusLabel);
        m_ledgerDeviceLabel->setVisible(false);
        m_ledgerConnectBtn->setEnabled(false);
        m_ledgerDeviceId.clear();
    } else {
        const auto& dev = devices.first();
        m_ledgerDeviceId = dev.deviceId;
        m_ledgerStatusLabel->setText(tr("Device found!"));
        m_ledgerStatusLabel->setProperty("statusType", "success");
        refreshWidgetStyle(m_ledgerStatusLabel);
        m_ledgerDeviceLabel->setText(dev.displayName);
        m_ledgerDeviceLabel->setVisible(true);
        m_ledgerConnectBtn->setEnabled(true);
        m_ledgerConnectBtn->setText(tr("Connect"));
    }
}

void SetupPage::onLedgerAddressReceived(const QString& address, const QByteArray& pubkey) {
    m_ledgerAddress = address;
    m_ledgerPubkey = pubkey;
    finalizeLedgerWallet();
}

void SetupPage::onLedgerConnectionFailed(const QString& error) {
    m_ledgerStatusLabel->setText(error);
    m_ledgerStatusLabel->setProperty("statusType", "error");
    refreshWidgetStyle(m_ledgerStatusLabel);
    m_ledgerConnectBtn->setEnabled(true);
    m_ledgerConnectBtn->setText(tr("Retry"));
}

void SetupPage::finalizeLedgerWallet() {
    SetupHandler::WalletCompletion completion;
    QString error;
    const SetupHandler::HardwareWalletRequest request{
        m_ledgerPubkey,
        m_ledgerAddress,
        m_pendingPassword,
        m_derivationPathInput ? m_derivationPathInput->text() : QStringLiteral("m/44'/501'/0'/0'"),
        WalletKeyType::Ledger,
    };
    if (!m_handler.completeHardwareWallet(request, &completion, &error)) {
        m_ledgerStatusLabel->setText(error);
        m_ledgerStatusLabel->setProperty("statusType", "error");
        refreshWidgetStyle(m_ledgerStatusLabel);
        return;
    }

    handleWalletCompletion(completion);
}

// ── Trezor Detect ───────────────────────────────────────────────

QWidget* SetupPage::buildTrezorDetect() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 40);
    layout->setSpacing(0);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SelectHardwareType); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    layout->addSpacing(20);

    // ── Hero device image (centered) ────────────────────────────
    QLabel* deviceImage = new QLabel();
    QPixmap devicePix(":/icons/wallet/trezor-device.png");
    deviceImage->setPixmap(devicePix.scaledToHeight(280, Qt::SmoothTransformation));
    deviceImage->setAlignment(Qt::AlignCenter);
    deviceImage->setProperty("uiClass", "setupTransparentBox");
    layout->addWidget(deviceImage, 0, Qt::AlignCenter);

    layout->addSpacing(24);

    // ── Title ───────────────────────────────────────────────────
    QLabel* title = new QLabel(tr("Connect Your Trezor"));
    title->setAlignment(Qt::AlignCenter);
    title->setProperty("uiClass", "setupHardwareHeroTitle");
    layout->addWidget(title);

    layout->addSpacing(6);

    // ── Tagline ─────────────────────────────────────────────────
    QLabel* tagline = new QLabel(tr("Connect and unlock your device to get started"));
    tagline->setAlignment(Qt::AlignCenter);
    tagline->setProperty("uiClass", "setupHardwareTagline");
    layout->addWidget(tagline);

    layout->addSpacing(32);

    // ── Numbered steps ──────────────────────────────────────────
    QWidget* stepsContainer = new QWidget();
    stepsContainer->setProperty("uiClass", "setupTransparentBox");
    stepsContainer->setFixedWidth(380);
    QVBoxLayout* stepsLayout = new QVBoxLayout(stepsContainer);
    stepsLayout->setContentsMargins(0, 0, 0, 0);
    stepsLayout->setSpacing(12);
    stepsLayout->addWidget(createHardwareStepRow("trezor", 1, tr("Plug in your Trezor device")));
    stepsLayout->addWidget(createHardwareStepRow("trezor", 2, tr("Unlock it with your PIN")));
    stepsLayout->addWidget(createHardwareStepRow("trezor", 3, tr("Confirm on the device")));
    layout->addWidget(stepsContainer, 0, Qt::AlignCenter);

    layout->addSpacing(28);

    // ── Status section (centered) ───────────────────────────────
    m_trezorStatusLabel = new QLabel(tr("Searching for Trezor devices..."));
    m_trezorStatusLabel->setAlignment(Qt::AlignCenter);
    m_trezorStatusLabel->setProperty("uiClass", "setupHardwareStatus");
    m_trezorStatusLabel->setProperty("statusType", "neutral");
    layout->addWidget(m_trezorStatusLabel);

    m_trezorDeviceLabel = new QLabel();
    m_trezorDeviceLabel->setAlignment(Qt::AlignCenter);
    m_trezorDeviceLabel->setProperty("uiClass", "setupHardwareDeviceLabel");
    m_trezorDeviceLabel->setVisible(false);
    layout->addWidget(m_trezorDeviceLabel);

    layout->addSpacing(8);

    // ── Advanced toggle (centered) ──────────────────────────────
    QPushButton* advToggle = new QPushButton(tr("Advanced"));
    advToggle->setCursor(Qt::PointingHandCursor);
    advToggle->setFlat(true);
    advToggle->setProperty("uiClass", "setupHardwareAdvanced");
    layout->addWidget(advToggle, 0, Qt::AlignCenter);

    QWidget* advContainer = new QWidget();
    advContainer->setVisible(false);
    advContainer->setProperty("uiClass", "setupTransparentBox");
    QHBoxLayout* advLayout = new QHBoxLayout(advContainer);
    advLayout->setContentsMargins(0, 4, 0, 0);
    QLabel* derivLabel = new QLabel(tr("Derivation Path:"));
    derivLabel->setProperty("uiClass", "setupHardwareAdvancedLabel");
    advLayout->addWidget(derivLabel);
    m_trezorDerivationPathInput = new QLineEdit(QStringLiteral("m/44'/501'/0'/0'"));
    m_trezorDerivationPathInput->setFixedWidth(200);
    m_trezorDerivationPathInput->setProperty("uiClass", "setupHardwarePathInput");
    advLayout->addWidget(m_trezorDerivationPathInput);
    advLayout->addStretch();
    layout->addWidget(advContainer, 0, Qt::AlignCenter);

    connect(advToggle, &QPushButton::clicked, this,
            [advContainer]() { advContainer->setVisible(!advContainer->isVisible()); });

    layout->addSpacing(20);

    // ── Connect button (centered) ───────────────────────────────
    m_trezorConnectBtn = new QPushButton(tr("Connect"));
    m_trezorConnectBtn->setCursor(Qt::PointingHandCursor);
    m_trezorConnectBtn->setFixedHeight(54);
    m_trezorConnectBtn->setFixedWidth(300);
    m_trezorConnectBtn->setEnabled(false);
    m_trezorConnectBtn->setStyleSheet(
        Theme::primaryBtnStyle +
        QStringLiteral("QPushButton:disabled {"
                       "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                       "    stop:0 #2a1a4a, stop:0.5 #252050, stop:1 #1a2545);"
                       "  color: rgba(255, 255, 255, 0.35); border: 1px solid transparent;"
                       "}"));
    connect(m_trezorConnectBtn, &QPushButton::clicked, this, [this]() {
        m_trezorConnectBtn->setEnabled(false);
        m_trezorConnectBtn->setText(tr("Connecting..."));
        emit trezorConnectionRequested(m_trezorDeviceId, m_trezorDerivationPathInput->text());
    });
    layout->addWidget(m_trezorConnectBtn, 0, Qt::AlignCenter);

    layout->addStretch();
    scroll->setWidget(content);
    return scroll;
}

void SetupPage::setAvailableTrezorDevices(const QList<HWDeviceInfo>& devices) {
    if (!m_trezorStatusLabel) {
        return;
    }

    if (devices.isEmpty()) {
        m_trezorStatusLabel->setText(tr("Searching for Trezor devices..."));
        m_trezorStatusLabel->setProperty("statusType", "neutral");
        refreshWidgetStyle(m_trezorStatusLabel);
        m_trezorDeviceLabel->setVisible(false);
        m_trezorConnectBtn->setEnabled(false);
        m_trezorDeviceId.clear();
    } else {
        const auto& dev = devices.first();
        m_trezorDeviceId = dev.deviceId;
        m_trezorStatusLabel->setText(tr("Device found!"));
        m_trezorStatusLabel->setProperty("statusType", "success");
        refreshWidgetStyle(m_trezorStatusLabel);
        m_trezorDeviceLabel->setText(dev.displayName);
        m_trezorDeviceLabel->setVisible(true);
        m_trezorConnectBtn->setEnabled(true);
        m_trezorConnectBtn->setText(tr("Connect"));
    }
}

void SetupPage::onTrezorAddressReceived(const QString& address, const QByteArray& pubkey) {
    m_trezorAddress = address;
    m_trezorPubkey = pubkey;
    finalizeTrezorWallet();
}

void SetupPage::onTrezorConnectionFailed(const QString& error) {
    m_trezorStatusLabel->setText(error);
    m_trezorStatusLabel->setProperty("statusType", "error");
    refreshWidgetStyle(m_trezorStatusLabel);
    m_trezorConnectBtn->setEnabled(true);
    m_trezorConnectBtn->setText(tr("Retry"));
}

void SetupPage::finalizeTrezorWallet() {
    SetupHandler::WalletCompletion completion;
    QString error;
    const SetupHandler::HardwareWalletRequest request{
        m_trezorPubkey,
        m_trezorAddress,
        m_pendingPassword,
        m_trezorDerivationPathInput ? m_trezorDerivationPathInput->text()
                                    : QStringLiteral("m/44'/501'/0'/0'"),
        WalletKeyType::Trezor,
    };
    if (!m_handler.completeHardwareWallet(request, &completion, &error)) {
        m_trezorStatusLabel->setText(error);
        m_trezorStatusLabel->setProperty("statusType", "error");
        refreshWidgetStyle(m_trezorStatusLabel);
        return;
    }

    handleWalletCompletion(completion);
}

// ── Success Page (index 6) ───────────────────────────────────────

QWidget* SetupPage::buildSuccessPage() {
    // Outer container — confetti pieces are absolutely positioned here
    QWidget* page = new QWidget();
    page->setObjectName("successPage");
    page->setProperty("uiClass", "setupSuccessPage");
    page->installEventFilter(this);

    // Central content area (transparent so confetti shows through behind it)
    QWidget* content = new QWidget(page);
    content->setObjectName("successContent");
    content->setAttribute(Qt::WA_TranslucentBackground);
    content->setProperty("uiClass", "setupTransparentBox");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 20, 48, 40);
    layout->setSpacing(0);

    layout->addStretch(1);

    // Title with glow
    QLabel* title = new QLabel();
    title->setTextFormat(Qt::RichText);
    title->setText(QString("<span style='color: white; font-size: 42px; font-weight: bold;'>"
                           "Wallet Created </span>"
                           "<span style='color: #14F195; font-size: 42px; font-weight: bold;'>"
                           "Successfully</span>"));
    title->setAlignment(Qt::AlignCenter);
    title->setProperty("uiClass", "setupSuccessTitle");

    QGraphicsDropShadowEffect* glow = new QGraphicsDropShadowEffect();
    glow->setBlurRadius(50);
    glow->setColor(QColor(20, 241, 149, 160));
    glow->setOffset(0, 0);
    title->setGraphicsEffect(glow);
    layout->addWidget(title);
    layout->addSpacing(12);

    // Description
    QLabel* desc = new QLabel(tr("Your wallet has been securely encrypted and saved."));
    desc->setProperty("uiClass", "setupSuccessDesc");
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    layout->addWidget(desc);
    layout->addSpacing(60);

    // Address + button in a centered container
    QWidget* centerBox = new QWidget();
    centerBox->setMaximumWidth(560);
    centerBox->setProperty("uiClass", "setupTransparentBox");
    QVBoxLayout* centerLayout = new QVBoxLayout(centerBox);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    QLabel* addrLabel = new QLabel(tr("Your Address"));
    addrLabel->setProperty("uiClass", "setupSuccessAddressLabel");
    addrLabel->setAlignment(Qt::AlignCenter);
    centerLayout->addWidget(addrLabel);
    centerLayout->addSpacing(8);

    // Address row
    QWidget* addrRow = new QWidget();
    addrRow->setProperty("uiClass", "setupSuccessAddressRow");
    QHBoxLayout* addrRowLayout = new QHBoxLayout(addrRow);
    addrRowLayout->setContentsMargins(18, 14, 10, 14);
    addrRowLayout->setSpacing(10);

    m_successAddress = new AddressLink({});
    addrRowLayout->addWidget(m_successAddress, 1);

    centerLayout->addWidget(addrRow);
    centerLayout->addSpacing(24);

    QPushButton* doneBtn = new QPushButton(tr("Done"));
    doneBtn->setCursor(Qt::PointingHandCursor);
    doneBtn->setFixedHeight(54);
    doneBtn->setStyleSheet(Theme::primaryBtnStyle);
    connect(doneBtn, &QPushButton::clicked, this, [this]() {
        UnlockResult result;
        result.address = m_successAddress->address();

        if (m_completedWalletType == WalletKeyType::Ledger) {
            result.publicKey = m_ledgerPubkey;
            result.walletType = WalletKeyType::Ledger;
            result.hardwarePlugin = HardwarePluginId::Ledger;
            result.derivationPath = m_derivationPathInput ? m_derivationPathInput->text()
                                                          : QStringLiteral("m/44'/501'/0'/0'");
        } else if (m_completedWalletType == WalletKeyType::Trezor) {
            result.publicKey = m_trezorPubkey;
            result.walletType = WalletKeyType::Trezor;
            result.hardwarePlugin = HardwarePluginId::Trezor;
            result.derivationPath = m_trezorDerivationPathInput
                                        ? m_trezorDerivationPathInput->text()
                                        : QStringLiteral("m/44'/501'/0'/0'");
        } else {
            result.publicKey = m_createdKeypair.publicKey();
            result.keypair = m_createdKeypair;
            result.walletType = m_completedWalletType;
        }

        // Pass session password for account switching (wiped by CinderWalletApp on lock)
        result.password = m_pendingPassword;
        sodium_memzero(m_pendingPassword.data(),
                       static_cast<size_t>(m_pendingPassword.size()) * sizeof(QChar));
        m_pendingPassword.clear();

        // Restore grid for future use if we were in import mode
        if (m_importMode) {
            m_cardGrid->removeWidget(m_importRecoveryCard);
            m_cardGrid->removeWidget(m_importKeyCard);
            m_cardGrid->removeWidget(m_hardwareCard);
            m_createCard->show();
            m_cardGrid->addWidget(m_createCard, 0, 0);
            m_cardGrid->addWidget(m_importRecoveryCard, 0, 1);
            m_cardGrid->addWidget(m_importKeyCard, 1, 0);
            m_cardGrid->addWidget(m_hardwareCard, 1, 1);
            m_importMode = false;
        }

        emit walletCreated(result);
    });
    centerLayout->addWidget(doneBtn);

    layout->addWidget(centerBox, 0, Qt::AlignHCenter);

    layout->addStretch(2);

    return page;
}

void SetupPage::showSuccess(const QString& address) {
    m_successAddress->setAddress(address);
    m_confettiPending = true;
    showStep(Step::Success);
    // Confetti is triggered by the event filter once the page has valid geometry
}

void SetupPage::handleWalletCompletion(const SetupHandler::WalletCompletion& completion) {
    m_createdKeypair = completion.keypair;
    m_completedWalletType = completion.type;
    showSuccess(completion.address);
}

void SetupPage::spawnConfetti() {
    QWidget* page = m_stack->widget(static_cast<int>(Step::Success));
    int pageW = page->width();
    int pageH = page->height();

    // Content overlay needs to fill the page
    QList<QWidget*> directKids =
        page->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    QWidget* content = nullptr;
    for (QWidget* kid : directKids) {
        if (kid->objectName() != "confettiPiece") {
            content = kid;
            break;
        }
    }
    if (content) {
        content->setGeometry(page->rect());
    }

    // Remove old confetti
    for (QLabel* old : page->findChildren<QLabel*>("confettiPiece")) {
        old->deleteLater();
    }

    // Load confetti pixmaps
    QList<QPixmap> pieces;
    for (int i = 1; i <= CONFETTI_PIECE_COUNT; ++i) {
        QPixmap pix(QString(":/icons/confetti/piece_%1.png").arg(i, 2, 10, QChar('0')));
        if (!pix.isNull()) {
            pieces.append(pix);
        }
    }
    if (pieces.isEmpty() || pageW < 50 || pageH < 50) {
        return;
    }

    auto* rng = QRandomGenerator::global();
    int spawnCount = 45;

    // Exclusion zone: avoid the center where text content lives
    int exclL = pageW * 0.20; // left edge of exclusion
    int exclR = pageW * 0.80; // right edge
    int exclT = pageH * 0.15; // top edge
    int exclB = pageH * 0.70; // bottom edge

    for (int i = 0; i < spawnCount; ++i) {
        const QPixmap& pix = pieces[rng->bounded(pieces.size())];

        qreal scale = 0.15 + rng->generateDouble() * 0.35;
        int w = qMax(8, static_cast<int>(pix.width() * scale));
        int h = qMax(8, static_cast<int>(pix.height() * scale));

        QLabel* piece = new QLabel(page);
        piece->setObjectName("confettiPiece");
        piece->setPixmap(pix.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        piece->setFixedSize(w, h);
        piece->setStyleSheet("background: transparent; border: none;");
        piece->setAttribute(Qt::WA_TransparentForMouseEvents);

        // Generate position outside the exclusion zone
        int startX, endY;
        int attempts = 0;
        do {
            startX = rng->bounded(qMax(1, pageW - w));
            endY = rng->bounded(qMax(1, pageH - h));
            attempts++;
        } while (attempts < 20 && startX + w > exclL && startX < exclR && endY + h > exclT &&
                 endY < exclB);

        int startY = -h - rng->bounded(200);

        piece->move(startX, startY);
        piece->show();

        QPropertyAnimation* anim = new QPropertyAnimation(piece, "pos", piece);
        anim->setStartValue(QPoint(startX, startY));
        anim->setEndValue(QPoint(startX, endY));
        anim->setDuration(1800 + rng->bounded(1500)); // 1.8–3.3s fall
        anim->setEasingCurve(QEasingCurve::OutBounce);

        int delay = rng->bounded(1000); // stagger over 1s
        QTimer::singleShot(delay, piece, [anim]() { anim->start(); });
    }

    // Raise content above confetti
    if (content) {
        content->raise();
    }
}

// ── Card Helpers (same pattern as SendReceivePage) ──────────────

QWidget* SetupPage::createActionCard(const QString& iconPath, const QColor& accent,
                                     const QString& title, const QString& subtitle, int iconSize) {
    QWidget* card = new QWidget();
    card->setObjectName("txCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setMinimumHeight(280);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    card->setProperty("accentColor", accent);
    applyCardStyle(card, accent, 0.40);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(10, 15, 10, 20);
    cardLayout->setAlignment(Qt::AlignHCenter);

    cardLayout->addStretch();

    QPixmap pixmap(iconPath);
    if (!pixmap.isNull()) {
        qreal dpr = qApp->devicePixelRatio();
        pixmap = pixmap.scaled(iconSize * dpr, iconSize * dpr, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(dpr);
    }
    QLabel* iconLabel = new QLabel();
    iconLabel->setPixmap(pixmap);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    cardLayout->addWidget(iconLabel, 0, Qt::AlignHCenter);

    if (!title.isEmpty() || !subtitle.isEmpty()) {
        cardLayout->addSpacing(12);

        QLabel* titleLabel = new QLabel(title);
        titleLabel->setObjectName("txCardTitle");
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        cardLayout->addWidget(titleLabel);

        QLabel* subtitleLabel = new QLabel(subtitle);
        subtitleLabel->setObjectName("txCardSubtitle");
        subtitleLabel->setAlignment(Qt::AlignCenter);
        subtitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        cardLayout->addWidget(subtitleLabel);
    }

    cardLayout->addStretch();

    card->installEventFilter(this);

    return card;
}

QWidget* SetupPage::buildLatticeDetect() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("walletContent");
    content->setProperty("uiClass", "content");

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(60, 40, 60, 40);
    layout->setSpacing(20);

    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::SelectHardwareType); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Device image
    QLabel* deviceImg = new QLabel();
    QPixmap latticePixmap(":/icons/wallet/lattice1-device.png");
    if (!latticePixmap.isNull()) {
        latticePixmap = latticePixmap.scaledToHeight(160, Qt::SmoothTransformation);
        deviceImg->setPixmap(latticePixmap);
    }
    deviceImg->setAlignment(Qt::AlignCenter);
    deviceImg->setProperty("uiClass", "setupTransparentBox");
    layout->addWidget(deviceImg, 0, Qt::AlignCenter);

    QLabel* title = new QLabel(tr("Connect Your Lattice1"));
    title->setObjectName("newTxTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    QLabel* desc = new QLabel(tr("Enter your Lattice1 Device ID from the Lattice Manager app, "
                                 "then enter the pairing code shown on your device screen."));
    desc->setObjectName("srSubtleDesc14");
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    layout->addWidget(desc);

    layout->addSpacing(10);

    // Device ID input
    QLabel* deviceIdLabel = new QLabel(tr("Device ID"));
    deviceIdLabel->setObjectName("txFormLabel");
    layout->addWidget(deviceIdLabel);

    m_latticeDeviceIdInput = new StyledLineEdit();
    m_latticeDeviceIdInput->setPlaceholderText(tr("Enter your Lattice1 Device ID"));
    m_latticeDeviceIdInput->setMinimumHeight(44);
    {
        QPalette pal = m_latticeDeviceIdInput->palette();
        pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
        m_latticeDeviceIdInput->setPalette(pal);
    }
    layout->addWidget(m_latticeDeviceIdInput);

    // Pairing code input (initially hidden)
    m_latticePairingContainer = new QWidget();
    QVBoxLayout* pairLayout = new QVBoxLayout(m_latticePairingContainer);
    pairLayout->setContentsMargins(0, 0, 0, 0);
    pairLayout->setSpacing(8);

    QLabel* pairLabel = new QLabel(tr("Pairing Code"));
    pairLabel->setObjectName("txFormLabel");
    pairLayout->addWidget(pairLabel);

    m_latticePairingInput = new StyledLineEdit();
    m_latticePairingInput->setPlaceholderText(tr("6-digit code from device screen"));
    m_latticePairingInput->setMinimumHeight(44);
    m_latticePairingInput->setMaxLength(6);
    {
        QPalette pal = m_latticePairingInput->palette();
        pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
        m_latticePairingInput->setPalette(pal);
    }
    pairLayout->addWidget(m_latticePairingInput);

    m_latticePairingContainer->setVisible(false);
    layout->addWidget(m_latticePairingContainer);

    // Status label
    m_latticeStatusLabel = new QLabel();
    m_latticeStatusLabel->setProperty("uiClass", "setupHardwareStatus");
    m_latticeStatusLabel->setProperty("statusType", "neutral");
    m_latticeStatusLabel->setVisible(false);
    m_latticeStatusLabel->setWordWrap(true);
    layout->addWidget(m_latticeStatusLabel);

    // Connect button
    m_latticeConnectBtn = new QPushButton(tr("Connect"));
    m_latticeConnectBtn->setCursor(Qt::PointingHandCursor);
    m_latticeConnectBtn->setMinimumHeight(48);
    m_latticeConnectBtn->setStyleSheet(Theme::primaryBtnStyle);
    layout->addWidget(m_latticeConnectBtn);

    connect(m_latticeConnectBtn, &QPushButton::clicked, this, [this]() {
        QString deviceId = m_latticeDeviceIdInput->text().trimmed();
        if (deviceId.isEmpty()) {
            m_latticeStatusLabel->setText(tr("Please enter a Device ID."));
            m_latticeStatusLabel->setProperty("statusType", "error");
            refreshWidgetStyle(m_latticeStatusLabel);
            m_latticeStatusLabel->setVisible(true);
            return;
        }

        if (m_latticePairingContainer->isVisible()) {
            // Submit pairing code
            QString code = m_latticePairingInput->text().trimmed();
            if (code.length() != 6) {
                m_latticeStatusLabel->setText(tr("Enter the 6-digit pairing code."));
                m_latticeStatusLabel->setProperty("statusType", "error");
                refreshWidgetStyle(m_latticeStatusLabel);
                m_latticeStatusLabel->setVisible(true);
                return;
            }
            emit latticePairingSubmitted(code);
        } else {
            // Connect to device
            m_latticeStatusLabel->setText(tr("Connecting..."));
            m_latticeStatusLabel->setProperty("statusType", "neutral");
            refreshWidgetStyle(m_latticeStatusLabel);
            m_latticeStatusLabel->setVisible(true);
            m_latticeConnectBtn->setEnabled(false);

            QString derivPath = QStringLiteral("m/44'/501'/0'/0'");
            emit latticeConnectionRequested(deviceId, derivPath);
        }
    });

    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

void SetupPage::applyCardStyle(QWidget* card, const QColor& accent, double borderOpacity) {
    card->setStyleSheet(QString("QWidget#txCard {"
                                "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                                "    stop:0 %1, stop:1 %2);"
                                "  border: 1px solid rgba(%3, %4, %5, %6);"
                                "  border-radius: %7px;"
                                "}"
                                "QWidget#txCard QLabel {"
                                "  background: transparent;"
                                "  border: none;"
                                "}")
                            .arg(Theme::cardBgStart)
                            .arg(Theme::cardBgEnd)
                            .arg(accent.red())
                            .arg(accent.green())
                            .arg(accent.blue())
                            .arg(borderOpacity, 0, 'f', 2)
                            .arg(Theme::cardRadius));
}

// ── Event Filter ────────────────────────────────────────────────

bool SetupPage::eventFilter(QObject* obj, QEvent* event) {
    // Handle success page events
    if (obj == m_stack->widget(static_cast<int>(Step::Success))) {
        if (event->type() == QEvent::Resize) {
            QWidget* page = static_cast<QWidget*>(obj);
            // Resize content overlay to fill page
            QList<QWidget*> kids =
                page->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
            for (QWidget* kid : kids) {
                if (kid->objectName() != "confettiPiece") {
                    kid->setGeometry(page->rect());
                    break;
                }
            }
            // Spawn confetti once page has real geometry
            if (m_confettiPending && page->width() > 100 && page->height() > 100) {
                m_confettiPending = false;
                QTimer::singleShot(0, this, [this]() { spawnConfetti(); });
            }
        }
    }

    QWidget* card = qobject_cast<QWidget*>(obj);
    if (!card || card->objectName() != "txCard") {
        return QWidget::eventFilter(obj, event);
    }

    QColor accent = card->property("accentColor").value<QColor>();
    if (!accent.isValid()) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::Enter) {
        applyCardStyle(card, accent, 0.65);
    } else if (event->type() == QEvent::Leave) {
        applyCardStyle(card, accent, 0.40);
    } else if (event->type() == QEvent::MouseButtonPress) {
        QString cardId = card->property("cardId").toString();
        if (cardId == "createWallet") {
            m_pendingWalletFlow = Step::CreateWallet;
            showStep(Step::SetPassword);
        } else if (cardId == "importWallet") {
            m_pendingWalletFlow = Step::ImportRecovery;
            showStep(Step::SetPassword);
        } else if (cardId == "importPrivateKey") {
            m_pendingWalletFlow = Step::ImportPrivateKey;
            showStep(Step::SetPassword);
        } else if (cardId == "connectHardware") {
            m_pendingWalletFlow = Step::SelectHardwareType;
            showStep(Step::SetPassword);
        } else if (cardId == "selectLedger") {
            showStep(Step::LedgerDetect);
            emit ledgerPageEntered();
        } else if (cardId == "selectTrezor") {
            showStep(Step::TrezorDetect);
            emit trezorPageEntered();
        } else if (cardId == "selectLattice") {
            showStep(Step::LatticeDetect);
        }
    }

    return QWidget::eventFilter(obj, event);
}
