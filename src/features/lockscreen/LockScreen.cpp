#include "LockScreen.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

namespace {

    void repolish(QWidget* widget) {
        if (!widget) {
            return;
        }
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

} // namespace

LockScreen::LockScreen(QWidget* parent) : QWidget(parent), m_handler(new LockScreenHandler(this)) {
    setObjectName("lockScreen");

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    // Center everything vertically and horizontally
    outer->addStretch(3);

    // ── Lock icon ──
    QLabel* icon = new QLabel();
    icon->setAlignment(Qt::AlignCenter);
    QPixmap lockPx(":/icons/lock.png");
    if (!lockPx.isNull()) {
        icon->setPixmap(lockPx.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    icon->setObjectName("lockIcon");
    outer->addWidget(icon);
    outer->addSpacing(0);

    // ── Title ──
    QLabel* title = new QLabel(tr("Wallet Locked"));
    title->setObjectName("lockTitle");
    title->setAlignment(Qt::AlignCenter);
    outer->addWidget(title);
    outer->addSpacing(8);

    // ── Subtitle ──
    QLabel* subtitle = new QLabel(tr("Enter your password to unlock your wallet"));
    subtitle->setObjectName("lockSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    outer->addWidget(subtitle);
    outer->addSpacing(32);

    // ── Password input card ──
    QWidget* card = new QWidget();
    card->setObjectName("lockCard");
    card->setFixedWidth(400);
    card->setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(16);

    // Password label
    QLabel* pwLabel = new QLabel(tr("Password"));
    pwLabel->setObjectName("lockPasswordLabel");
    cardLayout->addWidget(pwLabel);

    // Password input row: line edit + eye toggle in a styled container
    m_inputRow = new QWidget();
    QWidget* inputRow = m_inputRow;
    inputRow->setFixedHeight(48);
    inputRow->setObjectName("lockInputRow");
    inputRow->setProperty("focused", false);
    inputRow->setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 6, 0);
    inputLayout->setSpacing(0);

    m_passwordInput = new QLineEdit();
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setPlaceholderText(tr("Enter wallet password..."));
    m_passwordInput->setObjectName("lockPasswordInput");
    inputLayout->addWidget(m_passwordInput, 1);

    // Eye toggle button
    QPushButton* eyeBtn = new QPushButton();
    eyeBtn->setObjectName("lockEyeBtn");
    eyeBtn->setFixedSize(32, 32);
    eyeBtn->setCursor(Qt::PointingHandCursor);
    eyeBtn->setIcon(QIcon(":/icons/ui/eye-closed.svg"));
    eyeBtn->setIconSize(QSize(18, 18));
    eyeBtn->setToolTip(tr("Show password"));
    inputLayout->addWidget(eyeBtn);

    connect(eyeBtn, &QPushButton::clicked, this, [this, eyeBtn]() {
        if (m_passwordInput->echoMode() == QLineEdit::Password) {
            m_passwordInput->setEchoMode(QLineEdit::Normal);
            eyeBtn->setIcon(QIcon(":/icons/ui/eye-open.svg"));
            eyeBtn->setToolTip(tr("Hide password"));
        } else {
            m_passwordInput->setEchoMode(QLineEdit::Password);
            eyeBtn->setIcon(QIcon(":/icons/ui/eye-closed.svg"));
            eyeBtn->setToolTip(tr("Show password"));
        }
    });

    // Focus highlight: change container border when input is focused
    m_passwordInput->installEventFilter(this);

    cardLayout->addWidget(inputRow);

    // Error label (hidden by default)
    m_errorLabel = new QLabel();
    m_errorLabel->setObjectName("lockErrorLabel");
    m_errorLabel->setAlignment(Qt::AlignLeft);
    m_errorLabel->setVisible(false);
    cardLayout->addWidget(m_errorLabel);

    // Unlock button
    m_unlockBtn = new QPushButton(tr("Unlock Wallet"));
    m_unlockBtn->setObjectName("lockUnlockBtn");
    m_unlockBtn->setFixedHeight(48);
    m_unlockBtn->setCursor(Qt::PointingHandCursor);
    m_unlockBtn->setEnabled(false);
    cardLayout->addWidget(m_unlockBtn);

    connect(m_unlockBtn, &QPushButton::clicked, this,
            [this]() { m_handler->attemptUnlock(m_passwordInput->text()); });
    connect(m_passwordInput, &QLineEdit::returnPressed, this,
            [this]() { m_handler->attemptUnlock(m_passwordInput->text()); });
    connect(m_passwordInput, &QLineEdit::textChanged, this,
            [this](const QString& text) { m_unlockBtn->setEnabled(!text.isEmpty()); });

    // ── "or" separator ──
    m_orLabel = new QLabel(tr("or"));
    m_orLabel->setObjectName("lockOrLabel");
    m_orLabel->setAlignment(Qt::AlignCenter);
    m_orLabel->setVisible(false);
    cardLayout->addWidget(m_orLabel);

    // ── Touch ID button with fingerprint icon ──
    const int iconSize = 36;
    const qreal dpr = devicePixelRatioF();
    QSvgRenderer svgRenderer(QStringLiteral(":/icons/fingerprint.svg"));

    // Hover pixmap (full white — rendered directly from SVG)
    m_fpHover = QPixmap(QSize(iconSize, iconSize) * dpr);
    m_fpHover.setDevicePixelRatio(dpr);
    m_fpHover.fill(Qt::transparent);
    {
        QPainter p(&m_fpHover);
        svgRenderer.render(&p, QRectF(0, 0, iconSize, iconSize));
    }

    // Normal pixmap (dimmed — tint to rgba white 60%)
    m_fpNormal = QPixmap(m_fpHover.size());
    m_fpNormal.setDevicePixelRatio(dpr);
    m_fpNormal.fill(Qt::transparent);
    {
        QPainter p(&m_fpNormal);
        p.setOpacity(0.5);
        p.drawPixmap(0, 0, m_fpHover);
    }

    m_touchIdBtn = new QWidget();
    m_touchIdBtn->setObjectName("lockTouchIdBtn");
    m_touchIdBtn->setCursor(Qt::PointingHandCursor);
    m_touchIdBtn->setVisible(false);
    m_touchIdBtn->setAttribute(Qt::WA_StyledBackground, true);

    auto* touchIdLayout = new QVBoxLayout(m_touchIdBtn);
    touchIdLayout->setContentsMargins(0, 16, 0, 16);
    touchIdLayout->setSpacing(8);
    touchIdLayout->setAlignment(Qt::AlignCenter);

    m_touchIdIcon = new QLabel();
    m_touchIdIcon->setPixmap(m_fpNormal);
    m_touchIdIcon->setAlignment(Qt::AlignCenter);
    touchIdLayout->addWidget(m_touchIdIcon);

    m_touchIdLabel = new QLabel(tr("Unlock with Touch ID"));
    m_touchIdLabel->setAlignment(Qt::AlignCenter);
    m_touchIdLabel->setProperty("uiClass", "lockscreenTouchIdLabel");
    m_touchIdLabel->setProperty("hovered", false);
    touchIdLayout->addWidget(m_touchIdLabel);

    cardLayout->addWidget(m_touchIdBtn);

    m_touchIdBtn->installEventFilter(this);

    connect(m_handler, &LockScreenHandler::unlockStarted, this, [this]() {
        m_unlockBtn->setEnabled(false);
        m_unlockBtn->setText(tr("Unlocking..."));
        m_passwordInput->setEnabled(false);
        m_touchIdBtn->setEnabled(false);
        m_errorLabel->setVisible(false);
    });

    connect(m_handler, &LockScreenHandler::unlockFailed, this,
            [this](LockScreenHandler::UnlockError error) {
                m_errorLabel->setText(unlockErrorMessage(error));
                m_errorLabel->setVisible(true);
                m_passwordInput->setEnabled(true);
                m_passwordInput->selectAll();
                m_passwordInput->setFocus();
            });

    connect(m_handler, &LockScreenHandler::unlockSucceeded, this,
            [this](const UnlockResult& result) {
                m_passwordInput->clear();
                emit unlocked(result);
            });

    connect(m_handler, &LockScreenHandler::unlockFlowFinished, this, [this]() {
        m_passwordInput->setEnabled(true);
        m_touchIdBtn->setEnabled(true);
        m_unlockBtn->setText(tr("Unlock Wallet"));
        m_unlockBtn->setEnabled(!m_passwordInput->text().isEmpty());
    });

    connect(m_handler, &LockScreenHandler::biometricStateChanged, this, [this](bool available) {
        m_touchIdBtn->setVisible(available);
        m_orLabel->setVisible(available);
    });

    // Center the card horizontally
    QHBoxLayout* cardRow = new QHBoxLayout();
    cardRow->addStretch();
    cardRow->addWidget(card);
    cardRow->addStretch();
    outer->addLayout(cardRow);

    outer->addStretch(4);

    // ── Footer hint ──
    QLabel* footer = new QLabel(tr("This is the password you set when creating your wallet."));
    footer->setObjectName("lockFooter");
    footer->setAlignment(Qt::AlignCenter);
    outer->addWidget(footer);
    outer->addSpacing(24);
}

void LockScreen::refreshBiometricState() { m_handler->refreshBiometricState(); }

QString LockScreen::unlockErrorMessage(LockScreenHandler::UnlockError error) const {
    switch (error) {
        case LockScreenHandler::UnlockError::EmptyPassword:
            return tr("Please enter your password.");
        case LockScreenHandler::UnlockError::NoWallet:
            return tr("No wallet found.");
        case LockScreenHandler::UnlockError::WalletNotFound:
            return tr("Wallet not found.");
        case LockScreenHandler::UnlockError::IncorrectPassword:
            return tr("Incorrect password. Please try again.");
    }

    return QString();
}

bool LockScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_passwordInput) {
        if (event->type() == QEvent::FocusIn) {
            m_inputRow->setProperty("focused", true);
            m_inputRow->style()->unpolish(m_inputRow);
            m_inputRow->style()->polish(m_inputRow);
        } else if (event->type() == QEvent::FocusOut) {
            m_inputRow->setProperty("focused", false);
            m_inputRow->style()->unpolish(m_inputRow);
            m_inputRow->style()->polish(m_inputRow);
        }
    }
    if (obj == m_touchIdBtn) {
        if (event->type() == QEvent::MouseButtonRelease && m_touchIdBtn->isEnabled()) {
            m_handler->attemptBiometricUnlock();
        } else if (event->type() == QEvent::Enter && m_touchIdBtn->isEnabled()) {
            m_touchIdIcon->setPixmap(m_fpHover);
            m_touchIdLabel->setProperty("hovered", true);
            repolish(m_touchIdLabel);
        } else if (event->type() == QEvent::Leave) {
            m_touchIdIcon->setPixmap(m_fpNormal);
            m_touchIdLabel->setProperty("hovered", false);
            repolish(m_touchIdLabel);
        }
    }
    return QWidget::eventFilter(obj, event);
}
