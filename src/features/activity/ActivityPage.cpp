#include "ActivityPage.h"
#include "services/AvatarCache.h"
#include "util/ContactResolver.h"
#include "util/TimeUtils.h"
#include "util/TxIconUtils.h"
#include "widgets/Dropdown.h"
#include "widgets/StyledCalendar.h"
#include "widgets/StyledCheckbox.h"
#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

// Position popup below anchor, flipping left if it would overflow the screen
static void positionPopup(QWidget* popup, QPushButton* anchor) {
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 4));
    QScreen* screen = anchor->screen();
    if (screen) {
        int screenRight = screen->availableGeometry().right();
        if (pos.x() + popup->width() > screenRight) {
            pos.setX(anchor->mapToGlobal(QPoint(anchor->width(), 0)).x() - popup->width());
        }
    }
    popup->move(pos);
}

static QString activityBadgeKind(const QString& type) {
    if (type == "send") {
        return "send";
    }
    if (type == "receive") {
        return "receive";
    }
    if (type == "mint") {
        return "mint";
    }
    if (type == "burn") {
        return "burn";
    }
    if (type == "create_account") {
        return "create_account";
    }
    if (type == "create_nonce") {
        return "create_nonce";
    }
    if (type == "close_account") {
        return "close_account";
    }
    if (type == "init_account") {
        return "init_account";
    }
    return "unknown";
}

static QString amountToneForColor(const QString& color) {
    if (color.compare("#10b981", Qt::CaseInsensitive) == 0) {
        return "positive";
    }
    if (color.compare("#ef4444", Qt::CaseInsensitive) == 0) {
        return "negative";
    }
    return "neutral";
}

// ── Constructor ──────────────────────────────────────────────────

ActivityPage::ActivityPage(QWidget* parent) : QWidget(parent) {
    setObjectName("activityContent");
    setProperty("uiClass", "content");

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setObjectName("activityViewport");
    scroll->viewport()->setAttribute(Qt::WA_StyledBackground, true);

    QWidget* content = new QWidget();
    content->setObjectName("activityViewport");
    content->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 30);
    contentLayout->setSpacing(12);

    // ── Title row with syncing badge ──
    QWidget* titleRow = new QWidget();
    titleRow->setObjectName("activityTransparent");
    QHBoxLayout* titleLay = new QHBoxLayout(titleRow);
    titleLay->setContentsMargins(0, 0, 0, 0);
    titleLay->setSpacing(12);

    m_titleLabel = new QLabel(tr("Activity"));
    m_titleLabel->setObjectName("pageTitle");
    titleLay->addWidget(m_titleLabel);

    // Syncing badge — matches SEND/RECEIVE badge style
    m_syncBadge = new QWidget();
    m_syncBadge->setAttribute(Qt::WA_StyledBackground, true);
    m_syncBadge->setObjectName("activitySyncBadge");
    m_syncBadge->setFixedHeight(24);
    QHBoxLayout* badgeLay = new QHBoxLayout(m_syncBadge);
    badgeLay->setContentsMargins(8, 0, 8, 0);
    badgeLay->setSpacing(5);

    m_syncSpinner = new QWidget();
    m_syncSpinner->setFixedSize(12, 12);
    m_syncSpinner->setObjectName("activityTransparent");
    badgeLay->addWidget(m_syncSpinner, 0, Qt::AlignVCenter);

    m_syncText = new QLabel(tr("SYNCING"));
    m_syncText->setObjectName("activitySyncText");
    badgeLay->addWidget(m_syncText, 0, Qt::AlignVCenter);

    m_syncBadge->setVisible(false);

    QWidget* badgeWrap = new QWidget();
    badgeWrap->setObjectName("activityTransparent");
    QVBoxLayout* wrapLay = new QVBoxLayout(badgeWrap);
    wrapLay->setContentsMargins(0, 6, 0, 0);
    wrapLay->setSpacing(0);
    wrapLay->addWidget(m_syncBadge);
    titleLay->addWidget(badgeWrap, 0, Qt::AlignVCenter);

    titleLay->addStretch();
    contentLayout->addWidget(titleRow);

    // Animate: repaint the spinner arc every 30ms
    m_syncSpinTimer.setInterval(30);
    connect(&m_syncSpinTimer, &QTimer::timeout, this, [this]() {
        m_syncAngle = (m_syncAngle + 6) % 360;
        m_syncSpinner->update();
    });

    // Install a paint handler on the spinner widget
    m_syncSpinner->installEventFilter(this);

    contentLayout->addSpacing(12);

    // ── Active filter chips bar ──
    m_chipBar = new QWidget();
    m_chipBar->setObjectName("activityTransparent");
    m_chipBar->setVisible(false);
    m_chipLayout = new QHBoxLayout(m_chipBar);
    m_chipLayout->setContentsMargins(0, 0, 0, 0);
    m_chipLayout->setSpacing(6);
    m_chipLayout->addStretch();
    contentLayout->addWidget(m_chipBar);

    // ── Status label ──
    m_statusLabel = new QLabel(tr("Loading..."));
    m_statusLabel->setObjectName("activityStatusLabel");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_statusLabel);

    // ── Column headers with filter icons ──
    m_headerRow = new QWidget();
    m_headerRow->setObjectName("activityTransparent");
    m_headerRow->setVisible(false);
    QHBoxLayout* hdr = new QHBoxLayout(m_headerRow);
    hdr->setContentsMargins(14, 0, 14, 0);
    hdr->setSpacing(8);

    // Filter icon — SVG funnel rendered as pixmap
    QPixmap filterPx = txTypeIcon("filter", 12, devicePixelRatioF(), QColor(255, 255, 255, 100));
    QPixmap sortPx = txTypeIcon("sort", 12, devicePixelRatioF(), QColor(255, 255, 255, 100));
    int colIndex = 0;
    auto makeHdrWithFilter = [&](const QString& text, int stretch, Qt::Alignment align,
                                 QPushButton*& filterBtn) {
        QWidget* col = new QWidget();
        col->setObjectName("activityTransparent");
        QHBoxLayout* colLay = new QHBoxLayout(col);
        colLay->setContentsMargins(0, 0, 0, 0);
        colLay->setSpacing(3);

        QLabel* lbl = new QLabel(text);
        lbl->setObjectName("activityHeaderLabel");
        lbl->setProperty("activeSort", false);
        lbl->setAlignment(align | Qt::AlignVCenter);

        // Sort button
        QPushButton* sortBtn = new QPushButton();
        sortBtn->setIcon(QIcon(sortPx));
        sortBtn->setIconSize(QSize(12, 12));
        sortBtn->setCursor(Qt::PointingHandCursor);
        sortBtn->setObjectName("activitySortBtn");
        sortBtn->setFixedSize(16, 20);
        int ci = colIndex;
        connect(sortBtn, &QPushButton::clicked, this, [this, ci]() { toggleSort(ci); });
        m_sortBtns.append(sortBtn);
        m_sortLabels.append(lbl);

        // Filter button
        filterBtn = new QPushButton();
        filterBtn->setIcon(QIcon(filterPx));
        filterBtn->setIconSize(QSize(12, 12));
        filterBtn->setCursor(Qt::PointingHandCursor);
        filterBtn->setObjectName("activityFilterBtn");
        filterBtn->setProperty("activeFilter", false);
        filterBtn->setFixedSize(16, 20);

        if (align.testFlag(Qt::AlignRight)) {
            colLay->addStretch();
            colLay->addWidget(lbl);
            colLay->addWidget(sortBtn);
            colLay->addWidget(filterBtn);
        } else {
            colLay->addWidget(lbl);
            colLay->addWidget(sortBtn);
            colLay->addWidget(filterBtn);
            colLay->addStretch();
        }

        hdr->addWidget(col, stretch);
        ++colIndex;
    };

    makeHdrWithFilter(tr("SIGNATURE"), 2, Qt::AlignLeft, m_sigFilterBtn);
    makeHdrWithFilter(tr("TIME"), 2, Qt::AlignLeft, m_timeFilterBtn);
    makeHdrWithFilter(tr("ACTION"), 2, Qt::AlignLeft, m_actionFilterBtn);
    makeHdrWithFilter(tr("FROM"), 2, Qt::AlignLeft, m_fromFilterBtn);
    makeHdrWithFilter(tr("TO"), 2, Qt::AlignLeft, m_toFilterBtn);
    makeHdrWithFilter(tr("AMOUNT"), 2, Qt::AlignLeft, m_amountFilterBtn);
    makeHdrWithFilter(tr("TOKEN"), 1, Qt::AlignLeft, m_tokenFilterBtn);

    // Connect filter buttons
    connect(m_sigFilterBtn, &QPushButton::clicked, this,
            [this]() { showTextFilter(m_sigFilterBtn, tr("Search signature..."), m_sigFilter); });
    connect(m_timeFilterBtn, &QPushButton::clicked, this,
            [this]() { showTimeFilter(m_timeFilterBtn); });
    connect(m_actionFilterBtn, &QPushButton::clicked, this,
            [this]() { showActionFilter(m_actionFilterBtn); });
    connect(m_fromFilterBtn, &QPushButton::clicked, this,
            [this]() { showTextFilter(m_fromFilterBtn, tr("Search address..."), m_fromFilter); });
    connect(m_toFilterBtn, &QPushButton::clicked, this,
            [this]() { showTextFilter(m_toFilterBtn, tr("Search address..."), m_toFilter); });
    connect(m_amountFilterBtn, &QPushButton::clicked, this,
            [this]() { showAmountFilter(m_amountFilterBtn); });
    connect(m_tokenFilterBtn, &QPushButton::clicked, this,
            [this]() { showTextFilter(m_tokenFilterBtn, tr("Search token..."), m_tokenFilter); });

    // Default sort: TIME column descending (data arrives sorted by time desc)
    m_sortColumn = 1;
    m_sortDirection = 2;
    updateSortIcon(m_sortBtns[1], 2);
    m_sortLabels[1]->setProperty("activeSort", true);
    m_sortLabels[1]->style()->unpolish(m_sortLabels[1]);
    m_sortLabels[1]->style()->polish(m_sortLabels[1]);

    contentLayout->addWidget(m_headerRow);

    // ── Separator ──
    QFrame* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("activitySeparator");
    contentLayout->addWidget(sep);

    // ── Row list ──
    m_listLayout = new QVBoxLayout();
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(2);
    contentLayout->addLayout(m_listLayout);

    // ── Pagination bar: [stretch] Rows per page: [dropdown]  1-5 of 13  ‹  › ──
    QWidget* pagBar = new QWidget();
    pagBar->setObjectName("activityTransparent");
    QHBoxLayout* pagLay = new QHBoxLayout(pagBar);
    pagLay->setContentsMargins(14, 10, 14, 0);
    pagLay->setSpacing(12);

    pagLay->addStretch();

    QLabel* pageSizeLbl = new QLabel(tr("Rows per page:"));
    pageSizeLbl->setObjectName("activityMutedLabel12");
    pagLay->addWidget(pageSizeLbl);

    m_pageSizeDropdown = new Dropdown();
    m_pageSizeDropdown->addItem("50");
    m_pageSizeDropdown->addItem("100");
    m_pageSizeDropdown->addItem("200");
    m_pageSizeDropdown->addItem("500");
    m_pageSizeDropdown->setCurrentItem("100");
    m_pageSizeDropdown->setFixedWidth(80);
    connect(m_pageSizeDropdown, &Dropdown::itemSelected, this, [this](const QString& text) {
        m_pageSize = text.toInt();
        m_currentPage = 0;
        loadPage();
    });
    pagLay->addWidget(m_pageSizeDropdown);

    m_showingLabel = new QLabel();
    m_showingLabel->setObjectName("activityMutedLabel12");
    pagLay->addWidget(m_showingLabel);

    auto makePagBtn = [&](const QString& iconName) {
        auto* btn = new QPushButton();
        btn->setFixedSize(32, 32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName("activityPagBtn");
        btn->setIcon(
            QIcon(txTypeIcon(iconName, 14, devicePixelRatioF(), QColor(255, 255, 255, 180))));
        btn->setIconSize(QSize(14, 14));
        return btn;
    };

    m_firstPageBtn = makePagBtn("page-first");
    connect(m_firstPageBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentPage > 0) {
            m_currentPage = 0;
            loadPage();
        }
    });
    pagLay->addWidget(m_firstPageBtn);

    m_prevPageBtn = makePagBtn("cal-prev");
    connect(m_prevPageBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentPage > 0) {
            --m_currentPage;
            loadPage();
        }
    });
    pagLay->addWidget(m_prevPageBtn);

    m_nextPageBtn = makePagBtn("cal-next");
    connect(m_nextPageBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = qMax(1, (m_totalRows + m_pageSize - 1) / m_pageSize);
        if (m_currentPage < totalPages - 1) {
            ++m_currentPage;
            loadPage();
        }
    });
    pagLay->addWidget(m_nextPageBtn);

    m_lastPageBtn = makePagBtn("page-last");
    connect(m_lastPageBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = qMax(1, (m_totalRows + m_pageSize - 1) / m_pageSize);
        if (m_currentPage < totalPages - 1) {
            m_currentPage = totalPages - 1;
            loadPage();
        }
    });
    pagLay->addWidget(m_lastPageBtn);

    contentLayout->addWidget(pagBar);
    contentLayout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);

    // Refresh relative timestamps every 60 seconds
    m_relativeTimeTimer.setInterval(60000);
    connect(&m_relativeTimeTimer, &QTimer::timeout, this, &ActivityPage::refreshRelativeTimestamps);
    m_relativeTimeTimer.start();
}

// ── Sort helpers ─────────────────────────────────────────────────

void ActivityPage::updateSortIcon(QPushButton* btn, int state) {
    qreal dpr = devicePixelRatioF();
    // Always show both arrows — base in dim gray
    QPixmap pm = txTypeIcon("sort", 12, dpr, QColor(255, 255, 255, 100));
    // Overlay the active direction arrow in blue
    if (state == 1) {
        QPixmap overlay = txTypeIcon("sort-asc", 12, dpr, QColor("#60a5fa"));
        QPainter p(&pm);
        p.drawPixmap(0, 0, overlay);
    } else if (state == 2) {
        QPixmap overlay = txTypeIcon("sort-desc", 12, dpr, QColor("#60a5fa"));
        QPainter p(&pm);
        p.drawPixmap(0, 0, overlay);
    }
    btn->setIcon(QIcon(pm));
}

void ActivityPage::toggleSort(int column) {
    if (m_sortColumn == column) {
        // Cycle: asc → desc → none
        m_sortDirection = (m_sortDirection == 1) ? 2 : (m_sortDirection == 2) ? 0 : 1;
        if (m_sortDirection == 0) {
            m_sortColumn = -1;
        }
    } else {
        // Reset previous column icon and label
        if (m_sortColumn >= 0 && m_sortColumn < m_sortBtns.size()) {
            updateSortIcon(m_sortBtns[m_sortColumn], 0);
            m_sortLabels[m_sortColumn]->setProperty("activeSort", false);
            m_sortLabels[m_sortColumn]->style()->unpolish(m_sortLabels[m_sortColumn]);
            m_sortLabels[m_sortColumn]->style()->polish(m_sortLabels[m_sortColumn]);
        }
        m_sortColumn = column;
        m_sortDirection = 1; // start ascending
    }

    // Update icon and label for active column
    if (m_sortColumn >= 0 && m_sortColumn < m_sortBtns.size()) {
        updateSortIcon(m_sortBtns[m_sortColumn], m_sortDirection);
        m_sortLabels[m_sortColumn]->setProperty("activeSort", true);
        m_sortLabels[m_sortColumn]->style()->unpolish(m_sortLabels[m_sortColumn]);
        m_sortLabels[m_sortColumn]->style()->polish(m_sortLabels[m_sortColumn]);
    }
    // If cleared, reset the clicked button and label too
    if (m_sortDirection == 0 && column < m_sortBtns.size()) {
        updateSortIcon(m_sortBtns[column], 0);
        m_sortLabels[column]->setProperty("activeSort", false);
        m_sortLabels[column]->style()->unpolish(m_sortLabels[column]);
        m_sortLabels[column]->style()->polish(m_sortLabels[column]);
    }

    applySortToRows();
}

void ActivityPage::applySortToRows() {
    if (m_activityRows.isEmpty()) {
        return;
    }

    // Build a sorted index list
    QList<int> indices;
    for (int i = 0; i < m_activityRows.size(); ++i) {
        indices.append(i);
    }

    if (m_sortColumn >= 0 && m_sortDirection != 0) {
        bool asc = (m_sortDirection == 1);
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            const auto& ra = m_activityRows[a];
            const auto& rb = m_activityRows[b];
            int cmp = 0;
            switch (m_sortColumn) {
                case 0:
                    cmp = ra.signature.compare(rb.signature);
                    break; // SIGNATURE
                case 1:
                    cmp = (ra.blockTime < rb.blockTime)   ? -1
                          : (ra.blockTime > rb.blockTime) ? 1
                                                          : 0;
                    break; // TIME
                case 2:
                    cmp = ra.activityType.compare(rb.activityType);
                    break; // ACTION
                case 3:
                    cmp = ra.fromAddress.compare(rb.fromAddress);
                    break; // FROM
                case 4:
                    cmp = ra.toAddress.compare(rb.toAddress);
                    break; // TO
                case 5:
                    cmp = (ra.amount < rb.amount) ? -1 : (ra.amount > rb.amount) ? 1 : 0;
                    break; // AMOUNT
                case 6:
                    cmp = ra.tokenSymbol.compare(rb.tokenSymbol);
                    break; // TOKEN
                default:
                    break;
            }
            return asc ? (cmp < 0) : (cmp > 0);
        });
    }

    // Reorder widgets in the list layout
    for (int i = 0; i < indices.size(); ++i) {
        QWidget* w = m_activityRows[indices[i]].rowWidget;
        m_listLayout->removeWidget(w);
        m_listLayout->insertWidget(i, w);
    }
}

// ── Filter popup helpers ─────────────────────────────────────────

void ActivityPage::updateFilterBtnStyle(QPushButton* btn, bool active) {
    btn->setProperty("activeFilter", active);
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
    QColor tint = active ? QColor("#a78bfa") : QColor(255, 255, 255, 100);
    btn->setIcon(QIcon(txTypeIcon("filter", 12, devicePixelRatioF(), tint)));
}

void ActivityPage::showTextFilter(QPushButton* anchor, const QString& placeholder, QString& state) {
    QWidget* popup = new QWidget(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName("filterPopup");
    popup->setFixedWidth(240);

    QVBoxLayout* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    QLineEdit* input = new QLineEdit();
    input->setPlaceholderText(placeholder);
    input->setText(state);
    lay->addWidget(input);

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton(tr("Reset"));
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setProperty("uiRole", "reset");
    QPushButton* filterBtn = new QPushButton(tr("Filter"));
    filterBtn->setCursor(Qt::PointingHandCursor);
    filterBtn->setProperty("uiRole", "apply");
    btns->addWidget(resetBtn);
    btns->addStretch();
    btns->addWidget(filterBtn);
    lay->addLayout(btns);

    connect(resetBtn, &QPushButton::clicked, popup, [this, input, popup, &state, anchor]() {
        input->clear();
        state.clear();
        updateFilterBtnStyle(anchor, false);
        applyAllFilters();
        popup->close();
    });

    connect(filterBtn, &QPushButton::clicked, popup, [this, input, popup, &state, anchor]() {
        state = input->text().trimmed();
        updateFilterBtnStyle(anchor, !state.isEmpty());
        applyAllFilters();
        popup->close();
    });

    positionPopup(popup, anchor);
    popup->show();
    input->setFocus();
}

void ActivityPage::showTimeFilter(QPushButton* anchor) {
    QWidget* popup = new QWidget(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName("filterPopup");
    popup->setFixedWidth(280);

    QVBoxLayout* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    // ── Title ──
    QLabel* lbl = new QLabel(tr("Time Range"));
    lbl->setObjectName("activityPopupTitle");
    lay->addWidget(lbl);

    // ── From/To date input buttons (create early so presets can update them) ──
    QPushButton* fromBtn = new QPushButton(tr("Select start date"));
    fromBtn->setCursor(Qt::PointingHandCursor);
    fromBtn->setProperty("uiRole", "dateInput");
    fromBtn->setProperty("dateActive", false);

    QPushButton* toBtn = new QPushButton(tr("Select end date"));
    toBtn->setCursor(Qt::PointingHandCursor);
    toBtn->setProperty("uiRole", "dateInput");
    toBtn->setProperty("dateActive", false);

    // Pre-populate if dates already set
    if (m_timeFrom > 0) {
        fromBtn->setText(QDateTime::fromSecsSinceEpoch(m_timeFrom).date().toString("MMM d, yyyy"));
        fromBtn->setProperty("dateActive", true);
        fromBtn->style()->unpolish(fromBtn);
        fromBtn->style()->polish(fromBtn);
    }
    if (m_timeTo > 0) {
        toBtn->setText(QDateTime::fromSecsSinceEpoch(m_timeTo).date().toString("MMM d, yyyy"));
        toBtn->setProperty("dateActive", true);
        toBtn->style()->unpolish(toBtn);
        toBtn->style()->polish(toBtn);
    }

    // ── Preset buttons ──
    QHBoxLayout* presets = new QHBoxLayout();
    presets->setSpacing(6);

    auto makePreset = [&](const QString& text, qint64 secsAgo) {
        QPushButton* btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("uiRole", "preset");
        connect(btn, &QPushButton::clicked, popup, [this, secsAgo, anchor, fromBtn, toBtn]() {
            m_timeFrom = QDateTime::currentSecsSinceEpoch() - secsAgo;
            m_timeTo = 0;
            updateFilterBtnStyle(anchor, true);
            applyAllFilters();

            // Update date button labels
            fromBtn->setText(
                QDateTime::fromSecsSinceEpoch(m_timeFrom).date().toString("MMM d, yyyy"));
            fromBtn->setProperty("dateActive", true);
            fromBtn->style()->unpolish(fromBtn);
            fromBtn->style()->polish(fromBtn);
            toBtn->setText(tr("Select end date"));
            toBtn->setProperty("dateActive", false);
            toBtn->style()->unpolish(toBtn);
            toBtn->style()->polish(toBtn);
        });
        presets->addWidget(btn);
    };

    makePreset(tr("Last 1H"), 3600);
    makePreset(tr("Last 24H"), 86400);
    makePreset(tr("Last 7D"), 7LL * 86400);
    lay->addLayout(presets);

    // ── From date row ──
    QHBoxLayout* fromRow = new QHBoxLayout();
    fromRow->setSpacing(8);
    QLabel* fromLbl = new QLabel(tr("From:"));
    fromLbl->setObjectName("activityPopupSubLabel");
    fromLbl->setFixedWidth(36);
    fromRow->addWidget(fromLbl);
    fromRow->addWidget(fromBtn);
    lay->addLayout(fromRow);

    // ── To date row ──
    QHBoxLayout* toRow = new QHBoxLayout();
    toRow->setSpacing(8);
    QLabel* toLbl = new QLabel(tr("To:"));
    toLbl->setObjectName("activityPopupSubLabel");
    toLbl->setFixedWidth(36);
    toRow->addWidget(toLbl);
    toRow->addWidget(toBtn);
    lay->addLayout(toRow);

    // ── Helper to show calendar popup for a date button ──
    auto showCalendar = [this, anchor](QPushButton* dateBtn, bool isFrom) {
        QWidget* calPopup = new QWidget(nullptr, Qt::Popup);
        calPopup->setObjectName("activityTransparent");
        calPopup->setFixedSize(280, 260);

        QVBoxLayout* calLay = new QVBoxLayout(calPopup);
        calLay->setContentsMargins(0, 0, 0, 0);

        StyledCalendar* cal = new StyledCalendar();

        // Pre-select current date if set
        qint64 current = isFrom ? m_timeFrom : m_timeTo;
        if (current > 0) {
            cal->setSelectedDate(QDateTime::fromSecsSinceEpoch(current).date());
        }

        calLay->addWidget(cal);

        connect(cal, &QCalendarWidget::clicked, this,
                [this, calPopup, dateBtn, anchor, isFrom](const QDate& date) {
                    if (isFrom) {
                        m_timeFrom = QDateTime(date, QTime(0, 0, 0)).toSecsSinceEpoch();
                    } else {
                        m_timeTo = QDateTime(date, QTime(23, 59, 59)).toSecsSinceEpoch();
                    }
                    dateBtn->setText(date.toString("MMM d, yyyy"));
                    dateBtn->setProperty("dateActive", true);
                    dateBtn->style()->unpolish(dateBtn);
                    dateBtn->style()->polish(dateBtn);
                    updateFilterBtnStyle(anchor, m_timeFrom > 0 || m_timeTo > 0);
                    applyAllFilters();
                    calPopup->close();
                    calPopup->deleteLater();
                });

        QPoint pos = dateBtn->mapToGlobal(QPoint(0, dateBtn->height() + 2));
        calPopup->move(pos);
        calPopup->show();
    };

    connect(fromBtn, &QPushButton::clicked, this,
            [showCalendar, fromBtn]() { showCalendar(fromBtn, true); });
    connect(toBtn, &QPushButton::clicked, this,
            [showCalendar, toBtn]() { showCalendar(toBtn, false); });

    // ── Bottom buttons ──
    QHBoxLayout* bottom = new QHBoxLayout();

    QPushButton* resetBtn = new QPushButton(tr("Reset"));
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setProperty("uiRole", "reset");
    connect(resetBtn, &QPushButton::clicked, popup, [this, popup, anchor, fromBtn, toBtn]() {
        m_timeFrom = 0;
        m_timeTo = 0;
        fromBtn->setText(tr("Select start date"));
        fromBtn->setProperty("dateActive", false);
        fromBtn->style()->unpolish(fromBtn);
        fromBtn->style()->polish(fromBtn);
        toBtn->setText(tr("Select end date"));
        toBtn->setProperty("dateActive", false);
        toBtn->style()->unpolish(toBtn);
        toBtn->style()->polish(toBtn);
        updateFilterBtnStyle(anchor, false);
        applyAllFilters();
        popup->close();
    });

    QPushButton* filterBtn = new QPushButton(tr("Filter"));
    filterBtn->setCursor(Qt::PointingHandCursor);
    filterBtn->setProperty("uiRole", "apply");
    connect(filterBtn, &QPushButton::clicked, popup, [this, popup, anchor]() {
        updateFilterBtnStyle(anchor, m_timeFrom > 0 || m_timeTo > 0);
        applyAllFilters();
        popup->close();
    });

    bottom->addWidget(resetBtn);
    bottom->addStretch();
    bottom->addWidget(filterBtn);
    lay->addLayout(bottom);

    positionPopup(popup, anchor);
    popup->show();
}

void ActivityPage::showActionFilter(QPushButton* anchor) {
    QWidget* popup = new QWidget(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName("filterPopup");
    popup->setFixedWidth(220);

    QVBoxLayout* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(4);

    QList<StyledCheckbox*> boxes;
    for (const auto& t : m_handler.allActionTypes()) {
        StyledCheckbox* cb = new StyledCheckbox(m_handler.badgeText(t));
        cb->setProperty("actionType", t);
        cb->setChecked(m_actionFilter.contains(t));
        lay->addWidget(cb);
        boxes.append(cb);
    }

    QHBoxLayout* btns = new QHBoxLayout();
    btns->setContentsMargins(0, 6, 0, 0);
    QPushButton* resetBtn = new QPushButton(tr("Reset"));
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setProperty("uiRole", "reset");
    QPushButton* filterBtn = new QPushButton(tr("Filter"));
    filterBtn->setCursor(Qt::PointingHandCursor);
    filterBtn->setProperty("uiRole", "apply");
    btns->addWidget(resetBtn);
    btns->addStretch();
    btns->addWidget(filterBtn);
    lay->addLayout(btns);

    connect(resetBtn, &QPushButton::clicked, popup, [this, popup, anchor]() {
        m_actionFilter.clear();
        updateFilterBtnStyle(anchor, false);
        applyAllFilters();
        popup->close();
    });

    connect(filterBtn, &QPushButton::clicked, popup, [this, popup, boxes, anchor]() {
        m_actionFilter.clear();
        for (auto* cb : boxes) {
            if (cb->isChecked()) {
                m_actionFilter.insert(cb->property("actionType").toString());
            }
        }
        updateFilterBtnStyle(anchor, !m_actionFilter.isEmpty());
        applyAllFilters();
        popup->close();
    });

    positionPopup(popup, anchor);
    popup->show();
}

void ActivityPage::showAmountFilter(QPushButton* anchor) {
    QWidget* popup = new QWidget(this, Qt::Popup);
    popup->setObjectName("filterPopup");
    popup->setFixedWidth(240);

    QVBoxLayout* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    QLabel* lbl = new QLabel(tr("Amount Range"));
    lbl->setObjectName("activityPopupTitle");
    lay->addWidget(lbl);

    QLineEdit* minInput = new QLineEdit();
    minInput->setPlaceholderText(tr("Min"));
    if (m_amountMin >= 0) {
        minInput->setText(QString::number(m_amountMin));
    }
    lay->addWidget(minInput);

    QLineEdit* maxInput = new QLineEdit();
    maxInput->setPlaceholderText(tr("Max"));
    if (m_amountMax >= 0) {
        maxInput->setText(QString::number(m_amountMax));
    }
    lay->addWidget(maxInput);

    QHBoxLayout* btns = new QHBoxLayout();
    QPushButton* resetBtn = new QPushButton(tr("Reset"));
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setProperty("uiRole", "reset");
    QPushButton* filterBtn = new QPushButton(tr("Filter"));
    filterBtn->setCursor(Qt::PointingHandCursor);
    filterBtn->setProperty("uiRole", "apply");
    btns->addWidget(resetBtn);
    btns->addStretch();
    btns->addWidget(filterBtn);
    lay->addLayout(btns);

    connect(resetBtn, &QPushButton::clicked, popup, [this, popup, anchor]() {
        m_amountMin = -1;
        m_amountMax = -1;
        updateFilterBtnStyle(anchor, false);
        applyAllFilters();
        popup->close();
    });

    connect(filterBtn, &QPushButton::clicked, popup, [this, popup, minInput, maxInput, anchor]() {
        bool okMin = false;
        bool okMax = false;
        double mn = minInput->text().toDouble(&okMin);
        double mx = maxInput->text().toDouble(&okMax);
        m_amountMin = okMin && !minInput->text().isEmpty() ? mn : -1;
        m_amountMax = okMax && !maxInput->text().isEmpty() ? mx : -1;
        updateFilterBtnStyle(anchor, m_amountMin >= 0 || m_amountMax >= 0);
        applyAllFilters();
        popup->close();
    });

    positionPopup(popup, anchor);
    popup->show();
    minInput->setFocus();
}

void ActivityPage::setAvatarCache(AvatarCache* cache) { m_avatarCache = cache; }

ActivityFilters ActivityPage::currentFilters() const {
    ActivityFilters filters;
    filters.signature = m_sigFilter;
    filters.timeFrom = m_timeFrom;
    filters.timeTo = m_timeTo;
    filters.actionTypes = m_actionFilter;
    filters.fromAddress = m_fromFilter;
    filters.toAddress = m_toFilter;
    filters.amountMin = m_amountMin;
    filters.amountMax = m_amountMax;
    filters.token = m_tokenFilter;
    return filters;
}

// ── Refresh ──────────────────────────────────────────────────────

void ActivityPage::refresh(const QString& ownerAddress) {
    m_ownerAddress = ownerAddress;

    // Reset all filters
    m_sigFilter.clear();
    m_timeFrom = 0;
    m_timeTo = 0;
    m_actionFilter.clear();
    m_fromFilter.clear();
    m_toFilter.clear();
    m_amountMin = -1;
    m_amountMax = -1;
    m_tokenFilter.clear();

    // Reset filter button indicators
    QPushButton* allBtns[] = {m_sigFilterBtn, m_timeFilterBtn,   m_actionFilterBtn, m_fromFilterBtn,
                              m_toFilterBtn,  m_amountFilterBtn, m_tokenFilterBtn};
    for (auto* btn : allBtns) {
        updateFilterBtnStyle(btn, false);
    }

    // Reset sort to TIME descending (natural DB order)
    for (int i = 0; i < m_sortBtns.size(); ++i) {
        updateSortIcon(m_sortBtns[i], 0);
        m_sortLabels[i]->setProperty("activeSort", false);
        m_sortLabels[i]->style()->unpolish(m_sortLabels[i]);
        m_sortLabels[i]->style()->polish(m_sortLabels[i]);
    }
    m_sortColumn = 1;
    m_sortDirection = 2;
    updateSortIcon(m_sortBtns[1], 2);
    m_sortLabels[1]->setProperty("activeSort", true);
    m_sortLabels[1]->style()->unpolish(m_sortLabels[1]);
    m_sortLabels[1]->style()->polish(m_sortLabels[1]);

    m_currentPage = 0;
    m_unfilteredTotalRows = m_handler.totalRows(ownerAddress);
    m_totalRows = m_unfilteredTotalRows;
    m_titleLabel->setText(tr("Activity (%1)").arg(m_totalRows));
    loadPage();
}

void ActivityPage::refreshKeepingFilters() {
    // Update counts and reload current page without clearing filters
    m_unfilteredTotalRows = m_handler.totalRows(m_ownerAddress);
    const ActivityFilters filters = currentFilters();
    if (!m_handler.hasActiveFilter(filters)) {
        m_totalRows = m_unfilteredTotalRows;
    } else {
        m_totalRows = m_handler.filteredRows(m_ownerAddress, filters);
    }
    loadPage();

    if (m_handler.hasActiveFilter(filters)) {
        m_titleLabel->setText(
            tr("Activity (%1 of %2)").arg(m_totalRows).arg(m_unfilteredTotalRows));
    } else {
        m_titleLabel->setText(tr("Activity (%1)").arg(m_totalRows));
    }
}

void ActivityPage::setBackfillRunning(bool running) {
    m_syncBadge->setVisible(running);
    if (running) {
        if (!m_syncSpinTimer.isActive()) {
            m_syncSpinTimer.start();
        }
    } else {
        m_syncSpinTimer.stop();
    }
}

void ActivityPage::loadPage() {
    const ActivityFilters filters = currentFilters();
    const QList<TransactionRecord> txns =
        m_handler.loadPage(m_ownerAddress, filters, m_pageSize, m_currentPage);
    const QList<ActivityRowView> rows = m_handler.buildRows(txns);
    populateRows(rows);

    // Apply local sort if active (sort is still client-side on loaded rows)
    if (m_sortColumn >= 0 && m_sortDirection != 0) {
        applySortToRows();
    }

    // Update "Showing X-Y of Z"
    const int offset = m_currentPage * m_pageSize;
    if (m_totalRows > 0) {
        int from = offset + 1;
        int to = qMin(offset + static_cast<int>(rows.size()), m_totalRows);
        m_showingLabel->setText(tr("Showing %1-%2 of %3").arg(from).arg(to).arg(m_totalRows));
    } else {
        m_showingLabel->setText({});
    }

    // Update pagination button states
    int totalPages = qMax(1, (m_totalRows + m_pageSize - 1) / m_pageSize);
    m_firstPageBtn->setEnabled(m_currentPage > 0);
    m_prevPageBtn->setEnabled(m_currentPage > 0);
    m_nextPageBtn->setEnabled(m_currentPage < totalPages - 1);
    m_lastPageBtn->setEnabled(m_currentPage < totalPages - 1);
}

// ── Populate rows ────────────────────────────────────────────────

void ActivityPage::populateRows(const QList<ActivityRowView>& rows) {
    m_activityRows.clear();
    while (m_listLayout->count() > 0) {
        QLayoutItem* item = m_listLayout->takeAt(m_listLayout->count() - 1);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (rows.isEmpty()) {
        m_statusLabel->setText(tr("No transactions yet"));
        m_statusLabel->setVisible(true);
        m_headerRow->setVisible(false);
        return;
    }

    m_statusLabel->setVisible(false);
    m_headerRow->setVisible(true);

    qreal dpr = qApp->devicePixelRatio();

    for (const auto& data : rows) {
        // ── Build row ──
        QWidget* row = new QWidget();
        row->setObjectName("activityRow");
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setAttribute(Qt::WA_Hover, true);
        row->setCursor(Qt::PointingHandCursor);
        row->setFixedHeight(44);
        row->setProperty("failed", data.err);
        row->setProperty("groupHover", false);
        row->setProperty("signature", data.signature);
        row->installEventFilter(this);

        QHBoxLayout* rl = new QHBoxLayout(row);
        rl->setContentsMargins(14, 6, 14, 6);
        rl->setSpacing(8);

        // Col 1: Signature
        QLabel* sigLabel = new QLabel(data.signatureDisplay);
        sigLabel->setProperty("uiClass", data.err ? "activitySignatureError" : "activitySignature");
        sigLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        sigLabel->setToolTip(data.signature);
        rl->addWidget(sigLabel, 2);

        // Col 2: Time
        QLabel* timeLabel = new QLabel(formatRelativeTime(data.blockTime));
        timeLabel->setObjectName("activityTimeLabel");
        timeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        rl->addWidget(timeLabel, 2);

        // Col 3: Action badge
        QLabel* badge = new QLabel(data.badgeText);
        badge->setObjectName("activityActionBadge");
        badge->setProperty("activityKind", activityBadgeKind(data.activityType));
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedHeight(24);
        badge->setMinimumWidth(50);
        badge->setMaximumWidth(120);

        QWidget* badgeContainer = new QWidget();
        badgeContainer->setObjectName("activityTransparent");
        QHBoxLayout* badgeLay = new QHBoxLayout(badgeContainer);
        badgeLay->setContentsMargins(0, 0, 0, 0);
        badgeLay->setSpacing(0);
        badgeLay->addWidget(badge);
        badgeLay->addStretch();
        rl->addWidget(badgeContainer, 2);

        // Col 4: From
        QWidget* fromCol = new QWidget();
        fromCol->setObjectName("activityTransparent");
        QHBoxLayout* fromLay = new QHBoxLayout(fromCol);
        fromLay->setContentsMargins(0, 0, 0, 0);
        fromLay->setSpacing(4);
        QLabel* fromLabel = new QLabel(data.fromDisplay);
        fromLabel->setObjectName("activityAddressLabel");
        fromLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        fromLay->addWidget(fromLabel);
        if (!data.fromAddress.isEmpty()) {
            fromLabel->setToolTip(data.fromAddress);
            ContactResolver::resolveLabel(data.fromAddress, fromLabel, fromLay);
        }
        fromLay->addStretch();
        rl->addWidget(fromCol, 2);

        // Col 5: To
        QWidget* toCol = new QWidget();
        toCol->setObjectName("activityTransparent");
        QHBoxLayout* toLay = new QHBoxLayout(toCol);
        toLay->setContentsMargins(0, 0, 0, 0);
        toLay->setSpacing(4);
        QLabel* toLabel = new QLabel(data.toDisplay);
        toLabel->setObjectName("activityAddressLabel");
        toLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        toLay->addWidget(toLabel);
        if (!data.toAddress.isEmpty()) {
            toLabel->setToolTip(data.toAddress);
            ContactResolver::resolveLabel(data.toAddress, toLabel, toLay);
        }
        toLay->addStretch();
        rl->addWidget(toCol, 2);

        // Col 6: Amount (colored)
        QLabel* amountLabel = new QLabel(data.amountText);
        amountLabel->setObjectName("activityAmountLabel");
        amountLabel->setProperty("amountTone", amountToneForColor(data.amountColor));
        amountLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        amountLabel->setFixedWidth(140);
        rl->addWidget(amountLabel);

        // Col 7: Token (icon + symbol)
        QWidget* tokenCol = new QWidget();
        tokenCol->setObjectName("activityTransparent");
        QHBoxLayout* tokenLay = new QHBoxLayout(tokenCol);
        tokenLay->setContentsMargins(0, 0, 0, 0);
        tokenLay->setSpacing(4);

        // Try bundled icon first, then AvatarCache for on-chain logo
        QPixmap tokenPx;
        int iconSz = static_cast<int>(16 * dpr);
        if (!data.iconPath.isEmpty()) {
            tokenPx = QPixmap(data.iconPath);
        }
        if (tokenPx.isNull() && !data.logoUrl.isEmpty() && m_avatarCache) {
            tokenPx = m_avatarCache->get(data.logoUrl);
        }
        if (!tokenPx.isNull()) {
            tokenPx = AvatarCache::roundedRectClip(tokenPx, 16, dpr);
            QLabel* iconLbl = new QLabel();
            iconLbl->setPixmap(tokenPx);
            iconLbl->setFixedSize(16, 16);
            iconLbl->setObjectName("activityTransparent");
            tokenLay->addWidget(iconLbl, 0, Qt::AlignVCenter);
        }

        QLabel* tokenLabel = new QLabel(data.tokenSymbol);
        tokenLabel->setObjectName("activityTokenLabel");
        tokenLay->addWidget(tokenLabel, 0, Qt::AlignVCenter);
        tokenLay->addStretch();
        tokenCol->setFixedWidth(100);
        rl->addWidget(tokenCol);

        m_listLayout->addWidget(row);
        m_activityRows.append({row, timeLabel, data.blockTime, data.signature, data.activityType,
                               data.fromAddress, data.toAddress, data.amount, data.tokenSymbol});
    }
}

// ── Filter logic ─────────────────────────────────────────────────

void ActivityPage::applyAllFilters() {
    // Reset to first page and recount with the active filter
    m_currentPage = 0;
    const ActivityFilters filters = currentFilters();
    if (!m_handler.hasActiveFilter(filters)) {
        m_totalRows = m_unfilteredTotalRows;
    } else {
        m_totalRows = m_handler.filteredRows(m_ownerAddress, filters);
    }

    // Reload rows from DB with the filter applied
    loadPage();

    // Update title to reflect filtered count
    if (m_handler.hasActiveFilter(filters)) {
        m_titleLabel->setText(
            tr("Activity (%1 of %2)").arg(m_totalRows).arg(m_unfilteredTotalRows));
    } else {
        m_titleLabel->setText(tr("Activity (%1)").arg(m_totalRows));
    }

    rebuildChips();
}

// ── Filter chips ─────────────────────────────────────────────────

void ActivityPage::rebuildChips() {
    // Remove all existing chips (keep the trailing stretch)
    while (m_chipLayout->count() > 1) {
        QLayoutItem* item = m_chipLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    bool anyFilter = false;

    auto addChip = [&](const QString& text, const std::function<void()>& clearFn) {
        QPushButton* chip = new QPushButton(text + QString::fromUtf8("  \xc3\x97"));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setObjectName("activityChip");
        connect(chip, &QPushButton::clicked, this, [this, clearFn]() {
            clearFn();
            applyAllFilters();
        });
        m_chipLayout->insertWidget(m_chipLayout->count() - 1, chip);
        anyFilter = true;
    };

    // Signature chip
    if (!m_sigFilter.isEmpty()) {
        addChip(tr("Sig: %1").arg(m_handler.truncateAddr(m_sigFilter)), [this]() {
            m_sigFilter.clear();
            updateFilterBtnStyle(m_sigFilterBtn, false);
        });
    }

    // Time chip
    if (m_timeFrom > 0 || m_timeTo > 0) {
        QString label;

        if (m_timeFrom > 0 && m_timeTo == 0) {
            // Preset-style or from-only
            qint64 secsAgo = QDateTime::currentSecsSinceEpoch() - m_timeFrom;
            if (secsAgo <= 3700) {
                label = tr("Last 1H");
            } else if (secsAgo <= 87000) {
                label = tr("Last 24H");
            } else if (secsAgo <= 605000) {
                label = tr("Last 7D");
            } else {
                label = tr("From %1").arg(
                    QDateTime::fromSecsSinceEpoch(m_timeFrom).date().toString("MMM d"));
            }
        } else if (m_timeFrom > 0 && m_timeTo > 0) {
            label = tr("%1 - %2").arg(
                QDateTime::fromSecsSinceEpoch(m_timeFrom).date().toString("MMM d"),
                QDateTime::fromSecsSinceEpoch(m_timeTo).date().toString("MMM d"));
        } else {
            // To only
            label = tr("Until %1")
                        .arg(QDateTime::fromSecsSinceEpoch(m_timeTo).date().toString("MMM d"));
        }

        addChip(tr("Time: %1").arg(label), [this]() {
            m_timeFrom = 0;
            m_timeTo = 0;
            updateFilterBtnStyle(m_timeFilterBtn, false);
        });
    }

    // Action chip
    if (!m_actionFilter.isEmpty()) {
        QStringList names;
        for (const auto& a : m_actionFilter) {
            names.append(m_handler.badgeText(a));
        }
        names.sort();
        addChip(tr("Action: %1").arg(names.join(", ")), [this]() {
            m_actionFilter.clear();
            updateFilterBtnStyle(m_actionFilterBtn, false);
        });
    }

    // From chip
    if (!m_fromFilter.isEmpty()) {
        addChip(tr("From: %1").arg(m_handler.truncateAddr(m_fromFilter)), [this]() {
            m_fromFilter.clear();
            updateFilterBtnStyle(m_fromFilterBtn, false);
        });
    }

    // To chip
    if (!m_toFilter.isEmpty()) {
        addChip(tr("To: %1").arg(m_handler.truncateAddr(m_toFilter)), [this]() {
            m_toFilter.clear();
            updateFilterBtnStyle(m_toFilterBtn, false);
        });
    }

    // Amount chip
    if (m_amountMin >= 0 || m_amountMax >= 0) {
        QString label;
        if (m_amountMin >= 0 && m_amountMax >= 0) {
            label =
                tr("Amount: %1 - %2")
                    .arg(m_handler.formatNumber(m_amountMin), m_handler.formatNumber(m_amountMax));
        } else if (m_amountMin >= 0) {
            label = tr("Amount: %1+").arg(m_handler.formatNumber(m_amountMin));
        } else {
            label = tr("Amount: -%1").arg(m_handler.formatNumber(m_amountMax));
        }
        addChip(label, [this]() {
            m_amountMin = -1;
            m_amountMax = -1;
            updateFilterBtnStyle(m_amountFilterBtn, false);
        });
    }

    // Token chip
    if (!m_tokenFilter.isEmpty()) {
        addChip(tr("Token: %1").arg(m_tokenFilter), [this]() {
            m_tokenFilter.clear();
            updateFilterBtnStyle(m_tokenFilterBtn, false);
        });
    }

    // "Clear Filters" chip (first position, only if any filter is active)
    if (anyFilter) {
        QPushButton* clearAll =
            new QPushButton(tr("Clear Filters") + QString::fromUtf8("  \xc3\x97"));
        clearAll->setCursor(Qt::PointingHandCursor);
        clearAll->setObjectName("activityClearChip");
        connect(clearAll, &QPushButton::clicked, this, &ActivityPage::clearAllFilters);
        m_chipLayout->insertWidget(0, clearAll);
    }

    m_chipBar->setVisible(anyFilter);
}

void ActivityPage::clearAllFilters() {
    m_sigFilter.clear();
    m_timeFrom = 0;
    m_timeTo = 0;
    m_actionFilter.clear();
    m_fromFilter.clear();
    m_toFilter.clear();
    m_amountMin = -1;
    m_amountMax = -1;
    m_tokenFilter.clear();

    QPushButton* allBtns[] = {m_sigFilterBtn, m_timeFilterBtn,   m_actionFilterBtn, m_fromFilterBtn,
                              m_toFilterBtn,  m_amountFilterBtn, m_tokenFilterBtn};
    for (auto* btn : allBtns) {
        updateFilterBtnStyle(btn, false);
    }

    applyAllFilters();
}

// ── Relative timestamps ──────────────────────────────────────────

void ActivityPage::refreshRelativeTimestamps() {
    for (const auto& entry : m_activityRows) {
        entry.timeLabel->setText(formatRelativeTime(entry.blockTime));
    }
}

// ── Event filter ─────────────────────────────────────────────────

bool ActivityPage::eventFilter(QObject* obj, QEvent* event) {
    // Paint the spinning arc on the sync spinner widget
    if (obj == m_syncSpinner && event->type() == QEvent::Paint) {
        QPainter p(m_syncSpinner);
        p.setRenderHint(QPainter::Antialiasing);
        QColor arcColor(253, 214, 99, 200);
        QPen pen(arcColor, 2.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        QRectF r(2.0, 2.0, m_syncSpinner->width() - 4.0, m_syncSpinner->height() - 4.0);
        // Draw a 270° arc that rotates
        int startAngle = m_syncAngle * 16; // Qt uses 1/16th of a degree
        int spanAngle = 270 * 16;
        p.drawArc(r, startAngle, spanAngle);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QString sig = obj->property("signature").toString();
        if (!sig.isEmpty()) {
            emit transactionClicked(sig);
            return true;
        }
    }

    // Hover: highlight all rows sharing the same signature
    if (event->type() == QEvent::HoverEnter) {
        QString sig = obj->property("signature").toString();
        if (!sig.isEmpty()) {
            for (const auto& r : m_activityRows) {
                if (r.signature == sig && r.rowWidget != obj) {
                    r.rowWidget->setProperty("groupHover", true);
                    r.rowWidget->style()->unpolish(r.rowWidget);
                    r.rowWidget->style()->polish(r.rowWidget);
                }
            }
        }
    } else if (event->type() == QEvent::HoverLeave) {
        QString sig = obj->property("signature").toString();
        if (!sig.isEmpty()) {
            for (const auto& r : m_activityRows) {
                if (r.signature == sig && r.rowWidget != obj) {
                    r.rowWidget->setProperty("groupHover", false);
                    r.rowWidget->style()->unpolish(r.rowWidget);
                    r.rowWidget->style()->polish(r.rowWidget);
                }
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
