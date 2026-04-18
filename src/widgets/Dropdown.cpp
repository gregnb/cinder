#include "widgets/Dropdown.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

Dropdown::Dropdown(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Button (text + chevron) ──────────────────────────────
    m_button = new QPushButton();
    m_button->setObjectName("dropdownBtn");
    m_button->setMinimumHeight(40);
    m_button->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* btnLayout = new QHBoxLayout(m_button);
    btnLayout->setContentsMargins(14, 0, 14, 0);
    btnLayout->setSpacing(8);

    m_label = new QLabel();
    m_label->setObjectName("dropdownLabel");
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_label, 1);

    m_chevron = new QLabel();
    m_chevron->setObjectName("dropdownChevron");
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

    // ── Popup list (hidden, positioned as overlay) ──────────
    m_popup = new QWidget();
    m_popup->hide();

    QVBoxLayout* popLayout = new QVBoxLayout(m_popup);
    popLayout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget();
    m_list->setObjectName("dropdownList");
    m_list->setCursor(Qt::PointingHandCursor);
    m_list->setMouseTracking(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        setCurrentItem(item->text());
        m_list->clearSelection();
        hideDropdown();
        emit itemSelected(item->text());
    });

    popLayout->addWidget(m_list);

    qApp->installEventFilter(this);
}

Dropdown::~Dropdown() {
    delete m_popup; // m_popup is created unparented for overlay positioning
}

void Dropdown::addItem(const QString& text) { m_list->addItem(text); }

void Dropdown::addItem(const QIcon& icon, const QString& text) {
    m_list->setIconSize(QSize(16, 16));
    m_list->addItem(new QListWidgetItem(icon, text));
}

void Dropdown::clear() {
    m_list->clear();
    m_label->clear();
}

void Dropdown::setCurrentItem(const QString& text) { m_label->setText(text); }

QString Dropdown::currentText() const { return m_label->text(); }

void Dropdown::showDropdown() {
    QWidget* container = parentWidget();
    if (!container) {
        return;
    }

    // Walk up to find scrollable content widget
    QWidget* scrollContent = container;
    while (scrollContent->parentWidget() &&
           qobject_cast<QScrollArea*>(scrollContent->parentWidget()) == nullptr) {
        scrollContent = scrollContent->parentWidget();
    }
    if (scrollContent->parentWidget()) {
        scrollContent = scrollContent->parentWidget(); // the QScrollArea itself
    }

    // Use the scroll content widget as popup parent for correct overlay
    QWidget* popParent = container;
    // Walk up to find a suitable parent (the scroll area's viewport or content)
    for (QWidget* w = parentWidget(); w; w = w->parentWidget()) {
        popParent = w;
        if (w->inherits("QScrollArea")) {
            break;
        }
    }

    if (m_popup->parentWidget() != popParent) {
        m_popup->setParent(popParent);
    }

    // Measure actual row heights now that styles are applied
    int contentH = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        contentH += m_list->sizeHintForRow(i) + m_list->spacing();
    }
    contentH += 12; // list padding
    int listH = qMin(contentH, 260);
    m_list->setVerticalScrollBarPolicy(contentH > 260 ? Qt::ScrollBarAsNeeded
                                                      : Qt::ScrollBarAlwaysOff);

    // Position below by default; flip above if not enough room below
    QPoint globalBtn = m_button->mapToGlobal(QPoint(0, 0));
    QScreen* screen = m_button->screen();
    int screenBottom = screen ? screen->availableGeometry().bottom() : 9999;
    bool openAbove = (globalBtn.y() + m_button->height() + 4 + listH > screenBottom);

    QPoint pos;
    if (openAbove) {
        pos = m_button->mapTo(popParent, QPoint(0, -listH - 4));
    } else {
        pos = m_button->mapTo(popParent, QPoint(0, m_button->height() + 4));
    }
    m_popup->setGeometry(pos.x(), pos.y(), m_button->width(), listH);
    m_popup->raise();
    m_popup->show();
    updateChevron(true);
}

void Dropdown::hideDropdown() {
    m_popup->hide();
    m_button->clearFocus();
    updateChevron(false);
}

void Dropdown::updateChevron(bool open) {
    qreal dpr = qApp->devicePixelRatio();
    QString path = open ? ":/icons/ui/chevron-up.png" : ":/icons/ui/chevron-down.png";
    QPixmap px(path);
    px = px.scaled(16 * dpr, 16 * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    px.setDevicePixelRatio(dpr);
    m_chevron->setPixmap(px);
}

bool Dropdown::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress && m_popup->isVisible()) {
        auto* me = static_cast<QMouseEvent*>(event);
        QPoint globalPos = me->globalPosition().toPoint();

        QRect btnRect(m_button->mapToGlobal(QPoint(0, 0)), m_button->size());
        QRect popRect(m_popup->mapToGlobal(QPoint(0, 0)), m_popup->size());

        if (!btnRect.contains(globalPos) && !popRect.contains(globalPos)) {
            hideDropdown();
        }
    }
    return QWidget::eventFilter(obj, event);
}
