#include "AmountInput.h"

#include "PaintedPanel.h"
#include "StyledLineEdit.h"

#include <QHBoxLayout>
#include <QPalette>

AmountInput::AmountInput(QWidget* parent) : QWidget(parent) {
    auto* wrapper = new PaintedPanel(this);
    wrapper->setFillColor(QColor(30, 31, 55, 204));
    wrapper->setBorderColor(QColor(100, 100, 150, 77));
    wrapper->setCornerRadius(12.0);
    wrapper->setBorderWidth(1.0);

    QHBoxLayout* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(wrapper);

    QHBoxLayout* layout = new QHBoxLayout(wrapper);
    layout->setContentsMargins(12, 0, 4, 0);
    layout->setSpacing(6);

    auto* amountInput = new StyledLineEdit();
    amountInput->setPlaceholderText("0.00");
    amountInput->setMinimumHeight(40);
    amountInput->setFrameFillColor(Qt::transparent);
    amountInput->setFrameBorderColor(Qt::transparent);
    amountInput->setFrameFocusBorderColor(Qt::transparent);
    amountInput->setFrameBorderWidth(0.0);
    amountInput->setFrameRadius(6.0);
    amountInput->setTextMargins(0, 0, 0, 0);
    m_input = amountInput;

    QPalette pal = m_input->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 90));
    m_input->setPalette(pal);
    layout->addWidget(m_input, 1);

    m_maxBtn = new QPushButton(tr("Max"));
    m_maxBtn->setObjectName("amountMaxBtn");
    m_maxBtn->setCursor(Qt::PointingHandCursor);
    m_maxBtn->setFixedHeight(28);
    layout->addWidget(m_maxBtn);

    setMinimumHeight(44);
    setStyleSheet(QStringLiteral("QPushButton#amountMaxBtn {"
                                 "  background: rgba(100, 100, 150, 0.3);"
                                 "  color: rgba(255, 255, 255, 0.8);"
                                 "  border: none; border-radius: 6px;"
                                 "  padding: 4px 12px; font-size: 13px; font-weight: 600;"
                                 "}"
                                 "QPushButton#amountMaxBtn:hover {"
                                 "  background: rgba(100, 100, 150, 0.5);"
                                 "  color: white;"
                                 "}"));

    connect(m_input, &QLineEdit::textChanged, this, &AmountInput::textChanged);
    connect(m_maxBtn, &QPushButton::clicked, this, &AmountInput::maxClicked);
}

QString AmountInput::text() const { return m_input->text(); }

void AmountInput::setText(const QString& text) { m_input->setText(text); }

void AmountInput::clear() { m_input->clear(); }

void AmountInput::setPlaceholderText(const QString& text) { m_input->setPlaceholderText(text); }

QLineEdit* AmountInput::lineEdit() const { return m_input; }
