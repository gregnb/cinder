#include "widgets/TokenDropdown.h"
#include "services/AvatarCache.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <algorithm>

// ── Constants ────────────────────────────────────────────────────

static const int kMaxPopupHeight = 300;
static const int kRowHeight = 44;
static const int kSearchHeight = 36;
static const int kPopupMargin = 6;  // top + bottom of popup layout
static const int kPopupSpacing = 4; // between search and list
static const int kListPadding = 14; // QSS padding (6*2) + border (1*2)
static const int kDividerHeight = 16;

// ── Internal data ────────────────────────────────────────────────

static const QString kDividerMarker = QStringLiteral("__divider__");

struct TokenData {
    QString iconPath;
    QString displayName;
    QString balance;
    bool isDivider() const { return iconPath == kDividerMarker; }
    bool hasRealIcon() const { return iconPath.startsWith(":/") || iconPath.startsWith("http"); }
};

// ── Model ────────────────────────────────────────────────────────

class TokenListModel : public QAbstractListModel {
  public:
    enum Role { IconPathRole = Qt::UserRole, DisplayNameRole, BalanceRole };

    explicit TokenListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : m_tokens.size();
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (index.isValid() && index.row() < m_tokens.size() && m_tokens[index.row()].isDivider()) {
            return Qt::NoItemFlags; // divider: not selectable, not enabled
        }
        return QAbstractListModel::flags(index);
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= m_tokens.size()) {
            return {};
        }
        const auto& t = m_tokens[index.row()];
        switch (role) {
            case Qt::DisplayRole:
            case DisplayNameRole:
                return t.displayName;
            case IconPathRole:
                return t.iconPath;
            case BalanceRole:
                return t.balance;
            default:
                return {};
        }
    }

    void addToken(const QString& iconPath, const QString& displayName, const QString& balance) {
        int row = m_tokens.size();
        beginInsertRows({}, row, row);
        m_tokens.append({iconPath, displayName, balance});
        endInsertRows();
    }

    void clearAll() {
        beginResetModel();
        m_tokens.clear();
        endResetModel();
    }

    void sort(TokenDropdown::SortOrder order) {
        if (order == TokenDropdown::NoSort || m_tokens.size() <= 1) {
            return;
        }

        // Remove any existing divider
        m_tokens.erase(std::remove_if(m_tokens.begin(), m_tokens.end(),
                                      [](const TokenData& t) { return t.isDivider(); }),
                       m_tokens.end());

        // Partition into known (real icon) and unknown (grey placeholder)
        QVector<TokenData> known, unknown;
        for (const auto& t : m_tokens) {
            if (t.hasRealIcon()) {
                known.append(t);
            } else {
                unknown.append(t);
            }
        }

        auto cmp = [order](const TokenData& a, const TokenData& b) {
            int c = a.displayName.compare(b.displayName, Qt::CaseInsensitive);
            return order == TokenDropdown::AlphabeticalAsc ? c < 0 : c > 0;
        };
        std::sort(known.begin(), known.end(), cmp);
        std::sort(unknown.begin(), unknown.end(), cmp);

        beginResetModel();
        m_tokens.clear();
        m_tokens.append(known);
        if (!known.isEmpty() && !unknown.isEmpty()) {
            m_tokens.append({kDividerMarker, {}, {}});
        }
        m_tokens.append(unknown);
        endResetModel();
    }

    const QVector<TokenData>& tokens() const { return m_tokens; }

  private:
    QVector<TokenData> m_tokens;
};

// ── Filter proxy ─────────────────────────────────────────────────

class TokenFilterProxy : public QSortFilterProxyModel {
  public:
    explicit TokenFilterProxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setSearchText(const QString& text) {
        beginFilterChange();
        m_search = text.toLower();
        endFilterChange();
    }

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
        QString iconPath = idx.data(TokenListModel::IconPathRole).toString();

        // Hide divider when searching
        if (iconPath == kDividerMarker) {
            return m_search.isEmpty();
        }

        if (m_search.isEmpty()) {
            return true;
        }
        QString name = idx.data(TokenListModel::DisplayNameRole).toString().toLower();
        return name.contains(m_search);
    }

  private:
    QString m_search;
};

// ── Delegate ─────────────────────────────────────────────────────

class TokenItemDelegate : public QStyledItemDelegate {
  public:
    explicit TokenItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setAvatarCache(AvatarCache* cache) { m_avatarCache = cache; }

    QSize sizeHint(const QStyleOptionViewItem& /*option*/,
                   const QModelIndex& index) const override {
        if (index.data(TokenListModel::IconPathRole).toString() == kDividerMarker) {
            return {0, kDividerHeight};
        }
        return {0, kRowHeight};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QRect r = option.rect;

        // Divider — thin horizontal line
        QString iconPath = index.data(TokenListModel::IconPathRole).toString();
        if (iconPath == kDividerMarker) {
            int y = r.top() + r.height() / 2;
            painter->setPen(QPen(QColor(255, 255, 255, 180), 1));
            painter->drawLine(r.left() + 14, y, r.right() - 14, y);
            painter->restore();
            return;
        }

        // Hover / selection background
        if (option.state & QStyle::State_Selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#2a3a5a"));
            painter->drawRoundedRect(r.adjusted(2, 1, -2, -1), 6, 6);
        } else if (option.state & QStyle::State_MouseOver) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#253050"));
            painter->drawRoundedRect(r.adjusted(2, 1, -2, -1), 6, 6);
        }

        int x = r.left() + 14;
        int centerY = r.top() + r.height() / 2;

        // Icon (24x24)
        QPixmap px = resolveIcon(iconPath, painter->device()->devicePixelRatioF());
        if (!px.isNull()) {
            int iconY = centerY - 12;
            painter->drawPixmap(x, iconY, 24, 24, px);
        }
        x += 24 + 10;

        // Balance (measure first so name can fill remaining space)
        QString balance = index.data(TokenListModel::BalanceRole).toString();
        QFont balFont = option.font;
        balFont.setPixelSize(13);
        QFontMetrics balFm(balFont);
        int balW = balFm.horizontalAdvance(balance);
        int balX = r.right() - 14 - balW;

        // Name
        QString name = index.data(TokenListModel::DisplayNameRole).toString();
        QFont nameFont = option.font;
        nameFont.setPixelSize(14);
        nameFont.setWeight(QFont::Medium);
        QFontMetrics nameFm(nameFont);
        int nameMaxW = balX - x - 10;
        QString elidedName = nameFm.elidedText(name, Qt::ElideRight, nameMaxW);

        painter->setFont(nameFont);
        painter->setPen(Qt::white);
        painter->drawText(x, centerY - nameFm.height() / 2, nameMaxW, nameFm.height(),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedName);

        // Balance
        painter->setFont(balFont);
        painter->setPen(QColor(255, 255, 255, 128));
        painter->drawText(balX, centerY - balFm.height() / 2, balW, balFm.height(),
                          Qt::AlignRight | Qt::AlignVCenter, balance);

        painter->restore();
    }

  private:
    AvatarCache* m_avatarCache = nullptr;

    QPixmap resolveIcon(const QString& iconPath, qreal dpr) const {
        constexpr int kIconSize = 24;

        // Qt resource path — load directly with pixmap cache
        if (iconPath.startsWith(":/")) {
            QString key =
                QStringLiteral("tdd_%1_%2").arg(iconPath).arg(static_cast<int>(dpr * 100));
            QPixmap px;
            if (!QPixmapCache::find(key, &px)) {
                QPixmap raw(iconPath);
                if (!raw.isNull()) {
                    px = AvatarCache::roundedRectClip(raw, kIconSize, dpr);
                }
                QPixmapCache::insert(key, px);
            }
            return px;
        }

        // URL — use AvatarCache (returns null if not yet downloaded; avatarReady triggers repaint)
        if (m_avatarCache && !iconPath.isEmpty()) {
            QPixmap raw = m_avatarCache->get(iconPath);
            if (!raw.isNull()) {
                return AvatarCache::roundedRectClip(raw, kIconSize, dpr);
            }
        }

        // Fallback: pre-built grey Solana icon
        QString key = QStringLiteral("tdd_grey_%1").arg(static_cast<int>(dpr * 100));
        QPixmap grey;
        if (!QPixmapCache::find(key, &grey)) {
            QPixmap raw(":/icons/tokens/sol-grey.png");
            if (!raw.isNull()) {
                grey = AvatarCache::roundedRectClip(raw, kIconSize, dpr);
            }
            QPixmapCache::insert(key, grey);
        }
        return grey;
    }
};

// ── TokenDropdown ────────────────────────────────────────────────

TokenDropdown::TokenDropdown(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Button (icon + name + balance + chevron) ──
    m_button = new QPushButton();
    m_button->setObjectName("tokenDropdownBtn");
    m_button->setMinimumHeight(48);
    m_button->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* btnLayout = new QHBoxLayout(m_button);
    btnLayout->setContentsMargins(14, 0, 14, 0);
    btnLayout->setSpacing(10);

    m_icon = new QLabel();
    m_icon->setObjectName("tokenDropdownIcon");
    m_icon->setFixedSize(28, 28);
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_icon);

    m_name = new QLabel();
    m_name->setObjectName("tokenDropdownName");
    m_name->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_name, 1);

    m_balance = new QLabel();
    m_balance->setObjectName("tokenDropdownBalance");
    m_balance->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_balance);

    m_chevron = new QLabel();
    m_chevron->setObjectName("tokenDropdownChevron");
    m_chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
    updateChevron(false);
    btnLayout->addWidget(m_chevron);

    connect(m_button, &QPushButton::clicked, this, [this]() {
        if (m_popup->isVisible()) {
            hideDropdown();
        } else {
            showDropdown();
        }
    });
    outer->addWidget(m_button);

    // ── Model / proxy / delegate ──
    m_model = new TokenListModel(this);
    auto* proxy = new TokenFilterProxy(this);
    proxy->setSourceModel(m_model);
    m_proxy = proxy;

    // ── Popup (hidden, overlay-positioned) ──
    m_popup = new QWidget(); // reparented in showDropdown()
    m_popup->hide();

    QVBoxLayout* popLayout = new QVBoxLayout(m_popup);
    popLayout->setContentsMargins(kPopupMargin, kPopupMargin, kPopupMargin, kPopupMargin);
    popLayout->setSpacing(kPopupSpacing);

    // Search input
    m_searchInput = new QLineEdit();
    m_searchInput->setObjectName("tokenDropdownSearchInput");
    m_searchInput->setPlaceholderText("Search tokens...");
    m_searchInput->setFixedHeight(kSearchHeight);
    popLayout->addWidget(m_searchInput);

    connect(m_searchInput, &QLineEdit::textChanged, this, [this](const QString& text) {
        static_cast<TokenFilterProxy*>(m_proxy)->setSearchText(text);
        bool empty = m_proxy->rowCount() == 0;
        m_emptyLabel->setVisible(empty);
        m_listView->setVisible(!empty);
        recalcPopupSize();
    });

    // List view
    m_listView = new QListView();
    m_listView->setObjectName("tokenDropdownList");
    m_listView->setCursor(Qt::PointingHandCursor);
    m_listView->setMouseTracking(true);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setUniformItemSizes(false); // divider rows are shorter
    m_listView->setModel(m_proxy);
    m_listView->setItemDelegate(new TokenItemDelegate(m_listView));
    popLayout->addWidget(m_listView);

    // "No Results Found" label (hidden by default)
    m_emptyLabel = new QLabel(tr("No Results Found"));
    m_emptyLabel->setObjectName("tokenDropdownEmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFixedHeight(kRowHeight * 2);
    m_emptyLabel->hide();
    popLayout->addWidget(m_emptyLabel);

    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& proxyIndex) {
        QString iconPath = proxyIndex.data(TokenListModel::IconPathRole).toString();
        if (iconPath == kDividerMarker) {
            return; // dividers are not clickable
        }
        QString name = proxyIndex.data(TokenListModel::DisplayNameRole).toString();
        QString balance = proxyIndex.data(TokenListModel::BalanceRole).toString();
        setCurrentToken(iconPath, name, balance);
        hideDropdown();
        emit tokenSelected(iconPath, name);
    });

    // Close dropdown when user clicks outside
    qApp->installEventFilter(this);
}

TokenDropdown::~TokenDropdown() {
    delete m_popup; // created unparented for overlay positioning
}

// ── Public API ───────────────────────────────────────────────────

void TokenDropdown::addToken(const QString& iconPath, const QString& displayName,
                             const QString& balance) {
    static_cast<TokenListModel*>(m_model)->addToken(iconPath, displayName, balance);
}

void TokenDropdown::clear() { static_cast<TokenListModel*>(m_model)->clearAll(); }

void TokenDropdown::sortItems(SortOrder order) {
    static_cast<TokenListModel*>(m_model)->sort(order);
}

void TokenDropdown::setAvatarCache(AvatarCache* cache) {
    m_avatarCache = cache;
    auto* delegate = static_cast<TokenItemDelegate*>(m_listView->itemDelegate());
    delegate->setAvatarCache(cache);

    // Repaint list when an avatar finishes downloading
    connect(cache, &AvatarCache::avatarReady, this, [this]() {
        if (m_listView->isVisible()) {
            m_listView->viewport()->update();
        }
    });
}

bool TokenDropdown::selectByIcon(const QString& iconPath) {
    const auto& tokens = static_cast<TokenListModel*>(m_model)->tokens();
    for (const auto& t : tokens) {
        if (t.iconPath == iconPath) {
            setCurrentToken(t.iconPath, t.displayName, t.balance);
            return true;
        }
    }
    return false;
}

void TokenDropdown::setCurrentToken(const QString& iconPath, const QString& displayName,
                                    const QString& balance) {
    m_currentIconPath = iconPath;
    m_name->setText(displayName);
    m_balance->setText(balance);

    constexpr int kSelectedIconSize = 28;
    qreal dpr = qApp->devicePixelRatio();
    QPixmap raw;

    if (iconPath.startsWith(":/")) {
        raw = QPixmap(iconPath);
    } else if (m_avatarCache && !iconPath.isEmpty()) {
        raw = m_avatarCache->get(iconPath);
    }

    if (raw.isNull()) {
        raw = QPixmap(":/icons/tokens/sol-grey.png");
    }

    QPixmap px =
        raw.isNull() ? QPixmap() : AvatarCache::roundedRectClip(raw, kSelectedIconSize, dpr);
    m_icon->setPixmap(px);
}

QString TokenDropdown::currentText() const { return m_name->text(); }

QString TokenDropdown::currentIconPath() const { return m_currentIconPath; }

QString TokenDropdown::currentBalanceText() const { return m_balance->text(); }

// ── Popup management ─────────────────────────────────────────────

static int calcListContentH(QSortFilterProxyModel* proxy) {
    int rows = proxy->rowCount();
    if (rows == 0) {
        return kRowHeight * 2; // "No Results Found"
    }
    int h = kListPadding;
    for (int i = 0; i < rows; ++i) {
        QString ip = proxy->index(i, 0).data(TokenListModel::IconPathRole).toString();
        h += (ip == kDividerMarker) ? kDividerHeight : kRowHeight;
    }
    return h;
}

void TokenDropdown::recalcPopupSize() {
    int contentH = calcListContentH(static_cast<QSortFilterProxyModel*>(m_proxy));
    int totalH = kSearchHeight + kPopupSpacing + contentH + kPopupMargin * 2;
    if (totalH > kMaxPopupHeight) {
        totalH = kMaxPopupHeight;
    }

    int listH = totalH - kSearchHeight - kPopupSpacing - kPopupMargin * 2;
    if (listH < kRowHeight) {
        listH = kRowHeight;
    }
    m_listView->setFixedHeight(listH);

    // Resize popup in place (keep x, y, width)
    QRect geo = m_popup->geometry();
    m_popup->setGeometry(geo.x(), geo.y(), geo.width(), totalH);
}

void TokenDropdown::showDropdown() {
    QWidget* container = parentWidget();
    if (!container) {
        return;
    }

    if (m_popup->parentWidget() != container) {
        m_popup->setParent(container);
    }

    // Reset search
    m_searchInput->clear();

    // Calculate size
    int listContentH = calcListContentH(static_cast<QSortFilterProxyModel*>(m_proxy));
    int totalH = kSearchHeight + kPopupSpacing + listContentH + kPopupMargin * 2;
    if (totalH > kMaxPopupHeight) {
        totalH = kMaxPopupHeight;
    }

    int listH = totalH - kSearchHeight - kPopupSpacing - kPopupMargin * 2;
    if (listH < kRowHeight) {
        listH = kRowHeight;
    }
    m_listView->setFixedHeight(listH);

    // Position below button, flip above if overflows screen
    QPoint globalBtn = m_button->mapToGlobal(QPoint(0, 0));
    QScreen* screen = m_button->screen();
    int screenBottom = screen ? screen->availableGeometry().bottom() : 9999;
    bool openAbove = (globalBtn.y() + m_button->height() + 4 + totalH > screenBottom);

    QPoint pos;
    if (openAbove) {
        pos = mapTo(container, QPoint(0, -totalH - 4));
    } else {
        pos = mapTo(container, QPoint(0, m_button->height() + 4));
    }

    m_popup->setGeometry(pos.x(), pos.y(), width(), totalH);
    m_popup->raise();
    m_popup->show();
    m_searchInput->setFocus();
    updateChevron(true);
}

void TokenDropdown::hideDropdown() {
    m_popup->hide();
    m_searchInput->clear();
    m_listView->clearSelection();
    m_button->clearFocus();
    updateChevron(false);
}

void TokenDropdown::updateChevron(bool open) {
    qreal dpr = qApp->devicePixelRatio();
    QString path = open ? ":/icons/ui/chevron-up.png" : ":/icons/ui/chevron-down.png";
    QPixmap px(path);
    int sz = qRound(20 * dpr);
    px = px.scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    px.setDevicePixelRatio(dpr);
    m_chevron->setPixmap(px);
}

bool TokenDropdown::eventFilter(QObject* obj, QEvent* event) {
    if (m_popup->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint globalPos = me->globalPosition().toPoint();

            QRect btnRect(m_button->mapToGlobal(QPoint(0, 0)), m_button->size());
            QRect popRect(m_popup->mapToGlobal(QPoint(0, 0)), m_popup->size());

            if (!btnRect.contains(globalPos) && !popRect.contains(globalPos)) {
                hideDropdown();
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                hideDropdown();
                return true;
            }
            if (ke->key() == Qt::Key_Down && m_searchInput->hasFocus()) {
                m_listView->setFocus();
                if (m_proxy->rowCount() > 0) {
                    m_listView->setCurrentIndex(m_proxy->index(0, 0));
                }
                return true;
            }
            if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
                m_listView->hasFocus()) {
                QModelIndex idx = m_listView->currentIndex();
                if (idx.isValid()) {
                    emit m_listView->clicked(idx);
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
