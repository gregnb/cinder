#include "AddressLink.h"
#include "services/AvatarCache.h"
#include "util/CopyButton.h"
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QToolTip>

static const QString kDefaultStyle = "color: #10b981; font-size: 13px;"
                                     "font-family: 'JetBrains Mono', 'SF Mono', 'Menlo';"
                                     "background: transparent; border: none;";

static const QString kHoverStyle = "color: #fde68a; font-size: 13px;"
                                   "font-family: 'JetBrains Mono', 'SF Mono', 'Menlo';"
                                   "background: transparent; border: none;";

static const QString kCopyBtnStyle = "QPushButton { background: rgba(255,255,255,0.06);"
                                     "  border: none; border-radius: 6px; padding: 0; }"
                                     "QPushButton:hover { background: rgba(255,255,255,0.15); }";

// Static registry
QHash<QString, QList<AddressLink*>> AddressLink::s_registry;

AddressLink::AddressLink(const QString& address, QWidget* parent) : QWidget(parent) {
    QHBoxLayout* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);

    // Avatar (hidden by default, shown when setContactInfo is called)
    m_avatarLabel = new QLabel();
    m_avatarLabel->setFixedSize(16, 16);
    m_avatarLabel->setStyleSheet("background: transparent; border: none;");
    m_avatarLabel->hide();
    h->addWidget(m_avatarLabel);

    m_label = new QLabel();
    m_label->setStyleSheet(kDefaultStyle);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_label->setWordWrap(true);
    h->addWidget(m_label);

    m_copyBtn = new QPushButton();
    m_copyBtn->setFixedSize(26, 26);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setStyleSheet(kCopyBtnStyle);
    CopyButton::applyIcon(m_copyBtn);
    // Wire is deferred — address may change via setAddress(), so use lambda
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_address);
        m_copyBtn->setIcon(QIcon(CopyButton::renderIcon(":/icons/ui/checkmark.svg", 14)));
        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(1500);
        connect(timer, &QTimer::timeout, this, [this, timer]() {
            CopyButton::applyIcon(m_copyBtn);
            timer->deleteLater();
        });
        timer->start();
    });
    h->addWidget(m_copyBtn);
    h->addStretch();

    setAddress(address);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

AddressLink::~AddressLink() { unregisterLink(); }

void AddressLink::setAddress(const QString& address) {
    unregisterLink();
    m_address = address;
    updateDisplayText();
    registerLink();
}

void AddressLink::setMaxDisplayChars(int maxChars) {
    m_maxDisplayChars = maxChars;
    updateDisplayText();
}

void AddressLink::updateDisplayText() {
    if (m_maxDisplayChars > 0 && m_address.length() > m_maxDisplayChars) {
        m_label->setText(m_address.left(m_maxDisplayChars) + QStringLiteral("\u2026"));
    } else {
        m_label->setText(m_address);
    }
}

QString AddressLink::address() const { return m_address; }

void AddressLink::registerLink() {
    if (!m_address.isEmpty()) {
        s_registry[m_address].append(this);
    }
}

void AddressLink::unregisterLink() {
    if (!m_address.isEmpty()) {
        auto it = s_registry.find(m_address);
        if (it != s_registry.end()) {
            it->removeOne(this);
            if (it->isEmpty()) {
                s_registry.erase(it);
            }
        }
    }
}

bool AddressLink::event(QEvent* event) {
    if (event->type() == QEvent::HoverEnter) {
        // Highlight all siblings with the same address
        const auto& siblings = s_registry.value(m_address);
        for (AddressLink* link : siblings) {
            link->applyHoverStyle();
        }
    } else if (event->type() == QEvent::HoverLeave) {
        // Restore all siblings
        const auto& siblings = s_registry.value(m_address);
        for (AddressLink* link : siblings) {
            link->applyDefaultStyle();
            if (link->m_tooltip) {
                link->m_tooltip->hide();
            }
        }
    } else if (event->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(event);
        if (!m_tooltip) {
            m_tooltip = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
            m_tooltip->setStyleSheet("background: #1e1f37; color: white;"
                                     "border: 1px solid rgba(100,100,150,0.3);"
                                     "border-radius: 6px; padding: 6px 10px;"
                                     "font-size: 13px;"
                                     "font-family: 'JetBrains Mono', 'SF Mono', 'Menlo';");
        }
        m_tooltip->setText(m_address);
        m_tooltip->adjustSize();
        m_tooltip->move(he->globalPos().x() + 12, he->globalPos().y() + 16);
        m_tooltip->show();
        return true;
    }
    return QWidget::event(event);
}

void AddressLink::applyDefaultStyle() { m_label->setStyleSheet(kDefaultStyle); }

void AddressLink::applyHoverStyle() { m_label->setStyleSheet(kHoverStyle); }

void AddressLink::setContactInfo(const QString& name, const QString& avatarPath) {
    m_label->setText(name);

    if (!avatarPath.isEmpty()) {
        QPixmap pm(avatarPath);
        if (!pm.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            m_avatarLabel->setPixmap(AvatarCache::circleClip(pm, 16, dpr));
            m_avatarLabel->show();
        }
    }
    // m_address stays unchanged — copy + tooltip still use the raw address
}
