#include "AddressBookPage.h"
#include "Theme.h"
#include "db/ContactDb.h"
#include "services/AvatarCache.h"
#include "widgets/ActionIconButton.h"
#include "widgets/Dropdown.h"
#include "widgets/StyledLineEdit.h"
#include "widgets/UploadWidget.h"
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

// ── Contact row with C++ hover (viewport cascade kills QSS :hover) ────

class ContactRow : public QWidget {
  public:
    ContactRow(bool alternate, QWidget* parent = nullptr)
        : QWidget(parent), m_alternate(alternate) {
        setObjectName("abRow");
        setProperty("alternate", m_alternate);
        setProperty("hovered", false);
        setFixedHeight(48);
        setAttribute(Qt::WA_StyledBackground, true);
        setCursor(Qt::PointingHandCursor);
        applyBg(false);
    }

  protected:
    void enterEvent(QEnterEvent*) override { applyBg(true); }
    void leaveEvent(QEvent*) override { applyBg(false); }

  private:
    void applyBg(bool hovered) {
        setProperty("hovered", hovered);
        style()->unpolish(this);
        style()->polish(this);
    }
    bool m_alternate;
};

// ── AddressBookPage ────────────────────────────────────────────

AddressBookPage::AddressBookPage(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget();
    m_stack->addWidget(buildListView());    // Step::List
    m_stack->addWidget(buildContactForm()); // Step::ContactForm

    outerLayout->addWidget(m_stack);
}

void AddressBookPage::showStep(Step step) { m_stack->setCurrentIndex(static_cast<int>(step)); }

// ── List View ─────────────────────────────────────────────────

QWidget* AddressBookPage::buildListView() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("addressBookContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(0);

    // ── Title row with Add Contact button ─────────────────
    QHBoxLayout* titleRow = new QHBoxLayout();
    titleRow->setSpacing(0);

    QLabel* title = new QLabel(tr("Address Book"));
    title->setObjectName("pageTitle");
    titleRow->addWidget(title);
    titleRow->addStretch();

    QPushButton* addBtn = new QPushButton(tr("+ Add Contact"));
    addBtn->setObjectName("abAddButton");
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setMinimumHeight(40);
    connect(addBtn, &QPushButton::clicked, this, [this]() { openAddForm(); });
    titleRow->addWidget(addBtn);

    layout->addLayout(titleRow);
    layout->addSpacing(20);

    // ── Search + Filter row ────────────────────────────────
    QHBoxLayout* searchRow = new QHBoxLayout();
    searchRow->setSpacing(12);

    m_searchInput = new QLineEdit();
    m_searchInput->setObjectName("abSearchInput");
    m_searchInput->setPlaceholderText(tr("Search..."));
    m_searchInput->setMinimumHeight(40);
    QPalette pal = m_searchInput->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_searchInput->setPalette(pal);

    connect(m_searchInput, &QLineEdit::textChanged, this, [this]() { refreshList(); });

    Dropdown* filterDropdown = new Dropdown();
    filterDropdown->addItem(tr("Name"));
    filterDropdown->addItem(tr("Address"));
    filterDropdown->setCurrentItem(tr("Filter by"));
    filterDropdown->setFixedWidth(160);

    searchRow->addWidget(m_searchInput, 1);
    searchRow->addWidget(filterDropdown);
    layout->addLayout(searchRow);
    layout->addSpacing(16);

    // ── Column headers ─────────────────────────────────────
    QHBoxLayout* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(16, 0, 16, 0);

    QLabel* nameHeader = new QLabel(tr("Name"));
    nameHeader->setObjectName("abColumnHeader");
    nameHeader->setFixedWidth(160);

    QLabel* addressHeader = new QLabel(tr("Address"));
    addressHeader->setObjectName("abColumnHeader");

    QLabel* dateHeader = new QLabel(tr("Created"));
    dateHeader->setObjectName("abColumnHeader");
    dateHeader->setFixedWidth(120);

    QLabel* actionsHeader = new QLabel(tr("Actions"));
    actionsHeader->setObjectName("abColumnHeader");
    actionsHeader->setFixedWidth(120);
    actionsHeader->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    headerRow->addWidget(nameHeader);
    headerRow->addWidget(addressHeader, 1);
    headerRow->addWidget(dateHeader);
    headerRow->addWidget(actionsHeader);
    layout->addLayout(headerRow);
    layout->addSpacing(8);

    // ── Separator ──────────────────────────────────────────
    QWidget* sep = new QWidget();
    sep->setObjectName("abSeparator");
    sep->setFixedHeight(1);
    layout->addWidget(sep);
    layout->addSpacing(4);

    // ── Contact list ───────────────────────────────────────
    m_listLayout = new QVBoxLayout();
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(0);

    const QList<AddressBookContactView> contacts = m_handler.listContacts();
    for (int i = 0; i < contacts.size(); ++i) {
        m_listLayout->addWidget(createContactRow(contacts[i], i % 2 == 0));
    }

    layout->addLayout(m_listLayout);
    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

// ── Contact Form — shared for Add / Edit (index 1) ────────────

QWidget* AddressBookPage::buildContactForm() {
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("addressBookContent");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Back button
    QPushButton* backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + " " + tr("Back"));
    backBtn->setObjectName("txBackButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, [this]() { showStep(Step::List); });
    layout->addWidget(backBtn, 0, Qt::AlignLeft);

    // Title — swapped between "Add Contact" / "Edit Contact"
    m_formTitle = new QLabel(tr("Add Contact"));
    m_formTitle->setObjectName("pageTitle");
    layout->addWidget(m_formTitle);

    layout->addSpacing(12);

    // Avatar upload
    m_avatarUpload = new UploadWidget(UploadWidget::Circle, 64);
    layout->addWidget(m_avatarUpload, 0, Qt::AlignHCenter);

    layout->addSpacing(12);

    // Name
    QLabel* nameLabel = new QLabel(tr("Name"));
    nameLabel->setObjectName("txFormLabel");
    layout->addWidget(nameLabel);

    m_nameInput = new StyledLineEdit();
    m_nameInput->setPlaceholderText(tr("Enter contact name..."));
    m_nameInput->setMinimumHeight(44);
    QPalette namePal = m_nameInput->palette();
    namePal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_nameInput->setPalette(namePal);
    layout->addWidget(m_nameInput);

    layout->addSpacing(4);

    // Address
    QLabel* addressLabel = new QLabel(tr("Address"));
    addressLabel->setObjectName("txFormLabel");
    layout->addWidget(addressLabel);

    m_addressInput = new StyledLineEdit();
    m_addressInput->setPlaceholderText(tr("Enter wallet address..."));
    m_addressInput->setMinimumHeight(44);
    QPalette addrPal = m_addressInput->palette();
    addrPal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_addressInput->setPalette(addrPal);
    layout->addWidget(m_addressInput);

    m_addressError = new QLabel(tr("Invalid Solana address"));
    m_addressError->setProperty("uiClass", "addressBookErrorLabel");
    m_addressError->hide();
    layout->addWidget(m_addressError);

    layout->addSpacing(12);

    // Save button — starts disabled
    m_saveBtn = new QPushButton(tr("Save Contact"));
    m_saveBtn->setObjectName("abSaveButton");
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setMinimumHeight(48);
    m_saveBtn->setEnabled(false);
    // Keep button state managed via Theme button styles.
    updateSaveButtonState();

    connect(m_nameInput, &QLineEdit::textChanged, this, &AddressBookPage::updateSaveButtonState);
    connect(m_addressInput, &QLineEdit::textChanged, this, &AddressBookPage::updateSaveButtonState);

    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        const QString name = m_nameInput->text();
        const QString address = m_addressInput->text();
        const bool addressUnchanged =
            m_editContactId >= 0 && address.trimmed() == m_editOriginalAddress;
        if (!addressUnchanged && !m_handler.canSaveContact(name, address)) {
            return;
        }

        const bool ok = m_handler.saveContact(m_editContactId, name, address);
        if (ok) {
            // Save avatar if a new image was selected
            if (m_avatarUpload->hasImage() && !m_avatarUpload->imagePath().isEmpty()) {
                int contactId = m_editContactId;
                if (contactId < 0) {
                    // Was an insert — look up the new contact's ID
                    auto record = ContactDb::getByAddressRecord(address.trimmed());
                    if (record) {
                        contactId = record->id;
                    }
                }
                if (contactId >= 0) {
                    QString relPath;
                    m_handler.saveAvatar(contactId, address.trimmed(), m_avatarUpload->imagePath(),
                                         relPath);
                }
            }
            showStep(Step::List);
            refreshList();
            emit contactsChanged();
        }
    });
    layout->addWidget(m_saveBtn);

    layout->addStretch();

    scroll->setWidget(content);

    return scroll;
}

// ── Open form in Add mode ─────────────────────────────────────

void AddressBookPage::openAddForm() {
    m_editContactId = -1;
    m_editOriginalAddress.clear();
    m_formTitle->setText(tr("Add Contact"));
    m_nameInput->clear();
    m_addressInput->clear();
    m_avatarUpload->clear();
    m_addressError->hide();
    m_saveBtn->setText(tr("Save Contact"));
    updateSaveButtonState();
    showStep(Step::ContactForm);
}

// ── Open form in Edit mode ────────────────────────────────────

void AddressBookPage::openEditForm(int contactId, const QString& name, const QString& address) {
    m_editContactId = contactId;
    m_editOriginalAddress = address;
    m_formTitle->setText(tr("Edit Contact"));
    m_nameInput->setText(name);
    m_addressInput->setText(address);
    m_addressError->hide();

    // Load existing avatar
    QString avatarPath = ContactDb::getAvatarPath(contactId);
    if (!avatarPath.isEmpty()) {
        m_avatarUpload->setImagePath(ContactDb::avatarFullPath(avatarPath));
    } else {
        m_avatarUpload->clear();
    }

    m_saveBtn->setText(tr("Update Contact"));
    updateSaveButtonState();
    showStep(Step::ContactForm);
}

// ── Toggle save button enabled / disabled ─────────────────────

void AddressBookPage::updateSaveButtonState() {
    const QString name = m_nameInput->text().trimmed();
    const QString addr = m_addressInput->text().trimmed();

    // When editing and address hasn't changed, skip format validation
    const bool addressUnchanged = m_editContactId >= 0 && addr == m_editOriginalAddress;
    const bool valid = addressUnchanged
                           ? (!name.isEmpty() && !addr.isEmpty())
                           : m_handler.canSaveContact(m_nameInput->text(), m_addressInput->text());

    m_saveBtn->setEnabled(valid);
    m_addressError->setVisible(!addr.isEmpty() && !valid && !name.isEmpty());
}

// ── Delete confirmation dialog ────────────────────────────────

void AddressBookPage::confirmDeleteAsync(const QString& contactName,
                                         const std::function<void(bool accepted)>& onDone) {
    // Full-window overlay — child widget, no OS window = no animation
    QWidget* overlay = new QWidget(window());
    overlay->setObjectName("confirmOverlay");
    overlay->setGeometry(window()->rect());
    overlay->raise();
    overlay->show();

    // Card centered on overlay
    QWidget* card = new QWidget(overlay);
    card->setObjectName("confirmCard");
    card->setFixedSize(400, 210);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(30, 28, 30, 24);
    cardLayout->setSpacing(16);

    QLabel* icon = new QLabel(QString::fromUtf8("\xe2\x9a\xa0"));
    icon->setObjectName("confirmIcon");
    icon->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(icon);

    QLabel* msg = new QLabel(tr("Are you sure you want to delete <b>%1</b>?").arg(contactName));
    msg->setObjectName("confirmMessage");
    msg->setWordWrap(true);
    msg->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(msg);

    cardLayout->addSpacing(4);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setObjectName("confirmCancelBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setMinimumHeight(40);
    QObject::connect(cancelBtn, &QPushButton::clicked, overlay, [overlay, onDone]() {
        if (onDone) {
            onDone(false);
        }
        overlay->deleteLater();
    });

    QPushButton* okBtn = new QPushButton(tr("Delete"));
    okBtn->setObjectName("confirmDeleteBtn");
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setMinimumHeight(40);
    QObject::connect(okBtn, &QPushButton::clicked, overlay, [overlay, onDone]() {
        if (onDone) {
            onDone(true);
        }
        overlay->deleteLater();
    });

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    cardLayout->addLayout(btnRow);

    card->move((overlay->width() - card->width()) / 2, (overlay->height() - card->height()) / 2);
    card->show();
}

// ── Refresh list from database ─────────────────────────────────

void AddressBookPage::refreshList() {
    // Clear existing rows
    QLayoutItem* item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // Re-query from DB with optional search filter
    QString search = m_searchInput ? m_searchInput->text().trimmed() : QString();
    const QList<AddressBookContactView> contacts = m_handler.listContacts(search);

    for (int i = 0; i < contacts.size(); ++i) {
        m_listLayout->addWidget(createContactRow(contacts[i], i % 2 == 0));
    }
}

// ── Contact row with hover effect ──────────────────────────────

QWidget* AddressBookPage::createContactRow(const AddressBookContactView& contact, bool alternate) {
    ContactRow* row = new ContactRow(alternate);

    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(16, 0, 16, 0);
    rowLayout->setSpacing(0);

    // Avatar (28px circle)
    constexpr int kRowAvatarSize = 28;
    QLabel* avatarLabel = new QLabel();
    avatarLabel->setFixedSize(kRowAvatarSize, kRowAvatarSize);
    avatarLabel->setAlignment(Qt::AlignCenter);

    if (!contact.avatarPath.isEmpty()) {
        QString fullPath = ContactDb::avatarFullPath(contact.avatarPath);
        QPixmap pm(fullPath);
        if (!pm.isNull()) {
            qreal dpr = qApp->devicePixelRatio();
            avatarLabel->setPixmap(AvatarCache::circleClip(pm, kRowAvatarSize, dpr));
        }
    }
    if (avatarLabel->pixmap().isNull()) {
        // Fallback: colored circle with first letter
        uint hash = 0;
        for (const QChar& c : contact.address) {
            hash = hash * 31 + c.unicode();
        }
        static const QColor kColors[] = {"#14F195", "#9945FF", "#4DA1FF", "#FF6B6B",
                                         "#FFB347", "#B8E986", "#FF85C0", "#7AFBFF"};
        QColor bg = kColors[hash % 8];
        avatarLabel->setText(contact.name.left(1).toUpper());
        avatarLabel->setStyleSheet(QString("background: %1; color: white; border-radius: %2px;"
                                           "font-size: 13px; font-weight: bold;")
                                       .arg(bg.name())
                                       .arg(kRowAvatarSize / 2));
    }
    rowLayout->addWidget(avatarLabel);
    rowLayout->addSpacing(10);

    QLabel* nameLabel = new QLabel(contact.name);
    nameLabel->setObjectName("abContactName");
    nameLabel->setFixedWidth(160);

    QLabel* addressLabel = new QLabel(contact.address);
    addressLabel->setObjectName("abContactAddress");

    QLabel* dateLabel = new QLabel(contact.createdDate);
    dateLabel->setObjectName("abContactDate");
    dateLabel->setFixedWidth(120);

    // Action buttons using extracted icons
    QHBoxLayout* actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(6);
    actionsLayout->setContentsMargins(0, 0, 0, 0);

    auto* sendBtn = new ActionIconButton(":/icons/action/send.png");
    sendBtn->setToolTip(tr("Send to this address"));
    auto* editBtn = new ActionIconButton(":/icons/action/edit.png");
    editBtn->setToolTip(tr("Edit contact"));
    auto* deleteBtn = new ActionIconButton(":/icons/action/delete.png");
    deleteBtn->setToolTip(tr("Delete contact"));

    // Wire send button
    connect(sendBtn, &QPushButton::clicked, this,
            [this, contact]() { emit sendToAddress(contact.address); });

    // Wire edit button
    connect(editBtn, &QPushButton::clicked, this,
            [this, contact]() { openEditForm(contact.id, contact.name, contact.address); });

    // Wire delete button with confirmation
    connect(deleteBtn, &QPushButton::clicked, this, [this, contact]() {
        confirmDeleteAsync(contact.name, [this, contact](bool accepted) {
            if (!accepted) {
                return;
            }
            m_handler.deleteContact(contact.id);
            refreshList();
            emit contactsChanged();
        });
    });

    actionsLayout->addStretch();
    actionsLayout->addWidget(sendBtn);
    actionsLayout->addWidget(editBtn);
    actionsLayout->addWidget(deleteBtn);

    QWidget* actionsContainer = new QWidget();
    actionsContainer->setObjectName("abActionsContainer");
    actionsContainer->setFixedWidth(120);
    actionsContainer->setLayout(actionsLayout);

    rowLayout->addWidget(nameLabel);
    rowLayout->addWidget(addressLabel, 1);
    rowLayout->addWidget(dateLabel);
    rowLayout->addWidget(actionsContainer);

    return row;
}
