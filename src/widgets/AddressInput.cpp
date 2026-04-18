#include "AddressInput.h"
#include "StyledLineEdit.h"
#include "db/ContactDb.h"
#include <QApplication>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QScrollArea>
#include <QVBoxLayout>

AddressInput::AddressInput(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Line edit (looks like #txFormInput) ────────────────
    m_input = new StyledLineEdit();
    m_input->setPlaceholderText(tr("Enter wallet address or select contact..."));
    m_input->setMinimumHeight(44);

    QPalette pal = m_input->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_input->setPalette(pal);

    connect(m_input, &QLineEdit::textChanged, this, [this](const QString& text) {
        filterContacts(text);
        emit addressChanged(text);
    });

    m_input->installEventFilter(this);
    outer->addWidget(m_input);

    // ── Popup list (hidden, positioned as overlay) ─────────
    m_popup = new QWidget();
    m_popup->hide();

    QVBoxLayout* popLayout = new QVBoxLayout(m_popup);
    popLayout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget();
    m_list->setObjectName("addressInputList");
    m_list->setCursor(Qt::PointingHandCursor);
    m_list->setMouseTracking(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_list, &QListWidget::itemClicked, this, &AddressInput::onItemClicked);

    popLayout->addWidget(m_list);

    // Close popup on outside click
    qApp->installEventFilter(this);
}

QString AddressInput::address() const { return m_input->text(); }

void AddressInput::setAddress(const QString& address) { m_input->setText(address); }

void AddressInput::loadContacts() {
    m_contacts.clear();
    auto rows = ContactDb::getAllRecords();
    for (const auto& row : rows) {
        m_contacts.append({row.name, row.address});
    }
}

void AddressInput::filterContacts(const QString& text) {
    if (!m_popup->isVisible()) {
        return;
    }

    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (text.isEmpty()) {
            item->setHidden(false);
        } else {
            QString name = item->data(Qt::UserRole + 1).toString();
            QString addr = item->data(Qt::UserRole).toString();
            bool matches = name.contains(text, Qt::CaseInsensitive) ||
                           addr.contains(text, Qt::CaseInsensitive);
            item->setHidden(!matches);
        }
    }

    // Hide popup if no visible items
    bool anyVisible = false;
    for (int i = 0; i < m_list->count(); ++i) {
        if (!m_list->item(i)->isHidden()) {
            anyVisible = true;
            break;
        }
    }
    if (!anyVisible) {
        hidePopup();
    }
}

void AddressInput::showPopup() {
    // Reload contacts from DB every time (always fresh)
    loadContacts();

    if (m_contacts.isEmpty()) {
        return;
    }

    // Populate list
    m_list->clear();
    for (const auto& c : m_contacts) {
        QString display = c.name + QString::fromUtf8("  \xe2\x80\x94  ") + c.address;
        QListWidgetItem* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, c.address);
        item->setData(Qt::UserRole + 1, c.name);
        m_list->addItem(item);
    }

    // Filter by current text
    QString text = m_input->text();
    if (!text.isEmpty()) {
        bool anyVisible = false;
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem* item = m_list->item(i);
            QString name = item->data(Qt::UserRole + 1).toString();
            QString addr = item->data(Qt::UserRole).toString();
            bool matches = name.contains(text, Qt::CaseInsensitive) ||
                           addr.contains(text, Qt::CaseInsensitive);
            item->setHidden(!matches);
            if (matches) {
                anyVisible = true;
            }
        }
        if (!anyVisible) {
            return; // nothing to show
        }
    }

    // Walk up to find QScrollArea for popup parent (same as Dropdown)
    QWidget* popParent = parentWidget();
    if (!popParent) {
        return;
    }

    for (QWidget* w = parentWidget(); w; w = w->parentWidget()) {
        popParent = w;
        if (w->inherits("QScrollArea")) {
            break;
        }
    }

    if (m_popup->parentWidget() != popParent) {
        m_popup->setParent(popParent);
    }

    // Size: measure actual rows, cap at 260px
    int contentH = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        if (!m_list->item(i)->isHidden()) {
            contentH += m_list->sizeHintForRow(i) + m_list->spacing();
        }
    }
    contentH += 12;
    int listH = qMin(contentH, 260);
    m_list->setVerticalScrollBarPolicy(contentH > 260 ? Qt::ScrollBarAsNeeded
                                                      : Qt::ScrollBarAlwaysOff);

    QPoint pos = m_input->mapTo(popParent, QPoint(0, m_input->height() + 4));
    m_popup->setGeometry(pos.x(), pos.y(), m_input->width(), listH);
    m_popup->raise();
    m_popup->show();
}

void AddressInput::hidePopup() { m_popup->hide(); }

void AddressInput::onItemClicked(QListWidgetItem* item) {
    QString addr = item->data(Qt::UserRole).toString();
    QString name = item->data(Qt::UserRole + 1).toString();
    m_input->setText(addr);
    hidePopup();
    emit contactSelected(name, addr);
}

bool AddressInput::eventFilter(QObject* obj, QEvent* event) {
    // Show popup on focus-in to the input
    if (obj == m_input && event->type() == QEvent::FocusIn) {
        if (!m_popup->isVisible()) {
            showPopup();
        }
        return false;
    }

    // Hide on Escape
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape && m_popup->isVisible()) {
            hidePopup();
            return true;
        }
    }

    // Click outside to close
    if (event->type() == QEvent::MouseButtonPress && m_popup->isVisible()) {
        auto* me = static_cast<QMouseEvent*>(event);
        QPoint globalPos = me->globalPosition().toPoint();

        QRect inputRect(m_input->mapToGlobal(QPoint(0, 0)), m_input->size());
        QRect popRect(m_popup->mapToGlobal(QPoint(0, 0)), m_popup->size());

        if (!inputRect.contains(globalPos) && !popRect.contains(globalPos)) {
            hidePopup();
        }
    }

    return QWidget::eventFilter(obj, event);
}
