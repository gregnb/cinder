#ifndef TXPARSEUTILS_H
#define TXPARSEUTILS_H

#include "db/TransactionDb.h"
#include "services/model/TransactionResponse.h"
#include "tx/ProgramIds.h"
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace TxParseUtils {

    // ── CU breakdown entry (program ID + units consumed) ────────────

    struct CuEntry {
        QString programId;
        int units;
    };

    // ── Friendly program name mapping ───────────────────────────────

    inline QString friendlyProgramName(const QString& programId) {
        if (programId == SolanaPrograms::SystemProgram) {
            return "System Program";
        }
        if (programId == SolanaPrograms::ComputeBudget) {
            return "Compute Budget Program";
        }
        if (programId == SolanaPrograms::TokenProgram) {
            return "Token Program";
        }
        if (programId == SolanaPrograms::Token2022Program) {
            return "Token-2022 Program";
        }
        if (programId == SolanaPrograms::AssociatedTokenAccount) {
            return "Associated Token Program";
        }
        if (programId == SolanaPrograms::MemoProgram) {
            return "Memo Program";
        }
        if (programId == SolanaPrograms::StakeProgram) {
            return "Stake Program";
        }
        if (programId == SolanaPrograms::VoteProgram) {
            return "Vote Program";
        }
        if (programId == SolanaPrograms::BpfLoader) {
            return "BPF Loader";
        }
        if (programId == SolanaPrograms::BpfUpgradeableLoader) {
            return "BPF Upgradeable Loader";
        }
        return programId.left(8) + "..." + programId.right(4);
    }

    // ── Parse per-instruction CU breakdown from transaction logs ────
    //
    // Walks log messages to find depth-1 invoke/success pairs.
    // For each top-level instruction:
    //   - If a "consumed X of Y compute units" line exists, use X.
    //   - Otherwise, distribute the remainder (totalConsumed - sumLogged)
    //     evenly among instructions that lack a consumed line.
    //
    // Returns empty list if no invoke/success pairs are found in logs
    // (caller should handle fallback, e.g. even split by instruction count).

    inline QList<CuEntry> parseCuBreakdown(const QStringList& logMessages, quint64 totalConsumed) {
        // Phase 1: track each depth-1 instruction execution
        struct InstrExec {
            QString programId;
            int consumed = -1; // -1 means no "consumed" log line
        };
        QList<InstrExec> executions;

        QRegularExpression invokeRe("Program (\\S+) invoke \\[(\\d+)\\]");
        QRegularExpression consumedRe("Program (\\S+) consumed (\\d+) of \\d+ compute units");
        QRegularExpression endRe("Program \\S+ (success|failed)");

        int depth = 0;
        QString currentProgram;
        int currentConsumed = -1;

        for (const QString& line : logMessages) {
            QRegularExpressionMatch m;

            m = invokeRe.match(line);
            if (m.hasMatch()) {
                int newDepth = m.captured(2).toInt();
                if (newDepth == 1) {
                    currentProgram = m.captured(1);
                    currentConsumed = -1;
                }
                depth = newDepth;
                continue;
            }

            m = consumedRe.match(line);
            if (m.hasMatch() && depth == 1) {
                currentConsumed = m.captured(2).toInt();
                continue;
            }

            m = endRe.match(line);
            if (m.hasMatch() && depth > 0) {
                if (depth == 1) {
                    InstrExec exec;
                    exec.programId = currentProgram;
                    exec.consumed = currentConsumed;
                    executions.append(exec);
                }
                depth--;
            }
        }

        if (executions.isEmpty()) {
            return {};
        }

        // Phase 2: distribute unlogged CU
        int sumLogged = 0;
        int numUnlogged = 0;
        for (const auto& exec : executions) {
            if (exec.consumed >= 0) {
                sumLogged += exec.consumed;
            } else {
                numUnlogged++;
            }
        }

        int remainder = static_cast<int>(totalConsumed) - sumLogged;
        int perUnlogged = numUnlogged > 0 ? remainder / numUnlogged : 0;
        int unloggedRemainder = numUnlogged > 0 ? remainder % numUnlogged : 0;

        QList<CuEntry> result;
        int unloggedIdx = 0;
        for (const auto& exec : executions) {
            CuEntry entry;
            entry.programId = exec.programId;

            if (exec.consumed >= 0) {
                entry.units = exec.consumed;
            } else {
                entry.units = perUnlogged;
                if (unloggedIdx == numUnlogged - 1) {
                    entry.units += unloggedRemainder;
                }
                unloggedIdx++;
            }

            result.append(entry);
        }

        return result;
    }

    // ── Extract activities from a parsed transaction ─────────────
    //
    // Walks top-level + inner instructions and returns Activity entries
    // for System Program transfers, SPL Token transfers/mints/burns, etc.
    // walletAddress is used to determine send vs receive direction.

    QList<Activity> extractActivities(const TransactionResponse& tx, const QString& walletAddress);

} // namespace TxParseUtils

#endif // TXPARSEUTILS_H
