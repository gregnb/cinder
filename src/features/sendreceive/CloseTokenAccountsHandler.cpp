#include "features/sendreceive/CloseTokenAccountsHandler.h"

#include "db/TokenAccountDb.h"
#include "tx/TokenInstruction.h"

#include <QCoreApplication>

namespace {
    constexpr double kRecoverableSolPerAccount = 0.00203;
} // namespace

CloseTokenAccountsHandler::CloseTokenAccountsHandler(SendReceiveHandler* sharedHandler)
    : m_sharedHandler(sharedHandler) {}

QList<CloseTokenAccountEntry>
CloseTokenAccountsHandler::loadAccountEntries(const QString& ownerAddress) const {
    QList<CloseTokenAccountEntry> result;
    if (ownerAddress.isEmpty()) {
        return result;
    }

    const auto accounts = TokenAccountDb::getAccountsByOwnerRecords(ownerAddress);
    for (const auto& acct : accounts) {
        bool ok = false;
        const double bal = acct.balance.toDouble(&ok);
        if (!ok || bal != 0.0) {
            continue;
        }

        CloseTokenAccountEntry entry;
        entry.symbol = acct.symbol.isEmpty() ? acct.tokenAddress.left(6) : acct.symbol;
        entry.accountAddress = acct.accountAddress;
        entry.balance = acct.balance;
        entry.tokenProgram = acct.tokenProgram;
        result.append(entry);
    }
    return result;
}

QString CloseTokenAccountsHandler::summaryText(int selectedCount) const {
    if (selectedCount <= 0) {
        return QCoreApplication::translate("SendReceivePage", "Select accounts to close");
    }

    const double recoverable = selectedCount * kRecoverableSolPerAccount;
    const QString recoverableText = m_sharedHandler
                                        ? m_sharedHandler->formatCryptoAmount(recoverable)
                                        : QString::number(recoverable, 'f', 6);

    return QCoreApplication::translate("SendReceivePage",
                                       "%1 account%2 selected  ·  ~%3 SOL recoverable")
        .arg(selectedCount)
        .arg(selectedCount == 1 ? "" : "s")
        .arg(recoverableText);
}

bool CloseTokenAccountsHandler::hasSelection(int selectedCount) const { return selectedCount > 0; }

QList<TransactionInstruction> CloseTokenAccountsHandler::buildCloseInstructions(
    const QString& walletAddress, const QList<CloseTokenAccountEntry>& entries) const {
    QList<TransactionInstruction> instructions;
    for (const auto& entry : entries) {
        const QString program =
            entry.tokenProgram.isEmpty() ? SolanaPrograms::TokenProgram : entry.tokenProgram;
        instructions.append(TokenInstruction::closeAccount(entry.accountAddress, walletAddress,
                                                           walletAddress, program));
    }
    return instructions;
}

void CloseTokenAccountsHandler::executeCloseFlow(
    const QString& walletAddress, const QList<CloseTokenAccountEntry>& entries,
    SolanaApi* solanaApi, Signer* signer, QObject* context,
    const SendReceiveHandler::ExecuteSendCallbacks& callbacks) const {
    if (!m_sharedHandler || entries.isEmpty()) {
        return;
    }

    const auto instructions = buildCloseInstructions(walletAddress, entries);
    if (instructions.isEmpty()) {
        if (callbacks.onStatus) {
            callbacks.onStatus(
                QCoreApplication::translate("SendReceivePage",
                                            "Error: could not prepare close transaction."),
                true);
        }
        if (callbacks.onFinished) {
            callbacks.onFinished();
        }
        return;
    }

    SendReceiveExecutionRequest request;
    request.walletAddress = walletAddress;
    request.instructions = instructions;
    request.nonceEnabled = false;
    m_sharedHandler->executeSendFlow(request, solanaApi, signer, context, callbacks);
}
