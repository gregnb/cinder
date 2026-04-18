#include "CardListItem.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QVBoxLayout>

CardListItem::CardListItem(QWidget* parent) : QWidget(parent) {
    setObjectName("cardListItem");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(64);

    // Widget-level stylesheet overrides viewport cascade.
    // Child labels must be explicitly transparent so they don't paint the
    // viewport's inherited background over the hover highlight.
    setStyleSheet(QString("#cardListItem { border-radius: %1px; }"
                          "#cardListItem:hover { background: %2; }"
                          "#cardListItem QLabel { background: transparent; }")
                      .arg(Theme::smallRadius)
                      .arg(Theme::overlayBg));

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(12);

    // Icon
    m_icon = new QLabel();
    m_icon->setFixedSize(40, 40);
    m_icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_icon);

    // Title + subtitle
    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    textLayout->setContentsMargins(0, 0, 0, 0);

    m_title = new QLabel();
    m_title->setObjectName("listItemTitle");
    m_title->setTextFormat(Qt::PlainText);
    textLayout->addStretch();
    textLayout->addWidget(m_title);

    m_subtitle = new QLabel();
    m_subtitle->setObjectName("listItemSubtitle");
    m_subtitle->setTextFormat(Qt::PlainText);
    textLayout->addWidget(m_subtitle);
    textLayout->addStretch();

    layout->addLayout(textLayout, 1);

    // Right-side value(s)
    QVBoxLayout* valueLayout = new QVBoxLayout();
    valueLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_value = new QLabel();
    m_value->setAlignment(Qt::AlignRight);
    m_value->hide();
    valueLayout->addWidget(m_value);

    m_subValue = new QLabel();
    m_subValue->setAlignment(Qt::AlignRight);
    m_subValue->hide();
    valueLayout->addWidget(m_subValue);

    layout->addLayout(valueLayout);
}

void CardListItem::applyIconBackground(const QString& background) {
    if (!background.isEmpty()) {
        // Widget-level stylesheet overrides viewport parent cascade for 'background'.
        // Other properties (color, font-size, font-weight) still cascade from app QSS.
        m_icon->setStyleSheet(QString("background: %1; border-radius: 20px;").arg(background));
    }
}

void CardListItem::setIcon(const QString& text, const QString& objectName,
                           const QString& background) {
    m_icon->setText(text);
    m_icon->setObjectName(objectName);
    applyIconBackground(background);
}

void CardListItem::setIconPixmap(const QPixmap& pixmap, const QString& objectName,
                                 const QString& background) {
    m_icon->setPixmap(pixmap);
    m_icon->setObjectName(objectName);
    applyIconBackground(background);
}

void CardListItem::setTitle(const QString& text) { m_title->setText(text); }

void CardListItem::setSubtitle(const QString& text) { m_subtitle->setText(text); }

void CardListItem::setValue(const QString& text, const QString& objectName) {
    m_value->setText(text);
    m_value->setObjectName(objectName);
    m_value->show();
}

void CardListItem::setSubValue(const QString& text, const QString& objectName) {
    m_subValue->setText(text);
    m_subValue->setObjectName(objectName);
    m_subValue->show();
}
