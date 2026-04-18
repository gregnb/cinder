#include "features/sendreceive/SendReceiveHandler.h"

#include "db/ContactDb.h"
#include "db/PortfolioDb.h"
#include "db/TokenAccountDb.h"
#include "tx/KnownTokens.h"
#include "tx/ProgramIds.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TokenOperationBuilder.h"

#include <QCoreApplication>
#include <algorithm>

namespace {
    constexpr double kLamportsPerSol = 1e9;
    constexpr double kCreateTokenNetworkFeeSol = 0.000005;
    constexpr int kSolDecimals = 9;
    constexpr int kBalanceMajorPrecision = 2;
    constexpr int kBalanceMinorPrecision = 6;
    constexpr double kBalanceMajorThreshold = 1.0;
    using namespace Token2022AccountSize;

    const QString kSolMint = WSOL_MINT;

    QString escapeCsvField(const QString& value) {
        QString escaped = value;
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    }
} // namespace

QString SendReceiveHandler::formatCryptoAmount(double amount) const {
    QString value = QString::number(amount, 'f', 9);
    if (value.contains('.')) {
        while (value.endsWith('0')) {
            value.chop(1);
        }
        if (value.endsWith('.')) {
            value.append('0');
        }
    }
    return value;
}

int SendReceiveHandler::batchCount(int recipientCount) const {
    if (recipientCount <= 0) {
        return 0;
    }
    return (recipientCount + RECIPIENTS_PER_TX - 1) / RECIPIENTS_PER_TX;
}

SendReceiveTokenRefreshResult
SendReceiveHandler::refreshTokenRows(const QString& walletAddress,
                                     const QString& currentIcon) const {
    SendReceiveTokenRefreshResult result;
    if (walletAddress.isEmpty()) {
        return result;
    }

    const auto accounts = TokenAccountDb::getAccountsByOwnerRecords(walletAddress);

    QString solBalance = "0.00 SOL";
    for (const auto& acct : accounts) {
        if (acct.tokenAddress == WSOL_MINT) {
            const double balance = acct.balance.toDouble();
            solBalance =
                QString::number(balance, 'f',
                                balance >= kBalanceMajorThreshold ? kBalanceMajorPrecision
                                                                  : kBalanceMinorPrecision) +
                " SOL";
            break;
        }
    }

    SendReceiveTokenRow solRow;
    solRow.icon = ":/icons/tokens/sol.png";
    solRow.display = "SOL  —  Solana";
    solRow.balance = solBalance;
    solRow.meta = {WSOL_MINT, walletAddress, kSolDecimals, SolanaPrograms::TokenProgram};
    result.rows.append(solRow);

    for (const auto& acct : accounts) {
        const QString mint = acct.tokenAddress;
        if (mint == WSOL_MINT) {
            continue;
        }

        QString symbol = acct.symbol;
        const QString name = acct.name;
        const double balance = acct.balance.toDouble();

        const KnownToken known = resolveKnownToken(mint);
        if (!known.symbol.isEmpty()) {
            symbol = known.symbol;
        }

        QString icon = known.iconPath;
        if (icon.isEmpty()) {
            const auto token = TokenAccountDb::getTokenRecord(mint);
            if (token.has_value()) {
                icon = token->logoUrl;
            }
        }
        if (icon.isEmpty()) {
            icon = "mint:" + mint;
        }

        const bool hasRealName =
            !known.symbol.isEmpty() || (!symbol.isEmpty() && symbol != mint.left(symbol.length()));
        const QString display =
            hasRealName ? symbol + "  —  " + (name.isEmpty() ? mint : name) : mint;
        const QString balanceText =
            QString::number(balance, 'f',
                            balance >= kBalanceMajorThreshold ? kBalanceMajorPrecision
                                                              : kBalanceMinorPrecision) +
            " " + symbol;

        result.rows.append({icon,
                            display,
                            balanceText,
                            {mint, acct.accountAddress, acct.decimals, acct.tokenProgram}});
    }

    std::sort(result.rows.begin(), result.rows.end(),
              [](const SendReceiveTokenRow& lhs, const SendReceiveTokenRow& rhs) {
                  if (lhs.icon == ":/icons/tokens/sol.png") {
                      return true;
                  }
                  if (rhs.icon == ":/icons/tokens/sol.png") {
                      return false;
                  }
                  return lhs.display.toLower() < rhs.display.toLower();
              });

    const SendReceiveTokenRow* selected = &result.rows.first();
    if (!currentIcon.isEmpty()) {
        for (const auto& row : result.rows) {
            if (row.icon == currentIcon) {
                selected = &row;
                break;
            }
        }
    }

    result.selectedIcon = selected->icon;
    result.selectedDisplay = selected->display;
    result.selectedBalance = selected->balance;
    return result;
}

QList<SendReceiveRecipientReview>
SendReceiveHandler::collectReviewRecipients(const QList<SendReceiveRecipientInput>& inputs) const {
    QList<SendReceiveRecipientReview> recipients;
    recipients.reserve(inputs.size());
    for (const auto& input : inputs) {
        const QString address = input.address.trimmed();
        bool ok = false;
        const double amount = input.amountText.trimmed().toDouble(&ok);
        if (address.isEmpty() || !ok || amount <= 0.0) {
            continue;
        }

        SendReceiveRecipientReview row;
        row.address = address;
        row.name = ContactDb::getNameByAddress(address);
        row.amountText = input.amountText.trimmed();
        row.amount = amount;
        recipients.append(row);
    }
    return recipients;
}

SendReceiveReviewData
SendReceiveHandler::buildReviewData(const SendReceiveReviewBuildRequest& request) const {
    SendReceiveReviewData review;

    review.tokenSymbol = request.currentTokenDisplayText.split(' ', Qt::SkipEmptyParts).first();
    review.tokenIcon = request.currentTokenIcon;

    if (request.tokenMeta.contains(request.currentTokenIcon)) {
        review.meta = request.tokenMeta[request.currentTokenIcon];
        review.isSol = (review.meta.mint == WSOL_MINT);
    } else {
        review.meta = {WSOL_MINT, QString(), 9, SolanaPrograms::SystemProgram};
        review.isSol = true;
    }

    review.recipients = collectReviewRecipients(request.recipientInputs);
    review.totalRecipients = review.recipients.size();
    review.numBatches = batchCount(review.totalRecipients);
    for (const auto& recipient : review.recipients) {
        review.totalAmount += recipient.amount;
    }

    review.ok = !review.recipients.isEmpty();
    return review;
}

QList<SendReceiveRecipient>
SendReceiveHandler::collectValidRecipients(const QList<SendReceiveRecipientInput>& inputs) const {
    QList<SendReceiveRecipient> recipients;
    recipients.reserve(inputs.size());
    for (const auto& input : inputs) {
        const QString address = input.address.trimmed();
        bool ok = false;
        const double amount = input.amountText.trimmed().toDouble(&ok);
        if (address.isEmpty() || !ok || amount <= 0.0) {
            continue;
        }
        recipients.append({address, amount});
    }
    return recipients;
}

bool SendReceiveHandler::hasValidRecipient(const QList<SendReceiveRecipientInput>& inputs) const {
    for (const auto& input : inputs) {
        const QString address = input.address.trimmed();
        bool ok = false;
        const double amount = input.amountText.trimmed().toDouble(&ok);
        if (!address.isEmpty() && ok && amount > 0.0) {
            return true;
        }
    }
    return false;
}

SendReceivePrepareSendResult
SendReceiveHandler::prepareSendExecution(const SendReceivePrepareSendRequest& request) const {
    SendReceivePrepareSendResult result;

    const QList<SendReceiveRecipient> recipients = collectValidRecipients(request.recipientInputs);
    if (recipients.isEmpty()) {
        result.error = SendReceivePrepareSendError::NoValidRecipients;
        return result;
    }

    QList<TransactionInstruction> instructions;
    QString instructionError;
    if (!buildSendInstructions(request.walletAddress, request.tokenMeta, recipients,
                               request.transferFeeBasisPoints, request.transferFeeMax,
                               &instructions, &instructionError)) {
        result.error = SendReceivePrepareSendError::DeriveTokenAccountFailed;
        result.errorDetail = instructionError;
        return result;
    }

    result.executionRequest.walletAddress = request.walletAddress;
    result.executionRequest.instructions = instructions;
    result.executionRequest.nonceEnabled = request.nonceEnabled;
    result.executionRequest.nonceAddress = request.nonceAddress;
    result.executionRequest.nonceValue = request.nonceValue;
    result.ok = true;
    return result;
}

bool SendReceiveHandler::buildSendInstructions(
    const QString& walletAddress, const SendReceiveTokenMeta& meta,
    const QList<SendReceiveRecipient>& recipients, quint16 transferFeeBasisPoints,
    quint64 transferFeeMax, QList<TransactionInstruction>* instructions, QString* error) const {
    if (!instructions || !error) {
        return false;
    }
    instructions->clear();
    error->clear();

    QList<TransferInstructionRecipient> plannerRecipients;
    plannerRecipients.reserve(recipients.size());
    for (const SendReceiveRecipient& recipient : recipients) {
        plannerRecipients.append({recipient.address, recipient.amount});
    }

    TransferInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mint = meta.mint;
    input.sourceTokenAccount = meta.accountAddress;
    input.decimals = meta.decimals;
    input.tokenProgram = meta.tokenProgram;
    input.recipients = plannerRecipients;
    input.transferFeeBasisPoints = transferFeeBasisPoints;
    input.transferFeeMax = transferFeeMax;
    input.maxRecipients = RECIPIENTS_PER_TX;

    const TransferInstructionBuildResult buildResult = TokenOperationBuilder::buildTransfer(input);
    if (!buildResult.ok) {
        *error = buildResult.error;
        instructions->clear();
        return false;
    }

    *instructions = buildResult.instructions;
    return true;
}

SendReceiveCreateTokenCostSummary
SendReceiveHandler::buildCreateTokenCostSummary(const QString& walletAddress, quint64 rentLamports,
                                                quint64 uploadLamports) const {
    SendReceiveCreateTokenCostSummary summary;

    const double rentSol = static_cast<double>(rentLamports) / kLamportsPerSol;
    const double uploadSol = static_cast<double>(uploadLamports) / kLamportsPerSol;
    const double totalNeeded = rentSol + uploadSol + kCreateTokenNetworkFeeSol;

    summary.rentText = QCoreApplication::translate("SendReceivePage", "Rent: %1 SOL")
                           .arg(QString::number(rentSol, 'f', 6));
    summary.totalText = QCoreApplication::translate("SendReceivePage", "Total: %1 SOL")
                            .arg(QString::number(totalNeeded, 'f', 6));

    const SendReceiveWalletSolContext walletContext = loadWalletSolContext(walletAddress);
    if (walletContext.solPriceUsd > 0.0) {
        const double usd = totalNeeded * walletContext.solPriceUsd;
        summary.totalText += QCoreApplication::translate("SendReceivePage", "  ($%1)")
                                 .arg(QString::number(usd, 'f', 2));
    }

    const double balanceSol = walletContext.walletSolBalance;
    if (balanceSol < totalNeeded) {
        summary.insufficientSol = true;
        summary.insufficientSolText =
            QCoreApplication::translate("SendReceivePage",
                                        "Insufficient SOL — need %1, wallet has %2")
                .arg(QString::number(totalNeeded, 'f', 6), QString::number(balanceSol, 'f', 6));
    }

    return summary;
}

SendReceiveWalletSolContext
SendReceiveHandler::loadWalletSolContext(const QString& walletAddress) const {
    SendReceiveWalletSolContext context;

    const auto snapshot = PortfolioDb::getLatestSnapshotRecord(walletAddress);
    if (snapshot.has_value()) {
        context.solPriceUsd = snapshot->solPrice;
    }

    const auto solAccount = TokenAccountDb::getAccountRecord(walletAddress, kSolMint);
    if (solAccount.has_value()) {
        context.walletSolBalance = solAccount->balance.toDouble();
    }

    return context;
}

quint64 SendReceiveHandler::computeMintAccountSize(const SendReceiveMintSizeInput& input) const {
    quint64 size = kMintBaseWithExtensions;

    // MetadataPointer extension (always present for token creation)
    size += kTlvHeaderLen + kMetadataPointerDataLen;

    // TokenMetadata extension (variable length — on-chain TLV has NO SPL discriminator)
    const QByteArray nameUtf8 = input.name.trimmed().toUtf8();
    const QByteArray symbolUtf8 = input.symbol.trimmed().toUtf8();
    const QByteArray uriUtf8 = input.uri.trimmed().toUtf8();
    const quint64 metadataDataSize =
        kPubkeyLen + kPubkeyLen + kBorshStringLenPrefix + nameUtf8.size() + kBorshStringLenPrefix +
        symbolUtf8.size() + kBorshStringLenPrefix + uriUtf8.size() + kMetadataAdditionalKvVecLen;
    size += kTlvHeaderLen + metadataDataSize;

    if (input.hasTransferFee) {
        size += kTlvHeaderLen + kTransferFeeDataLen;
    }
    if (input.hasNonTransferable) {
        size += kTlvHeaderLen + kNonTransferableDataLen;
    }
    if (input.hasMintClose) {
        size += kTlvHeaderLen + kMintCloseDataLen;
    }
    if (input.hasPermanentDelegate) {
        size += kTlvHeaderLen + kPermanentDelegateDataLen;
    }
    return size;
}

QString SendReceiveHandler::buildReviewCsv(const QList<SendReceiveRecipientInput>& recipientInputs,
                                           const QString& tokenSymbol, const QString& walletAddress,
                                           const QMap<QString, SendReceiveTokenMeta>& tokenMeta,
                                           const QString& currentTokenIcon) const {
    QString csv;
    csv += "Recipient,Contact Name,Amount," + tokenSymbol + "\n";

    const QList<SendReceiveRecipientReview> rows = collectReviewRecipients(recipientInputs);
    double totalAmount = 0.0;
    for (const auto& row : rows) {
        csv += escapeCsvField(row.address) + ",";
        csv += escapeCsvField(row.name) + ",";
        csv += row.amountText + ",";
        csv += tokenSymbol + "\n";
        totalAmount += row.amount;
    }

    csv += "\nTotal,," + QString::number(totalAmount, 'f', 6) + "," + tokenSymbol + "\n";
    csv += "Fee Payer," + escapeCsvField(walletAddress) + "\n";

    if (tokenMeta.contains(currentTokenIcon)) {
        const auto& meta = tokenMeta[currentTokenIcon];
        csv += "Token Mint," + escapeCsvField(meta.mint) + "\n";
        csv += "Token Program," + meta.tokenProgram + "\n";
    }

    return csv;
}
