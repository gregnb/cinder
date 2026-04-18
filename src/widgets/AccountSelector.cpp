#include "widgets/AccountSelector.h"
#include "db/WalletDb.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>

// ── Helpers ──────────────────────────────────────────────────────

static QPixmap renderSvgIcon(const QString& path, int logicalSize, qreal opacity = 1.0) {
    qreal dpr = qApp->devicePixelRatio();
    int px = qRound(logicalSize * dpr);
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);

    QSvgRenderer renderer(path);
    QPainter p(&pm);
    p.setOpacity(opacity);
    renderer.render(&p);

    pm.setDevicePixelRatio(dpr); // set AFTER rendering so SVG fills full physical pixels
    return pm;
}

// ── Constants ────────────────────────────────────────────────────

static const int kRowHeight = 40;
static const int kMaxPopupHeight = 320;
static const int kPopupPadding = 10;

// Deterministic avatar colors from address hash
static const QColor kAvatarColors[] = {
    QColor("#14F195"), QColor("#9945FF"), QColor("#4DA1FF"), QColor("#FF6B6B"),
    QColor("#FFB347"), QColor("#B8E986"), QColor("#FF85C0"), QColor("#7AFBFF"),
};
static const int kNumColors = sizeof(kAvatarColors) / sizeof(kAvatarColors[0]);

// ── AccountSelector ─────────────────────────────────────────────

AccountSelector::AccountSelector(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Button (avatar + address + chevron) ──
    m_button = new QPushButton();
    m_button->setObjectName("accountSelectorBtn");
    m_button->setFixedHeight(40);
    m_button->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* btnLayout = new QHBoxLayout(m_button);
    // 10px margins: avatar(24) + 2*margin(10) = 44px, so avatar stays
    // at x=10 in both expanded (margin) and collapsed (centered in 44px).
    // Center at x=22 matches nav button icon centers: (44-32)/2+16 = 22.
    btnLayout->setContentsMargins(10, 0, 10, 0);
    btnLayout->setSpacing(8);

    m_avatar = new QLabel();
    m_avatar->setFixedSize(24, 24);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_avatar);

    m_addressLabel = new QLabel();
    m_addressLabel->setObjectName("accountSelectorAddressLabel");
    m_addressLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_addressLabel, 1);

    // Copy address icon (QLabel — avoids QPushButton icon DPI clipping)
    m_copyIcon = new QLabel();
    m_copyIcon->setObjectName("accountSelectorCopyIcon");
    m_copyIcon->setFixedSize(16, 16);
    m_copyIcon->setAlignment(Qt::AlignCenter);
    m_copyIcon->setCursor(Qt::PointingHandCursor);
    m_copyIcon->setToolTip(tr("Copy address"));
    m_copyIcon->setPixmap(renderSvgIcon(":/icons/ui/copy.svg", 16, 0.55));
    m_copyIcon->setAttribute(Qt::WA_Hover, true);
    m_copyIcon->installEventFilter(this);
    // NOT WA_TransparentForMouseEvents — this label intercepts clicks
    btnLayout->addWidget(m_copyIcon);

    m_copyFeedbackTimer.setSingleShot(true);
    m_copyFeedbackTimer.setInterval(1500);
    connect(&m_copyFeedbackTimer, &QTimer::timeout, this, [this]() {
        m_copyIcon->setPixmap(renderSvgIcon(":/icons/ui/copy.svg", 16, 0.55));
        m_copyIcon->setToolTip(tr("Copy address"));
    });

    m_chevron = new QLabel();
    m_chevron->setObjectName("accountSelectorChevron");
    m_chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
    updateChevron(false);
    btnLayout->addWidget(m_chevron);

    connect(m_button, &QPushButton::clicked, this, [this]() {
        if (m_popup && m_popup->isVisible()) {
            hidePopup();
        } else {
            showPopup();
        }
    });
    outer->addWidget(m_button);

    // ── Popup (created unparented for overlay positioning) ──
    m_popup = new QWidget();
    m_popup->hide();
    m_popup->setObjectName("accountPopup");
    m_popup->setAttribute(Qt::WA_StyledBackground, true);

    m_popupLayout = new QVBoxLayout(m_popup);
    m_popupLayout->setContentsMargins(kPopupPadding, kPopupPadding, kPopupPadding, kPopupPadding);
    m_popupLayout->setSpacing(2);

    // Scrollable area for wallet rows
    m_scrollArea = new QScrollArea();
    m_scrollArea->setObjectName("accountPopupScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scrollContent = new QWidget();
    m_scrollContent->setObjectName("accountPopupScrollContent");
    m_scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    m_scrollLayout = new QVBoxLayout(m_scrollContent);
    m_scrollLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollLayout->setSpacing(2);

    m_scrollArea->setWidget(m_scrollContent);
    m_popupLayout->addWidget(m_scrollArea, 1);

    qApp->installEventFilter(this);
}

AccountSelector::~AccountSelector() { delete m_popup; }

// ── Public API ──────────────────────────────────────────────────

void AccountSelector::setAccounts(const QList<WalletSummaryRecord>& wallets) {
    m_accounts = wallets;
    updateButton();
    if (m_popup->isVisible()) {
        rebuildPopup();
    }
}

void AccountSelector::setActiveAddress(const QString& address) {
    m_activeAddress = address;
    updateButton();
    if (m_popup->isVisible()) {
        rebuildPopup();
    }
}

void AccountSelector::setCollapsed(bool collapsed) {
    m_collapsed = collapsed;
    m_addressLabel->setVisible(!collapsed);
    m_copyIcon->setVisible(!collapsed);
    m_chevron->setVisible(!collapsed);
    if (collapsed) {
        m_button->setFixedWidth(44);
    } else {
        m_button->setMinimumWidth(0);
        m_button->setMaximumWidth(QWIDGETSIZE_MAX);
    }
}

// ── Internal ────────────────────────────────────────────────────

void AccountSelector::updateButton() {
    if (m_activeAddress.isEmpty() && !m_accounts.isEmpty()) {
        m_activeAddress = m_accounts.first().address;
    }

    // Avatar
    int sz = 24;
    qreal dpr = qApp->devicePixelRatio();
    int pxSz = qRound(sz * dpr);
    QPixmap pm(pxSz, pxSz);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    // Check for custom avatar
    QString avatarRel;
    QString avatarText;
    for (const auto& w : m_accounts) {
        if (w.address == m_activeAddress) {
            avatarRel = w.avatarPath;
            if (w.isImportedPrivateKey()) {
                avatarText = "i";
            } else {
                avatarText = QString::number(w.accountIndex + 1);
            }
            break;
        }
    }

    bool hasCustomAvatar = false;
    if (!avatarRel.isEmpty()) {
        QPixmap src(WalletDb::avatarFullPath(avatarRel));
        if (!src.isNull()) {
            src = src.scaled(pxSz, pxSz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            src.setDevicePixelRatio(dpr);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addEllipse(0, 0, sz, sz);
            p.setClipPath(clip);
            p.drawPixmap(0, 0, src);
            hasCustomAvatar = true;
        }
    }

    if (!hasCustomAvatar) {
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(avatarColor(m_activeAddress));
        p.drawEllipse(0, 0, sz, sz);
        p.setPen(Qt::white);
        QFont f;
        f.setPixelSize(11);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.drawText(QRect(0, 0, sz, sz), Qt::AlignCenter, avatarText);
    }
    m_avatar->setPixmap(pm);

    // Address text
    m_addressLabel->setText(truncateAddress(m_activeAddress));
}

QPushButton* AccountSelector::buildPopupRow(const WalletSummaryRecord& wallet) {
    QString address = wallet.address;
    int accountIndex = wallet.accountIndex;
    bool isImported = wallet.isImportedPrivateKey();
    QString label = wallet.label;
    if (label.isEmpty()) {
        label = QString("Wallet %1").arg(accountIndex + 1);
    }
    bool isActive = (address == m_activeAddress);

    QPushButton* row = new QPushButton();
    row->setFixedHeight(kRowHeight);
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet(QString("QPushButton {"
                               "  background: %1;"
                               "  border: %2;"
                               "  border-radius: 8px;"
                               "  padding: 0 10px;"
                               "}"
                               "QPushButton:hover {"
                               "  background: #253050;"
                               "}")
                           .arg(isActive ? "rgba(20,241,149,0.08)" : "transparent",
                                isActive ? "1px solid rgba(20,241,149,0.3)" : "none"));

    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(6, 0, 6, 0);
    rowLayout->setSpacing(10);

    // Avatar circle
    QLabel* avatar = new QLabel();
    avatar->setFixedSize(28, 28);
    avatar->setAttribute(Qt::WA_TransparentForMouseEvents);
    qreal dpr = qApp->devicePixelRatio();
    int avPx = qRound(28 * dpr);
    QPixmap pm(avPx, avPx);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QString avatarRel = wallet.avatarPath;
    bool hasCustom = false;
    if (!avatarRel.isEmpty()) {
        QPixmap src(WalletDb::avatarFullPath(avatarRel));
        if (!src.isNull()) {
            src = src.scaled(avPx, avPx, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            src.setDevicePixelRatio(dpr);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addEllipse(0, 0, 28, 28);
            p.setClipPath(clip);
            p.drawPixmap(0, 0, src);
            hasCustom = true;
        }
    }

    if (!hasCustom) {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(avatarColor(address));
        p.drawEllipse(0, 0, 28, 28);
        p.setPen(Qt::white);
        QFont f;
        f.setPixelSize(12);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        QString avatarText = isImported ? "i" : QString::number(accountIndex + 1);
        p.drawText(QRect(0, 0, 28, 28), Qt::AlignCenter, avatarText);
    }
    avatar->setPixmap(pm);
    rowLayout->addWidget(avatar, 0, Qt::AlignVCenter);

    // Name + address
    QWidget* textCol = new QWidget();
    textCol->setAttribute(Qt::WA_TransparentForMouseEvents);
    QVBoxLayout* textLayout = new QVBoxLayout(textCol);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(0);

    QWidget* nameRow = new QWidget();
    nameRow->setAttribute(Qt::WA_TransparentForMouseEvents);
    QHBoxLayout* nameRowLayout = new QHBoxLayout(nameRow);
    nameRowLayout->setContentsMargins(0, 0, 0, 0);
    nameRowLayout->setSpacing(6);

    QLabel* nameLabel = new QLabel(label);
    nameLabel->setProperty("uiClass", "accountPopupNameLabel");
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameRowLayout->addWidget(nameLabel);

    if (isImported) {
        QLabel* tag = new QLabel(tr("Imported"));
        tag->setProperty("uiClass", "accountPopupImportedTag");
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameRowLayout->addWidget(tag);
    }

    nameRowLayout->addStretch();
    textLayout->addWidget(nameRow);

    QLabel* addrLabel = new QLabel(truncateAddress(address));
    addrLabel->setProperty("uiClass", "accountPopupAddressLabel");
    addrLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    textLayout->addWidget(addrLabel);

    rowLayout->addWidget(textCol, 1);

    // Active indicator
    if (isActive) {
        QLabel* check = new QLabel();
        check->setFixedSize(16, 16);
        check->setAttribute(Qt::WA_TransparentForMouseEvents);
        QPixmap checkPm(qRound(16 * dpr), qRound(16 * dpr));
        checkPm.setDevicePixelRatio(dpr);
        checkPm.fill(Qt::transparent);
        {
            QPainter p(&checkPm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor("#14F195"), 2));
            p.drawEllipse(2, 2, 12, 12);
            p.setBrush(QColor("#14F195"));
            p.setPen(Qt::NoPen);
            p.drawEllipse(5, 5, 6, 6);
        }
        check->setPixmap(checkPm);
        rowLayout->addWidget(check, 0, Qt::AlignVCenter);
    }

    connect(row, &QPushButton::clicked, this, [this, address]() {
        if (address != m_activeAddress) {
            emit accountSwitched(address);
        }
        hidePopup();
    });

    return row;
}

void AccountSelector::rebuildPopup() {
    // Clear scroll content (wallet rows)
    while (m_scrollLayout->count() > 0) {
        QLayoutItem* item = m_scrollLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    // Remove footer widgets (divider + add button) from popup layout,
    // but keep the scroll area (index 0)
    while (m_popupLayout->count() > 1) {
        QLayoutItem* item = m_popupLayout->takeAt(1);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    // ── Group wallets by parent-child hierarchy ──
    struct WalletGroup {
        WalletSummaryRecord parent;
        QList<WalletSummaryRecord> children;
    };

    QList<WalletGroup> groups;
    QMap<int, int> parentToGroupIdx;

    // First pass: root wallets (parentWalletId <= 0)
    for (const auto& w : m_accounts) {
        if (w.parentWalletId <= 0) {
            WalletGroup group;
            group.parent = w;
            parentToGroupIdx[w.id] = groups.size();
            groups.append(group);
        }
    }

    // Second pass: assign children
    for (const auto& w : m_accounts) {
        if (w.parentWalletId > 0) {
            if (parentToGroupIdx.contains(w.parentWalletId)) {
                groups[parentToGroupIdx[w.parentWalletId]].children.append(w);
            } else {
                // Orphan — treat as standalone
                WalletGroup group;
                group.parent = w;
                groups.append(group);
            }
        }
    }

    // Sort children by accountIndex
    for (auto& group : groups) {
        std::sort(group.children.begin(), group.children.end(),
                  [](const WalletSummaryRecord& a, const WalletSummaryRecord& b) {
                      return a.accountIndex < b.accountIndex;
                  });
    }

    // ── Render grouped rows into scroll content ──
    for (int g = 0; g < groups.size(); ++g) {
        const WalletGroup& group = groups[g];

        // Parent row
        m_scrollLayout->addWidget(buildPopupRow(group.parent));

        // Child rows with tree connectors
        for (int c = 0; c < group.children.size(); ++c) {
            const auto& child = group.children[c];
            bool isLast = (c == group.children.size() - 1);

            QWidget* container = new QWidget();
            container->setObjectName("accountPopupChildContainer");
            container->setAttribute(Qt::WA_StyledBackground, true);
            QHBoxLayout* hbox = new QHBoxLayout(container);
            hbox->setContentsMargins(16, 0, 0, 0);
            hbox->setSpacing(0);

            // Small tree connector via painted pixmap
            qreal dpr = qApp->devicePixelRatio();
            int cw = 20, ch = kRowHeight;
            QPixmap connPm(qRound(cw * dpr), qRound(ch * dpr));
            connPm.setDevicePixelRatio(dpr);
            connPm.fill(Qt::transparent);
            {
                QPainter p(&connPm);
                p.setRenderHint(QPainter::Antialiasing);
                QColor lineColor(100, 150, 255, 65);
                p.setPen(QPen(lineColor, 1.5));
                int midX = 6;
                int midY = ch / 2;
                p.drawLine(midX, 0, midX, midY);
                if (!isLast) {
                    p.drawLine(midX, midY, midX, ch);
                }
                p.drawLine(midX, midY, cw - 2, midY);
            }
            QLabel* connLabel = new QLabel();
            connLabel->setFixedSize(cw, ch);
            connLabel->setPixmap(connPm);
            connLabel->setProperty("uiClass", "accountPopupConnectorLabel");
            connLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            hbox->addWidget(connLabel);

            hbox->addWidget(buildPopupRow(child), 1);
            m_scrollLayout->addWidget(container);
        }

        // Per-group "+ Add Account" if parent has a stored seed
        if (WalletDb::getSeedBlobRecord(group.parent.id).has_value()) {
            QPushButton* addAccBtn = new QPushButton("+ Add Account");
            addAccBtn->setObjectName("accountPopupAddAccountBtn");
            addAccBtn->setFixedHeight(30);
            addAccBtn->setCursor(Qt::PointingHandCursor);
            int parentId = group.parent.id;
            connect(addAccBtn, &QPushButton::clicked, this, [this, parentId]() {
                hidePopup();
                emit addAccountRequested(parentId);
            });
            m_scrollLayout->addWidget(addAccBtn);
        }

        // Separator between groups (not after last)
        if (g < groups.size() - 1) {
            QFrame* sep = new QFrame();
            sep->setObjectName("accountPopupGroupSeparator");
            sep->setFixedHeight(1);
            m_scrollLayout->addWidget(sep);
        }
    }

    // ── Footer (outside scroll area) ──

    // Divider
    QFrame* divider = new QFrame();
    divider->setObjectName("accountPopupFooterDivider");
    divider->setFixedHeight(1);
    m_popupLayout->addWidget(divider);

    // "+ Add Wallet" button (opens import flow)
    QPushButton* addBtn = new QPushButton("+ Add Wallet");
    addBtn->setObjectName("accountPopupAddWalletBtn");
    addBtn->setFixedHeight(38);
    addBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        hidePopup();
        emit addWalletRequested();
    });
    m_popupLayout->addWidget(addBtn);
}

void AccountSelector::showPopup() {
    QWidget* container = window();
    if (!container) {
        return;
    }

    if (m_popup->parentWidget() != container) {
        m_popup->setParent(container);
    }

    rebuildPopup();

    // Calculate height: scroll content + footer (divider + add button) + padding
    int scrollContentH = m_scrollContent->sizeHint().height();
    int footerH = 1 + 38; // divider + add button
    int maxScrollH = kMaxPopupHeight - footerH - kPopupPadding * 2;
    int scrollH = qMin(scrollContentH, maxScrollH);
    int totalH = scrollH + footerH + kPopupPadding * 2 + 4; // 4px spacing

    int popupWidth = 300;

    // Position above the button
    QPoint globalBtn = m_button->mapToGlobal(QPoint(0, 0));
    QPoint containerPos = container->mapFromGlobal(globalBtn);

    int popupY = containerPos.y() - totalH - 4;
    if (popupY < 0) {
        // Not enough room above — show below
        popupY = containerPos.y() + m_button->height() + 4;
    }

    // Align left edge with the button
    int popupX = containerPos.x();
    if (popupX < 4) {
        popupX = 4;
    }

    m_popup->setGeometry(popupX, popupY, popupWidth, totalH);
    m_popup->raise();
    m_popup->show();
    updateChevron(true);
}

void AccountSelector::hidePopup() {
    m_popup->hide();
    m_button->clearFocus();
    updateChevron(false);
}

void AccountSelector::updateChevron(bool open) {
    qreal dpr = qApp->devicePixelRatio();
    QString path = open ? ":/icons/ui/chevron-up.png" : ":/icons/ui/chevron-down.png";
    QPixmap px(path);
    int sz = qRound(14 * dpr);
    px = px.scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    px.setDevicePixelRatio(dpr);
    m_chevron->setPixmap(px);
}

QColor AccountSelector::avatarColor(const QString& address) {
    if (address.isEmpty()) {
        return kAvatarColors[0];
    }
    uint hash = 0;
    for (const QChar& c : address) {
        hash = hash * 31 + c.unicode();
    }
    return kAvatarColors[hash % kNumColors];
}

QString AccountSelector::truncateAddress(const QString& address) {
    if (address.length() <= 10) {
        return address;
    }
    return address.left(4) + "..." + address.right(4);
}

bool AccountSelector::eventFilter(QObject* obj, QEvent* event) {
    // Copy icon hover + click
    if (obj == m_copyIcon) {
        if (event->type() == QEvent::Enter && !m_copyFeedbackTimer.isActive()) {
            m_copyIcon->setPixmap(renderSvgIcon(":/icons/ui/copy.svg", 16, 1.0));
            return false;
        }
        if (event->type() == QEvent::Leave && !m_copyFeedbackTimer.isActive()) {
            m_copyIcon->setPixmap(renderSvgIcon(":/icons/ui/copy.svg", 16, 0.55));
            return false;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            QApplication::clipboard()->setText(m_activeAddress);
            m_copyIcon->setPixmap(renderSvgIcon(":/icons/ui/checkmark.svg", 16));
            m_copyIcon->setToolTip(tr("Copied!"));
            m_copyFeedbackTimer.start();
            return true;
        }
    }

    if (m_popup && m_popup->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint globalPos = me->globalPosition().toPoint();

            QRect btnRect(m_button->mapToGlobal(QPoint(0, 0)), m_button->size());
            QRect popRect(m_popup->mapToGlobal(QPoint(0, 0)), m_popup->size());

            if (!btnRect.contains(globalPos) && !popRect.contains(globalPos)) {
                hidePopup();
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                hidePopup();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
