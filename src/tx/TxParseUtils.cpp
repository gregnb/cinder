#include "TxParseUtils.h"
#include "ProgramIds.h"
#include <QHash>

namespace TxParseUtils {

    QList<Activity> extractActivities(const TransactionResponse& tx, const QString& walletAddress) {
        QList<Activity> activities;

        auto isTokenProgram = [&](const QString& pid) {
            return SolanaPrograms::isTokenProgram(pid);
        };

        // Build token-account → owner mapping from pre/postTokenBalances.
        // SPL instructions reference token account addresses (ATAs), not wallet addresses.
        // This map lets us resolve ATAs back to their owner wallets.
        QHash<QString, QString> tokenAccountOwners;
        auto addBalances = [&](const QList<TokenBalance>& balances) {
            for (const auto& tb : balances) {
                if (tb.accountIndex >= 0 && tb.accountIndex < tx.message.accountKeys.size()) {
                    QString tokenAddr = tx.message.accountKeys[tb.accountIndex].pubkey;
                    if (!tb.owner.isEmpty()) {
                        tokenAccountOwners[tokenAddr] = tb.owner;
                    }
                }
            }
        };
        addBalances(tx.meta.preTokenBalances);
        addBalances(tx.meta.postTokenBalances);

        // Resolve a token account address to its owner wallet, or return as-is if unknown.
        auto resolveOwner = [&](const QString& addr) -> QString {
            return tokenAccountOwners.value(addr, addr);
        };

        auto involvesWallet = [&](const Activity& a) {
            return a.fromAddress == walletAddress || a.toAddress == walletAddress;
        };

        auto parseInstruction = [&](const Instruction& instr) {
            // ── System Program ──────────────────────────────
            if (instr.programId == SolanaPrograms::SystemProgram) {
                if (instr.type == "transfer") {
                    Activity a;
                    a.token = "SOL";
                    a.fromAddress = instr.info["source"].toString();
                    a.toAddress = instr.info["destination"].toString();
                    a.amount = instr.info["lamports"].toDouble() / 1e9;
                    a.activityType = (a.fromAddress == walletAddress) ? "send" : "receive";
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                } else if (instr.type == "createAccount") {
                    Activity a;
                    a.activityType = "create_account";
                    a.token = "SOL";
                    a.fromAddress = instr.info["source"].toString();
                    a.toAddress = instr.info["newAccount"].toString();
                    a.amount = instr.info["lamports"].toDouble() / 1e9;
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                } else if (instr.type == "initializeNonce") {
                    Activity a;
                    a.activityType = "create_nonce";
                    a.token = "SOL";
                    a.fromAddress = instr.info["nonceAuthority"].toString();
                    a.toAddress = instr.info["nonceAccount"].toString();
                    a.amount = 0;
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                }
            }
            // ── SPL Token / Token-2022 ──────────────────────
            else if (isTokenProgram(instr.programId)) {
                if (instr.type == "transfer" || instr.type == "transferChecked") {
                    Activity a;
                    a.token = instr.info["mint"].toString();
                    if (a.token.isEmpty()) {
                        a.token = "unknown-spl";
                    }
                    // Resolve token account addresses to owner wallets
                    a.fromAddress = resolveOwner(instr.info["source"].toString());
                    a.toAddress = resolveOwner(instr.info["destination"].toString());
                    if (instr.type == "transferChecked") {
                        a.amount = instr.info["tokenAmount"].toObject()["uiAmount"].toDouble();
                    } else {
                        a.amount = instr.info["amount"].toString().toDouble();
                    }
                    a.activityType = (a.fromAddress == walletAddress) ? "send" : "receive";
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                } else if (instr.type == "mintTo" || instr.type == "mintToChecked") {
                    Activity a;
                    a.activityType = "mint";
                    a.token = instr.info["mint"].toString();
                    a.fromAddress = instr.info["mintAuthority"].toString();
                    a.toAddress = resolveOwner(instr.info["account"].toString());
                    if (instr.type == "mintToChecked") {
                        a.amount = instr.info["tokenAmount"].toObject()["uiAmount"].toDouble();
                    } else {
                        a.amount = instr.info["amount"].toString().toDouble();
                    }
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                } else if (instr.type == "burn" || instr.type == "burnChecked") {
                    Activity a;
                    a.activityType = "burn";
                    a.token = instr.info["mint"].toString();
                    a.fromAddress = resolveOwner(instr.info["account"].toString());
                    a.toAddress.clear();
                    if (instr.type == "burnChecked") {
                        a.amount = instr.info["tokenAmount"].toObject()["uiAmount"].toDouble();
                    } else {
                        a.amount = instr.info["amount"].toString().toDouble();
                    }
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                } else if (instr.type == "closeAccount") {
                    Activity a;
                    a.activityType = "close_account";
                    a.token = "SOL";
                    a.fromAddress = resolveOwner(instr.info["account"].toString());
                    a.toAddress = instr.info["destination"].toString();
                    a.amount = 0;
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                } else if (instr.type == "initializeAccount" ||
                           instr.type == "initializeAccount2" ||
                           instr.type == "initializeAccount3") {
                    Activity a;
                    a.activityType = "init_account";
                    a.token = instr.info["mint"].toString();
                    a.fromAddress = instr.info["owner"].toString();
                    a.toAddress = instr.info["account"].toString();
                    a.amount = 0;
                    if (involvesWallet(a)) {
                        activities.append(a);
                    }
                }
            }
        };

        for (const auto& instr : tx.message.instructions) {
            parseInstruction(instr);
        }
        for (const auto& innerSet : tx.meta.innerInstructions) {
            for (const auto& instr : innerSet.instructions) {
                parseInstruction(instr);
            }
        }

        return activities;
    }

} // namespace TxParseUtils
