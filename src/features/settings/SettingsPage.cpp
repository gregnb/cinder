#include "SettingsPage.h"
#include "Constants.h"
#include "Theme.h"
#include "features/settings/SettingsHandler.h"
#include "widgets/Dropdown.h"
#include "widgets/StyledCheckbox.h"
#include "widgets/TabBar.h"

#include <QAbstractItemDelegate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

SettingsPage::SettingsPage(SolanaApi* api, QWidget* parent)
    : QWidget(parent), m_handler(new SettingsHandler(api, this)) {
    setObjectName("settingsContent");
    setProperty("uiClass", "content");

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setProperty("uiClass", "contentViewport");

    QWidget* content = new QWidget();
    content->setObjectName("settingsInner");
    content->setProperty("uiClass", "content");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(40, 20, 40, 30);
    layout->setSpacing(16);

    // Title
    QLabel* title = new QLabel(tr("Settings"));
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // Tabs
    m_tabs = new TabBar();
    m_tabs->addTab(tr("General"));
    m_tabs->addTab(tr("Experimental"));
    m_tabs->addTab(tr("About"));
    layout->addWidget(m_tabs);

    // Tab content
    m_tabStack = new QStackedWidget();
    m_tabStack->addWidget(buildGeneralTab());
    m_tabStack->addWidget(buildExperimentalTab());
    m_tabStack->addWidget(buildAboutTab());
    connect(m_tabs, &TabBar::currentChanged, m_tabStack, &QStackedWidget::setCurrentIndex);
    m_tabs->setActiveIndex(0);
    layout->addWidget(m_tabStack, 1);

    layout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll);
}

QWidget* SettingsPage::makeSettingsRow(const QString& titleText, const QString& desc,
                                       QWidget* control) {
    QWidget* row = new QWidget();
    row->setObjectName("settingsRow");

    QHBoxLayout* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 16, 0, 16);
    h->setSpacing(20);

    // Left: title + description
    QVBoxLayout* left = new QVBoxLayout();
    left->setSpacing(4);

    QLabel* t = new QLabel(titleText);
    t->setObjectName("settingsRowTitle");
    left->addWidget(t);

    QLabel* d = new QLabel(desc);
    d->setObjectName("settingsRowDesc");
    d->setWordWrap(true);
    left->addWidget(d);

    h->addLayout(left, 1);

    // Right: control
    if (control) {
        control->setFixedWidth(260);
        h->addWidget(control);
    }

    return row;
}

QWidget* SettingsPage::makeSeparator() {
    QFrame* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Plain);
    sep->setFixedHeight(1);
    sep->setObjectName("settingsSeparator");
    return sep;
}

QWidget* SettingsPage::buildGeneralTab() {
    QWidget* tab = new QWidget();
    tab->setObjectName("settingsTab");
    QVBoxLayout* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(0);

    // RPC Endpoints
    lay->addWidget(buildRpcEndpointSection());
    lay->addWidget(makeSeparator());

    // Preferred currency
    Dropdown* currencyDrop = new Dropdown();
    currencyDrop->addItem(tr("US Dollar - USD"));
    currencyDrop->addItem(tr("Euro - EUR"));
    currencyDrop->addItem(tr("British Pound - GBP"));
    currencyDrop->addItem(tr("Japanese Yen - JPY"));
    currencyDrop->addItem(tr("Korean Won - KRW"));
    currencyDrop->setCurrentItem(tr("US Dollar - USD"));
    lay->addWidget(makeSettingsRow(
        tr("Preferred currency"),
        tr("Choose the currency shown next to your balance and operations."), currencyDrop));
    lay->addWidget(makeSeparator());

    // Language
    Dropdown* langDrop = new Dropdown();
    const QStringList languageItems = SettingsHandler::languageDisplayNames();
    for (const QString& language : languageItems) {
        langDrop->addItem(language);
    }
    langDrop->setCurrentItem(
        SettingsHandler::displayNameForCode(SettingsHandler::savedLanguageCode()));
    connect(langDrop, &Dropdown::itemSelected, this, [this](const QString& displayName) {
        const QString newCode = m_handler->applyLanguageDisplayName(displayName);
        if (newCode.isEmpty()) {
            return;
        }
        emit languageChanged(newCode);
    });
    lay->addWidget(makeSettingsRow(tr("Language"), tr("Choose the display language."), langDrop));
    lay->addWidget(makeSeparator());

    // Theme
    Dropdown* themeDrop = new Dropdown();
    themeDrop->addItem(tr("Dark"));
    themeDrop->setCurrentItem(tr("Dark"));
    lay->addWidget(makeSettingsRow(tr("Theme"), tr("Select the visual theme."), themeDrop));
    lay->addWidget(makeSeparator());

    // Auto-lock timeout
    Dropdown* lockDrop = new Dropdown();
    lockDrop->addItem(tr("5 minutes"));
    lockDrop->addItem(tr("15 minutes"));
    lockDrop->addItem(tr("30 minutes"));
    lockDrop->addItem(tr("1 hour"));
    lockDrop->addItem(tr("Never"));
    lockDrop->setCurrentItem(tr("15 minutes"));
    lay->addWidget(makeSettingsRow(tr("Auto-lock timeout"),
                                   tr("Lock the wallet after a period of inactivity."), lockDrop));

    // Touch ID (macOS only, if hardware available)
    if (m_handler->isBiometricAvailableOnDevice()) {
        lay->addWidget(makeSeparator());

        m_biometricCb = new StyledCheckbox();

        // Load current state — use active wallet address if set, else first wallet
        m_biometricCb->setChecked(m_handler->biometricEnabledForWallet(m_walletAddress));

        connect(m_biometricCb, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked) {
                // Need password confirmation — CinderWalletApp handles this
                emit biometricToggled(true);
            } else {
                m_handler->disableBiometric(m_walletAddress);
            }
        });

        lay->addWidget(makeSettingsRow(
            tr("Touch ID"),
            tr("Use Touch ID to unlock your wallet instead of typing your password."),
            m_biometricCb));
    }

    lay->addStretch();
    return tab;
}

QWidget* SettingsPage::buildRpcEndpointSection() {
    QWidget* section = new QWidget();
    section->setObjectName("settingsRow");

    QVBoxLayout* vlay = new QVBoxLayout(section);
    vlay->setContentsMargins(0, 16, 0, 16);
    vlay->setSpacing(8);

    QLabel* title = new QLabel(tr("RPC Endpoints"));
    title->setObjectName("settingsRowTitle");
    vlay->addWidget(title);

    QLabel* desc = new QLabel(tr("All endpoints are active simultaneously."));
    desc->setObjectName("settingsRowDesc");
    desc->setWordWrap(true);
    vlay->addWidget(desc);

    vlay->addSpacing(4);

    // Scrollable list
    m_rpcList = new QListWidget();
    m_rpcList->setObjectName("rpcEndpointList");
    m_rpcList->setFixedHeight(140);
    m_rpcList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rpcList->setSelectionMode(QAbstractItemView::SingleSelection);

    // Populate from saved endpoints
    const QStringList endpoints = m_handler->currentRpcUrls();
    for (const QString& ep : endpoints) {
        auto* item = new QListWidgetItem(ep, m_rpcList);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }

    vlay->addWidget(m_rpcList);

    // Button row
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_rpcAddBtn = new QPushButton(tr("Add"));
    m_rpcAddBtn->setCursor(Qt::PointingHandCursor);
    m_rpcAddBtn->setFixedHeight(28);
    m_rpcAddBtn->setProperty("uiClass", "settingsRpcAddBtn");

    m_rpcRemoveBtn = new QPushButton(tr("Remove"));
    m_rpcRemoveBtn->setCursor(Qt::PointingHandCursor);
    m_rpcRemoveBtn->setFixedHeight(28);
    m_rpcRemoveBtn->setProperty("uiClass", "settingsRpcRemoveBtn");
    m_rpcRemoveBtn->setEnabled(m_rpcList->count() > 1);

    btnRow->addWidget(m_rpcAddBtn);
    btnRow->addWidget(m_rpcRemoveBtn);
    btnRow->addStretch();

    vlay->addLayout(btnRow);

    // ── Connections ──

    connect(m_rpcAddBtn, &QPushButton::clicked, this, [this]() {
        auto* item = new QListWidgetItem(QString(), m_rpcList);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_rpcList->setCurrentItem(item);
        m_rpcList->editItem(item);
        m_rpcRemoveBtn->setEnabled(m_rpcList->count() > 1);
    });

    connect(m_rpcRemoveBtn, &QPushButton::clicked, this, [this]() {
        if (m_rpcList->count() <= 1) {
            return;
        }
        QListWidgetItem* item = m_rpcList->currentItem();
        if (!item) {
            return;
        }
        delete m_rpcList->takeItem(m_rpcList->row(item));
        syncRpcListToHandler();
        m_rpcRemoveBtn->setEnabled(m_rpcList->count() > 1);
    });

    connect(m_rpcList->itemDelegate(), &QAbstractItemDelegate::commitData, this, [this]() {
        syncRpcListToHandler();
        m_rpcRemoveBtn->setEnabled(m_rpcList->count() > 1);
    });

    return section;
}

void SettingsPage::syncRpcListToHandler() {
    // Remove invalid/blank items (iterate in reverse to avoid index shifts)
    for (int i = m_rpcList->count() - 1; i >= 0; --i) {
        const QString text = m_rpcList->item(i)->text().trimmed();
        if (text.isEmpty()) {
            delete m_rpcList->takeItem(i);
            continue;
        }
        QUrl url(text);
        if (!url.isValid() || url.scheme().isEmpty() ||
            (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")) ||
            url.host().isEmpty()) {
            delete m_rpcList->takeItem(i);
            continue;
        }
    }

    QStringList urls;
    for (int i = 0; i < m_rpcList->count(); ++i) {
        urls.append(m_rpcList->item(i)->text().trimmed());
    }
    m_handler->setRpcUrls(urls);
}

QWidget* SettingsPage::buildExperimentalTab() {
    QWidget* tab = new QWidget();
    tab->setObjectName("settingsTab");
    QVBoxLayout* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(0);

    // Disclaimer
    QLabel* disclaimer = new QLabel(tr("These features are experimental and may change or be "
                                       "removed in future versions."));
    disclaimer->setObjectName("settingsDisclaimer");
    disclaimer->setWordWrap(true);
    lay->addWidget(disclaimer);

    lay->addStretch();
    return tab;
}

QWidget* SettingsPage::buildAboutTab() {
    QWidget* tab = new QWidget();
    tab->setObjectName("settingsTab");
    QVBoxLayout* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(0);

    // Helper for about rows
    auto addInfoRow = [&](const QString& label, const QString& value, bool isLink = false) {
        QWidget* row = new QWidget();
        row->setObjectName("settingsRow");
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 12, 0, 12);

        QLabel* lbl = new QLabel(label);
        lbl->setObjectName("settingsAboutLabel");
        lbl->setFixedWidth(180);
        h->addWidget(lbl);

        QLabel* val = new QLabel(value);
        if (isLink) {
            val->setObjectName("settingsAboutLink");
            val->setCursor(Qt::PointingHandCursor);
            val->setTextInteractionFlags(Qt::TextBrowserInteraction);
            val->setOpenExternalLinks(true);
            val->setText(QString("<a href='%1'>%1</a>").arg(value));
        } else {
            val->setObjectName("settingsAboutValue");
            val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        }
        h->addWidget(val, 1);

        lay->addWidget(row);
        lay->addWidget(makeSeparator());
    };

    addInfoRow(tr("Application"), "Cinder");
    addInfoRow(tr("Version"), AppVersion::string);
    addInfoRow(tr("Built with"), "Qt 6, libsodium, ed25519");
    addInfoRow(tr("License"), "MIT");

    lay->addStretch();
    return tab;
}

void SettingsPage::setWalletAddress(const QString& address) {
    m_walletAddress = address;
    if (m_biometricCb) {
        m_biometricCb->blockSignals(true);
        m_biometricCb->setChecked(m_handler->biometricEnabledForWallet(m_walletAddress));
        m_biometricCb->blockSignals(false);
    }
}

void SettingsPage::setBiometricChecked(bool checked) {
    if (m_biometricCb) {
        m_biometricCb->blockSignals(true);
        m_biometricCb->setChecked(checked);
        m_biometricCb->blockSignals(false);
    }
}
