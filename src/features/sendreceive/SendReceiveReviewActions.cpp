#include "features/sendreceive/SendReceivePage.h"

#include "Theme.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "services/SolanaApi.h"
#include "services/model/PriorityFee.h"
#include "tx/ProgramIds.h"
#include "widgets/AddressInput.h"
#include "widgets/AmountInput.h"
#include "widgets/PillButtonGroup.h"
#include "widgets/TokenDropdown.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
    constexpr int kPdfMarginMm = 15;
    constexpr quint32 kComputeUnitLimit = 200000;
    constexpr quint64 kPriorityFeeFloor = 1000;      // micro-lamports/CU
    constexpr quint64 kPriorityFeeCeiling = 5000000; // micro-lamports/CU
    constexpr quint64 kCustomFeeCeiling = 10000000;  // micro-lamports/CU
    constexpr int kFeeTimeoutMs = 5000;
} // namespace

void SendReceivePage::executeSend() {
    if (!m_solanaApi || !m_signer) {
        setStatusLabelState(m_sendStatusLabel, tr("Error: wallet not available for signing."),
                            true);
        return;
    }

    m_sendConfirmBtn->setEnabled(false);
    m_sendConfirmBtn->setText(tr("Sending..."));
    m_sendConfirmBtn->setStyleSheet(Theme::primaryBtnDisabledStyle);
    m_sendStatusLabel->setVisible(false);

    auto resetBtn = [this]() {
        m_sendConfirmBtn->setEnabled(true);
        m_sendConfirmBtn->setText(tr("Confirm & Send"));
        m_sendConfirmBtn->setStyleSheet(Theme::primaryBtnStyle);
    };

    const QString currentIcon = m_tokenDropdown->currentIconPath();
    if (!m_tokenMeta.contains(currentIcon)) {
        setStatusLabelState(m_sendStatusLabel, tr("Error: token metadata not found."), true);
        resetBtn();
        return;
    }

    QList<SendReceiveRecipientInput> recipientInputs;
    recipientInputs.reserve(m_recipientRows.size());
    for (const auto& row : m_recipientRows) {
        recipientInputs.append({row.addressInput->address(), row.amountInput->text()});
    }

    SendReceivePrepareSendRequest prepRequest;
    prepRequest.walletAddress = m_walletAddress;
    prepRequest.tokenMeta = m_tokenMeta[currentIcon];
    prepRequest.recipientInputs = recipientInputs;
    prepRequest.transferFeeBasisPoints = m_transferFeeBasisPoints;
    prepRequest.transferFeeMax = m_transferFeeMax;
    prepRequest.nonceEnabled = m_nonceEnabled;
    prepRequest.nonceAddress = m_nonceAddress;
    prepRequest.nonceValue = m_nonceValue;
    SendReceivePrepareSendResult prep = m_handler->prepareSendExecution(prepRequest);
    if (!prep.ok) {
        QString errorText;
        if (prep.error == SendReceivePrepareSendError::NoValidRecipients) {
            errorText = tr("No valid recipients.");
        } else if (prep.error == SendReceivePrepareSendError::DeriveTokenAccountFailed) {
            errorText = tr("Error: could not derive token account for %1").arg(prep.errorDetail);
        } else {
            errorText = tr("Error: token metadata not found.");
        }
        setStatusLabelState(m_sendStatusLabel, errorText, true);
        resetBtn();
        return;
    }

    SendReceiveHandler::ExecuteSendCallbacks callbacks;
    callbacks.onStatus = [this](const QString& text, bool isError) {
        setStatusLabelState(m_sendStatusLabel, text, isError);
    };
    callbacks.onSuccess = [this](const QString& txSig) {
        emit transactionSent(txSig);

        const QString tokenSymbol =
            m_tokenDropdown->currentText().split(' ', Qt::SkipEmptyParts).first();
        double totalAmount = 0.0;
        QString recipientAddress;
        for (const auto& row : m_recipientRows) {
            totalAmount += row.amountInput->text().remove(',').toDouble();
            if (recipientAddress.isEmpty()) {
                recipientAddress = row.addressInput->address();
            }
        }
        const QString amountStr = m_handler->formatCryptoAmount(totalAmount) + " " + tokenSymbol;
        constexpr double kNetworkFeeSol = 0.000005;
        const QString feeStr = m_handler->formatCryptoAmount(kNetworkFeeSol) + " SOL";

        SendReceiveSuccessPageInfo info;
        info.title = tr("Transaction Sent");
        info.amount = amountStr;
        info.tokenSymbol = tokenSymbol;
        info.recipient = recipientAddress;
        info.sender = m_walletAddress;
        info.signature = txSig;
        info.networkFee = feeStr;
        info.txVersion = tr("Legacy");
        info.result = tr("Submitted");
        showSuccessPage(info);
        startConfirmationPolling(txSig);
    };
    callbacks.onFinished = [resetBtn]() { resetBtn(); };

    // Determine priority fee based on speed selector
    const int speedIndex = m_speedSelector ? m_speedSelector->activeIndex() : 0;

    if (speedIndex == 1 && m_customFeeInput) {
        // Custom mode — convert SOL to microLamports/CU
        double solAmount = m_customFeeInput->text().remove(',').toDouble();
        quint64 microLamports =
            static_cast<quint64>((solAmount * 1e9) / static_cast<double>(kComputeUnitLimit));
        microLamports = qBound(kPriorityFeeFloor, microLamports, kCustomFeeCeiling);
        prep.executionRequest.priorityFeeMicroLamports = microLamports;
        m_handler->executeSendFlow(prep.executionRequest, m_solanaApi, m_signer, this, callbacks);
    } else {
        // Auto mode — fetch recent fees, compute median, then execute
        auto* guard = new QObject(this);
        auto done = std::make_shared<bool>(false);

        connect(m_solanaApi, &SolanaApi::prioritizationFeesReady, guard,
                [this, guard, done, prep, callbacks](const QList<PriorityFee>& fees) mutable {
                    if (*done) {
                        return;
                    }
                    *done = true;
                    guard->deleteLater();

                    quint64 median = 0;
                    if (!fees.isEmpty()) {
                        QList<quint64> sorted;
                        sorted.reserve(fees.size());
                        for (const auto& f : fees) {
                            sorted.append(f.prioritizationFee);
                        }
                        std::sort(sorted.begin(), sorted.end());
                        median = sorted[sorted.size() / 2];
                    }
                    median = qBound(kPriorityFeeFloor, median, kPriorityFeeCeiling);

                    prep.executionRequest.priorityFeeMicroLamports = median;
                    m_handler->executeSendFlow(prep.executionRequest, m_solanaApi, m_signer, this,
                                               callbacks);
                });

        // Timeout fallback — proceed with floor fee if RPC is slow
        QTimer::singleShot(kFeeTimeoutMs, guard, [this, guard, done, prep, callbacks]() mutable {
            if (*done) {
                return;
            }
            *done = true;
            guard->deleteLater();

            prep.executionRequest.priorityFeeMicroLamports = kPriorityFeeFloor;
            m_handler->executeSendFlow(prep.executionRequest, m_solanaApi, m_signer, this,
                                       callbacks);
        });

        m_solanaApi->fetchRecentPrioritizationFees();
    }
}

void SendReceivePage::exportReviewPdf() {
    QWidget* content = m_reviewScroll->widget();
    if (!content) {
        return;
    }

    const QString defaultName =
        QString("tx-review-%1.pdf").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmmss"));

    const QString filePath =
        QFileDialog::getSaveFileName(this, tr("Export Review as PDF"),
                                     QDir::homePath() + "/" + defaultName, tr("PDF Files (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }

    QWidget* headerRow = content->findChild<QWidget*>("reviewHeaderRow");
    const bool headerWasVisible = headerRow && headerRow->isVisible();
    if (headerRow) {
        headerRow->hide();
    }
    const bool sendConfirmWasVisible = m_sendConfirmBtn && m_sendConfirmBtn->isVisible();
    const bool sendStatusWasVisible = m_sendStatusLabel && m_sendStatusLabel->isVisible();
    const bool createConfirmWasVisible =
        m_createTokenConfirmBtn && m_createTokenConfirmBtn->isVisible();
    const bool createStatusWasVisible =
        m_createTokenStatusLabel && m_createTokenStatusLabel->isVisible();
    if (m_sendConfirmBtn) {
        m_sendConfirmBtn->hide();
    }
    if (m_sendStatusLabel) {
        m_sendStatusLabel->hide();
    }
    if (m_createTokenConfirmBtn) {
        m_createTokenConfirmBtn->hide();
    }
    if (m_createTokenStatusLabel) {
        m_createTokenStatusLabel->hide();
    }

    QSize fullSize = content->sizeHint();
    if (fullSize.width() < content->width()) {
        fullSize.setWidth(content->width());
    }
    const QSize oldSize = content->size();
    content->resize(fullSize);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(kPdfMarginMm, kPdfMarginMm, kPdfMarginMm, kPdfMarginMm),
                           QPageLayout::Millimeter);

    QPainter painter(&printer);
    if (!painter.isActive()) {
        content->resize(oldSize);
        if (headerWasVisible && headerRow) {
            headerRow->show();
        }
        if (sendConfirmWasVisible && m_sendConfirmBtn) {
            m_sendConfirmBtn->show();
        }
        if (sendStatusWasVisible && m_sendStatusLabel) {
            m_sendStatusLabel->show();
        }
        if (createConfirmWasVisible && m_createTokenConfirmBtn) {
            m_createTokenConfirmBtn->show();
        }
        if (createStatusWasVisible && m_createTokenStatusLabel) {
            m_createTokenStatusLabel->show();
        }
        return;
    }

    const QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
    const double scaleX = pageRect.width() / fullSize.width();
    const double scaleY = scaleX;
    const double pageContentHeight = pageRect.height() / scaleY;
    const int totalPages =
        qMax(1, static_cast<int>(std::ceil(fullSize.height() / pageContentHeight)));

    for (int page = 0; page < totalPages; ++page) {
        if (page > 0) {
            printer.newPage();
        }

        painter.save();
        painter.scale(scaleX, scaleY);
        painter.translate(0, -page * pageContentHeight);
        const QRectF clipRect(0, page * pageContentHeight, fullSize.width(), pageContentHeight);
        painter.setClipRect(clipRect);
        content->render(&painter);
        painter.restore();
    }

    painter.end();

    content->resize(oldSize);
    if (headerWasVisible && headerRow) {
        headerRow->show();
    }
    if (sendConfirmWasVisible && m_sendConfirmBtn) {
        m_sendConfirmBtn->show();
    }
    if (sendStatusWasVisible && m_sendStatusLabel) {
        m_sendStatusLabel->show();
    }
    if (createConfirmWasVisible && m_createTokenConfirmBtn) {
        m_createTokenConfirmBtn->show();
    }
    if (createStatusWasVisible && m_createTokenStatusLabel) {
        m_createTokenStatusLabel->show();
    }
}

void SendReceivePage::exportReviewCsv() {
    const QString defaultName =
        QString("tx-review-%1.csv").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmmss"));

    const QString filePath =
        QFileDialog::getSaveFileName(this, tr("Export Review as CSV"),
                                     QDir::homePath() + "/" + defaultName, tr("CSV Files (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }

    const QString tokenSymbol =
        m_tokenDropdown->currentText().split(' ', Qt::SkipEmptyParts).first();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    QList<SendReceiveRecipientInput> recipientInputs;
    recipientInputs.reserve(m_recipientRows.size());
    for (const auto& row : m_recipientRows) {
        recipientInputs.append({row.addressInput->address(), row.amountInput->text()});
    }

    const QString csv = m_handler->buildReviewCsv(recipientInputs, tokenSymbol, m_walletAddress,
                                                  m_tokenMeta, m_tokenDropdown->currentIconPath());
    out << csv;
    file.close();
}

void SendReceivePage::debugShowReview() {
    m_tokenDropdown->clear();
    m_tokenDropdown->addToken(":/icons/tokens/bonk.png", "BERN  — BonkEarn", "42,069.00");
    m_tokenDropdown->selectByIcon(":/icons/tokens/bonk.png");
    m_tokenMeta[":/icons/tokens/bonk.png"] = {"CKfatsPMUf8SkiURsDXs7eK6GWb4Jsd6UDbs7twMCWxo",
                                              "FakeTokenAcct1111111111111111111111111111111", 5,
                                              SolanaPrograms::Token2022Program};

    while (m_recipientRows.size() < 3) {
        addRecipientRow();
    }

    m_recipientRows[0].addressInput->setAddress("7xKXtg2CW87d97TXJSDpbD5jBkheTqA83TZRuJosgAsU");
    m_recipientRows[0].amountInput->setText("1500.00");
    m_recipientRows[1].addressInput->setAddress("9WzDXwBbmkg8ZTbNMqUxvQRAyrZzDsGYdLVL9zYtAWWM");
    m_recipientRows[1].amountInput->setText("250.50");
    m_recipientRows[2].addressInput->setAddress("DYw8jCTfwHNRJhhmFcbXvVDTqWMEVFBX6ZKUmG5CNSKK");
    m_recipientRows[2].amountInput->setText("3000.00");

    m_nonceEnabled = true;
    m_nonceAddress = "5ZWj7a1f8tWkjBESHKgrLmXshuXxqeY9SYcfbshpAqPG";
    m_nonceValue = "GfVcyD4kkTrj4bKceuHApstKFGMnJhRkbQSqUHREgnHr";

    populateReviewPage();
    setCurrentPage(StackPage::Review);
}
