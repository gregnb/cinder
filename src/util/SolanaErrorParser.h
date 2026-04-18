#ifndef SOLANAERORPARSER_H
#define SOLANAERORPARSER_H

#include <QRegularExpression>
#include <QString>

namespace SolanaErrorParser {

    /// Translate a raw SolanaApi error string into a human-friendly message.
    /// Returns a two-line string: line 1 = friendly summary, line 2 = technical detail.
    inline QString humanize(const QString& raw) {
        // ── HTTP-level errors ──
        if (raw.contains("HTTP 429"))
            return "Rate limited by the RPC node. Please wait a moment and try again.";
        if (raw.contains("HTTP 502") || raw.contains("HTTP 503"))
            return "The RPC node is temporarily unavailable. Try again shortly.";
        if (raw.startsWith("HTTP "))
            return "Network error communicating with the Solana RPC node.";

        // ── Blockhash / timing ──
        if (raw.contains("Blockhash not found", Qt::CaseInsensitive))
            return "Transaction expired — the network didn't process it in time.\n"
                   "This is normal. Please try again.";

        // ── Instruction-level errors ──
        // Extract the inner error description after "Error processing Instruction N:"
        static const QRegularExpression ixErrorRe(R"(Error processing Instruction \d+:\s*(.+))",
                                                  QRegularExpression::CaseInsensitiveOption);

        QRegularExpressionMatch m = ixErrorRe.match(raw);
        QString inner = m.hasMatch() ? m.captured(1).trimmed() : QString();

        if (!inner.isEmpty()) {
            // custom program error: 0xHEX
            static const QRegularExpression customErrRe(
                R"(custom program error:\s*0x([0-9a-fA-F]+))");
            QRegularExpressionMatch cm = customErrRe.match(inner);

            if (cm.hasMatch()) {
                bool ok = false;
                uint code = cm.captured(1).toUInt(&ok, 16);
                if (ok) {
                    switch (code) {
                    case 0x0:
                        return "An account required by this transaction already exists.\n"
                               "If you just submitted a stake, wait a moment and try again.";
                    case 0x1:
                        return "Insufficient funds. Your wallet doesn't have enough SOL\n"
                               "to cover the stake amount plus the rent-exempt reserve (~0.00228 "
                               "SOL).";
                    case 0x2:
                        return "Invalid program for this operation.\n"
                               "This is an internal error — please report it.";
                    case 0x3:
                        return "The stake account data size is incorrect.\n"
                               "This is an internal error — please report it.";
                    case 0x5:
                        return "The stake account is not yet fully activated.\n"
                               "Wait for the current epoch to end before taking this action.";
                    case 0x6:
                        return "This stake account is already being deactivated.\n"
                               "Wait for deactivation to complete, then withdraw.";
                    default:
                        return QString("The transaction was rejected (program error 0x%1).\n"
                                       "This may be a temporary issue — try again or check your "
                                       "balance.")
                            .arg(code, 0, 16);
                    }
                }
            }

            // Plain-text inner errors
            if (inner.contains("insufficient funds", Qt::CaseInsensitive))
                return "Insufficient funds to cover the transaction fee.\n"
                       "Make sure you have enough SOL for both the stake and network fees.";

            if (inner.contains("already deactivated", Qt::CaseInsensitive) ||
                inner.contains("not delegated", Qt::CaseInsensitive))
                return "This stake account is not currently active.\n"
                       "It may have already been deactivated.";

            if (inner.contains("account not found", Qt::CaseInsensitive))
                return "The stake account was not found on-chain.\n"
                       "It may have already been closed or withdrawn.";

            if (inner.contains("incorrect program id", Qt::CaseInsensitive))
                return "Wrong program for this account.\n"
                       "The account is not a valid stake account.";

            // Minimum delegation
            if (inner.contains("minimum delegation", Qt::CaseInsensitive))
                return "The amount is below the minimum stake delegation.\n"
                       "You need to stake at least the minimum required amount.";
        }

        // ── Simulation-level (non-instruction) errors ──
        if (raw.contains("simulation failed", Qt::CaseInsensitive)) {
            if (raw.contains("AccountNotFound", Qt::CaseInsensitive))
                return "One of the accounts in this transaction was not found.\n"
                       "Make sure your wallet is funded and try again.";
            if (raw.contains("InsufficientFundsForFee", Qt::CaseInsensitive))
                return "Not enough SOL to pay the transaction fee.\n"
                       "You need a small amount of SOL beyond the stake amount for fees.";
        }

        // ── Node health ──
        if (raw.contains("Node is behind", Qt::CaseInsensitive) ||
            raw.contains("node is unhealthy", Qt::CaseInsensitive))
            return "The RPC node is still syncing. Please try again in a few moments.";

        // ── Fallback: return the raw message but with a friendlier prefix ──
        return QString("Transaction failed.\n%1").arg(raw);
    }

} // namespace SolanaErrorParser

#endif // SOLANAERORPARSER_H
