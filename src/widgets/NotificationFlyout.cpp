#include "NotificationFlyout.h"
#include "Theme.h"
#include "db/NotificationDb.h"
#include "db/TokenAccountDb.h"
#include "tx/KnownTokens.h"
#include "util/TimeUtils.h"
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStyle>
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

NotificationFlyout::NotificationFlyout(QWidget* parent) : QWidget(parent) {
    setFixedWidth(360);
    setFixedHeight(400);
    hide();
    setObjectName("notificationFlyout");
    setAttribute(Qt::WA_StyledBackground, true);

    // Drop shadow
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(40);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 120));
    setGraphicsEffect(shadow);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(0);

    // Header row: title + "Mark all as read" button
    auto* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("Notifications");
    title->setObjectName("notificationFlyoutTitle");

    auto* markReadBtn = new QPushButton("Mark all as read");
    markReadBtn->setObjectName("notificationFlyoutMarkReadBtn");
    markReadBtn->setCursor(Qt::PointingHandCursor);

    connect(markReadBtn, &QPushButton::clicked, this, [this]() {
        NotificationDb::markAllRead();
        reload();
        emit allMarkedRead();
    });

    headerRow->addWidget(title);
    headerRow->addStretch();
    headerRow->addWidget(markReadBtn);
    mainLayout->addLayout(headerRow);

    // Separator
    auto* separator = new QWidget();
    separator->setObjectName("notificationFlyoutSeparator");
    separator->setFixedHeight(1);
    mainLayout->addSpacing(12);
    mainLayout->addWidget(separator);
    mainLayout->addSpacing(8);

    // Scroll area for notification list
    m_scrollArea = new QScrollArea();
    m_scrollArea->setObjectName("notificationFlyoutScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->setObjectName("notificationFlyoutViewport");
    m_scrollArea->viewport()->setAttribute(Qt::WA_StyledBackground, true);

    auto* listWidget = new QWidget();
    listWidget->setObjectName("notificationFlyoutList");
    listWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_listLayout = new QVBoxLayout(listWidget);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(0);
    m_listLayout->addStretch();

    m_scrollArea->setWidget(listWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // Empty state label (inside scroll content so layout stays stable)
    m_emptyLabel = new QLabel("No notifications yet");
    m_emptyLabel->setObjectName("notificationFlyoutEmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();
    m_listLayout->insertWidget(0, m_emptyLabel);
}

void NotificationFlyout::showFlyout() {
    reload();
    raise();
    show();
    qApp->installEventFilter(this);
}

void NotificationFlyout::hideFlyout() {
    hide();
    qApp->removeEventFilter(this);
}

void NotificationFlyout::reload() {
    auto notifications = NotificationDb::getRecentRecords(50);
    buildList(notifications);
}

void NotificationFlyout::buildList(const QList<NotificationRecord>& notifications) {
    // Remove all items from layout — protect m_emptyLabel from deletion
    while (m_listLayout->count() > 0) {
        auto* item = m_listLayout->takeAt(0);
        QWidget* w = item->widget();
        if (w && w != m_emptyLabel) {
            w->deleteLater();
        }
        delete item;
    }

    // Rebuild layout from scratch
    if (notifications.isEmpty()) {
        m_emptyLabel->show();
        m_listLayout->addWidget(m_emptyLabel);
    } else {
        m_emptyLabel->hide();
        for (int i = 0; i < notifications.size(); ++i) {
            QWidget* row = createNotificationRow(notifications[i]);
            m_listLayout->addWidget(row);
        }
    }
    m_listLayout->addStretch();
}

QWidget* NotificationFlyout::createNotificationRow(const NotificationRecord& n) {
    bool isRead = n.isRead;
    qint64 createdAt = n.createdAt;
    QString amountText = n.amount;
    QString bodyText = n.body;

    // Resolve token icon + symbol from stored token field
    QString token = n.token;
    QString tokenSymbol;
    QString iconPath;
    if (token == "SOL") {
        tokenSymbol = "SOL";
        iconPath = ":/icons/tokens/sol.png";
    } else if (!token.isEmpty()) {
        KnownToken kt = resolveKnownToken(token);
        if (!kt.symbol.isEmpty()) {
            tokenSymbol = kt.symbol;
            iconPath = kt.iconPath;
        } else {
            auto tokenInfo = TokenAccountDb::getTokenRecord(token);
            if (tokenInfo.has_value()) {
                tokenSymbol = tokenInfo->symbol;
            }
            if (tokenSymbol.isEmpty()) {
                tokenSymbol = token.left(4) + "..." + token.right(4);
            }
        }
    }

    int notifId = n.id;

    auto* row = new QWidget();
    row->setObjectName("notificationFlyoutRow");
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setAttribute(Qt::WA_Hover, true);
    row->setCursor(Qt::PointingHandCursor);
    row->setFixedHeight(60);
    row->setProperty("notifId", notifId);
    row->setProperty("isRead", isRead);
    row->setProperty("hovered", false);
    row->installEventFilter(this);

    auto* hLayout = new QHBoxLayout(row);
    hLayout->setContentsMargins(8, 8, 8, 8);
    hLayout->setSpacing(8);

    // Unread indicator dot
    auto* dot = new QLabel();
    dot->setObjectName("notificationFlyoutDot");
    dot->setFixedSize(8, 8);
    dot->setProperty("isRead", isRead);
    hLayout->addWidget(dot, 0, Qt::AlignVCenter);

    // Center column: title row (with inline icon) + body
    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);

    // Title row: "Received 0.00001 <icon> SOL"
    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(4);

    auto* receivedLabel = new QLabel(QString("Received %1").arg(amountText));
    receivedLabel->setProperty("uiClass", "notificationFlyoutReceivedLabel");
    titleRow->addWidget(receivedLabel);

    // Inline token icon (14x14)
    if (!iconPath.isEmpty()) {
        auto* inlineIcon = new QLabel();
        inlineIcon->setFixedSize(14, 14);
        qreal dpr = screen() ? screen()->devicePixelRatio() : 2.0;
        int px = static_cast<int>(14 * dpr);
        QPixmap tokenPx(iconPath);
        if (!tokenPx.isNull()) {
            tokenPx = tokenPx.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            tokenPx.setDevicePixelRatio(dpr);
            inlineIcon->setPixmap(tokenPx);
        }
        inlineIcon->setProperty("uiClass", "notificationFlyoutInlineIcon");
        titleRow->addWidget(inlineIcon, 0, Qt::AlignVCenter);
    }

    auto* symbolLabel = new QLabel(tokenSymbol);
    symbolLabel->setProperty("uiClass", "notificationFlyoutSymbolLabel");
    titleRow->addWidget(symbolLabel);
    titleRow->addStretch();

    textLayout->addLayout(titleRow);

    // Body: "From CpKb3YdZ...V6Nhd6q"
    auto* bodyLabel = new QLabel(bodyText);
    bodyLabel->setProperty("uiClass", "notificationFlyoutBodyLabel");

    textLayout->addWidget(bodyLabel);
    hLayout->addLayout(textLayout, 1);

    // Right: relative time
    auto* timeLabel = new QLabel(formatRelativeTime(createdAt));
    timeLabel->setProperty("uiClass", "notificationFlyoutTimeLabel");
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
    hLayout->addWidget(timeLabel);

    return row;
}

bool NotificationFlyout::eventFilter(QObject* obj, QEvent* event) {
    // Click-outside-to-dismiss
    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
        if (!rect().contains(localPos)) {
            hideFlyout();
            return false;
        }
    }

    // Notification row hover + click
    auto* widget = qobject_cast<QWidget*>(obj);
    if (widget && widget->property("notifId").isValid()) {
        if (event->type() == QEvent::HoverEnter) {
            widget->setProperty("hovered", true);
            repolish(widget);
        } else if (event->type() == QEvent::HoverLeave) {
            widget->setProperty("hovered", false);
            repolish(widget);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (!widget->property("isRead").toBool()) {
                int id = widget->property("notifId").toInt();
                NotificationDb::markRead(id);
                // Defer reload — we're inside eventFilter for this widget,
                // and reload() destroys it (use-after-free)
                QTimer::singleShot(0, this, [this]() {
                    reload();
                    emit badgeChanged();
                });
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void NotificationFlyout::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Solid dark background with rounded corners
    QPainterPath path;
    path.addRoundedRect(rect(), Theme::cardRadius, Theme::cardRadius);

    p.fillPath(path, QColor(Theme::bgGradientEnd));

    // Border
    p.setPen(QPen(Theme::borderQColor, 1));
    p.drawPath(path);
}
