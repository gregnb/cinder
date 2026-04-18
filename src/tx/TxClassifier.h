#ifndef TXCLASSIFIER_H
#define TXCLASSIFIER_H

#include "services/model/TransactionResponse.h"
#include "tx/KnownTokens.h"
#include "tx/ProgramIds.h"
#include <QCoreApplication>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <cmath>

namespace TxClassifier {

    // ── Transaction type enum ────────────────────────────────────

    enum class TxType {
        Failed,
        SolTransfer,
        TokenTransfer,
        Swap,
        Stake,
        Unstake,
        CreateAccount,
        CreateTokenAccount,
        TokenMint,
        TokenBurn,
        CloseTokenAccount,
        Unknown
    };

    // ── Classification result ────────────────────────────────────

    struct Classification {
        TxType type = TxType::Unknown;
        QString label; // e.g. "Swap", "SOL Transfer"

        // Transfer details
        QString from;        // sender address
        QString to;          // first receiver address
        double amount = 0;   // total amount transferred/swapped
        double toAmount = 0; // first receiver's individual amount
        QString mint;        // token mint (empty for SOL)
        QString tokenSymbol; // resolved symbol if known

        // Multi-recipient transfers (beyond the first)
        struct Recipient {
            QString address;
            double amount = 0;
        };
        QList<Recipient> extraRecipients;

        // Swap details
        double amountIn = 0;
        QString mintIn;
        QString symbolIn;
        double amountOut = 0;
        QString mintOut;
        QString symbolOut;
        QString dexProgramId; // DEX that executed the swap
    };

    // ── Known DEX program IDs ────────────────────────────────────

    inline bool isDexProgram(const QString& id) {
        static const QSet<QString> dex = {
            // Jupiter
            "JUP6LkbZbjS1jKKwapdHNy74zcZ3tLUZoi5QNyVTaV4",
            "JUP4Fb2cqiRUcaTHdrPC8h2gNsA2ETXiPDD33WcGuJB",
            "JUP3c2Uh3WA4Ng34tw6kPd2G4C5BB21Xo36Je1s32Ph",
            "JUP2jxvXaqu7NQY1GmNF4m1vodw12LVXYxbGL2VB5Rz",
            "DCA265Vj8a9CEuX1eb1LWRnDT7uK6q1xMipnNyatn23M",
            "j1to2NAwzMibodJ3GmAqJPMcwyDriC6MfkKM5pXVsWj",
            "jupoNjAxXgZ4rjzxzPMP4oxduvQsQtZzyknqvzYNrNu",
            "PERPHjGBqRHArX4DySjwM6UJHiR3sWAatqfdBS2qQJu",
            // Raydium
            "675kPX9MHTjS2zt1qfr1NYHuzeLXfQM9H24wFSUt1Mp8",
            "CAMMCzo5YL8w4VFF8KVHrK22GGUsp5VTaW7grrKgrWqK",
            "CPMMoo8L3F4NbTegBCKVNunggL7H1ZpdTHKxQB5qKP1C",
            // Orca
            "whirLbMiicVdio4qvUfM5KAg6Ct8VwpYzGff3uctyCc",
            "DjVE6JNiYqPL2QXyCUUh8rNjHrbz9hXHNYt99MQ59qw1",
            "9W959DqEETiGZocYWCQPaJ6sBmUzgfxXfqGeTEdp3aQP",
            // Meteora
            "LBUZKhRxPF3XUpBCjp4YzTKgLccjZhTSDM9YuVaPwxo",
            "Eo7WjKq67rjJQSZxS6z3YkapzY3eMj6Xy8X5EQVn5UaB",
            // Phoenix
            "PhoeNiXZ8ByJGLkxNfZRnkUfjvmuYqLR89jjFHGqdXY",
            // Lifinity
            "EewxydAPCCVuNEyrVN68PuSYdQ7wKn27V9Gjeoi8dy3S",
            "2wT8Yq49kHgDzXuPxZSaeLaH1qbmGXtEyPy64bL7aD3c",
            // OpenBook / Serum
            "opnb2LAfJYbRMAHHvqjCwQxanZn7ReEHp1k81EQMQvR",
            "srmqPvymJeFKQ4zGQed1GFppgkRHL9kaELCbyksJtPX",
            "9xQeWvG816bUx9EPjHmaT23yvVM2ZWbrrpZb9PusVFin",
            // Pump.fun
            "6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P",
            // Saber
            "SSwpkEEcbUqx4vtoEByFjSkhKdCT862DNVb52nZg1UZ",
            // Fluxbeam
            "FLUXubRmkEi2q6K3Y5o2pmh97cwLOwH2QUEoSPJP1uf",
            // GooseFX
            "GFXsSL5sSaDfNFQUYsHekbWBW1TsFdjDYzACh62tEHxn",
            // Aldrin
            "AMM55ShdkoGRB5jVYPjWziwk8m5MpwyDgsMWHaMSQWH6",
            // Crema
            "CLMM9tUoggJu2wagPkkqs9eFG4BWhVBZWkP1qv3Sp7tR",
        };
        return dex.contains(id);
    }

    inline QString dexName(const QString& id) {
        static const QMap<QString, QString> names = {
            {"JUP6LkbZbjS1jKKwapdHNy74zcZ3tLUZoi5QNyVTaV4", "Jupiter"},
            {"JUP4Fb2cqiRUcaTHdrPC8h2gNsA2ETXiPDD33WcGuJB", "Jupiter"},
            {"JUP3c2Uh3WA4Ng34tw6kPd2G4C5BB21Xo36Je1s32Ph", "Jupiter"},
            {"JUP2jxvXaqu7NQY1GmNF4m1vodw12LVXYxbGL2VB5Rz", "Jupiter"},
            {"j1to2NAwzMibodJ3GmAqJPMcwyDriC6MfkKM5pXVsWj", "Jupiter"},
            {"DCA265Vj8a9CEuX1eb1LWRnDT7uK6q1xMipnNyatn23M", "Jupiter DCA"},
            {"jupoNjAxXgZ4rjzxzPMP4oxduvQsQtZzyknqvzYNrNu", "Jupiter Limit Order"},
            {"PERPHjGBqRHArX4DySjwM6UJHiR3sWAatqfdBS2qQJu", "Jupiter Perps"},
            {"675kPX9MHTjS2zt1qfr1NYHuzeLXfQM9H24wFSUt1Mp8", "Raydium"},
            {"CAMMCzo5YL8w4VFF8KVHrK22GGUsp5VTaW7grrKgrWqK", "Raydium CLMM"},
            {"CPMMoo8L3F4NbTegBCKVNunggL7H1ZpdTHKxQB5qKP1C", "Raydium"},
            {"whirLbMiicVdio4qvUfM5KAg6Ct8VwpYzGff3uctyCc", "Orca"},
            {"DjVE6JNiYqPL2QXyCUUh8rNjHrbz9hXHNYt99MQ59qw1", "Orca v1"},
            {"9W959DqEETiGZocYWCQPaJ6sBmUzgfxXfqGeTEdp3aQP", "Orca v2"},
            {"LBUZKhRxPF3XUpBCjp4YzTKgLccjZhTSDM9YuVaPwxo", "Meteora DLMM"},
            {"Eo7WjKq67rjJQSZxS6z3YkapzY3eMj6Xy8X5EQVn5UaB", "Meteora"},
            {"PhoeNiXZ8ByJGLkxNfZRnkUfjvmuYqLR89jjFHGqdXY", "Phoenix"},
            {"EewxydAPCCVuNEyrVN68PuSYdQ7wKn27V9Gjeoi8dy3S", "Lifinity"},
            {"2wT8Yq49kHgDzXuPxZSaeLaH1qbmGXtEyPy64bL7aD3c", "Lifinity v2"},
            {"6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P", "Pump.fun"},
            {"SSwpkEEcbUqx4vtoEByFjSkhKdCT862DNVb52nZg1UZ", "Saber"},
            {"opnb2LAfJYbRMAHHvqjCwQxanZn7ReEHp1k81EQMQvR", "OpenBook"},
            {"srmqPvymJeFKQ4zGQed1GFppgkRHL9kaELCbyksJtPX", "Serum v3"},
            {"9xQeWvG816bUx9EPjHmaT23yvVM2ZWbrrpZb9PusVFin", "Serum v2"},
            {"FLUXubRmkEi2q6K3Y5o2pmh97cwLOwH2QUEoSPJP1uf", "Fluxbeam"},
            {"GFXsSL5sSaDfNFQUYsHekbWBW1TsFdjDYzACh62tEHxn", "GooseFX"},
            {"AMM55ShdkoGRB5jVYPjWziwk8m5MpwyDgsMWHaMSQWH6", "Aldrin"},
            {"CLMM9tUoggJu2wagPkkqs9eFG4BWhVBZWkP1qv3Sp7tR", "Crema"},
        };
        return names.value(id, id.left(8) + "...");
    }

    // ── Token delta helper ───────────────────────────────────────

    struct TokenDelta {
        QString owner;
        QString mint;
        double delta;
    };

    inline QList<TokenDelta> computeTokenDeltas(const TransactionResponse& tx) {
        // Key on accountIndex (not owner+mint) to preserve per-account
        // granularity. This is critical for same-token arbitrage swaps
        // where the signer sends USDC from one account and receives USDC
        // into another — grouping by (owner, mint) would collapse these
        // into a net delta, hiding the swap.
        struct Entry {
            QString owner;
            QString mint;
            double pre = 0;
            double post = 0;
        };
        QMap<int, Entry> map;

        for (const auto& tb : tx.meta.preTokenBalances) {
            map[tb.accountIndex].owner = tb.owner;
            map[tb.accountIndex].mint = tb.mint;
            map[tb.accountIndex].pre = tb.amount.uiAmount;
        }
        for (const auto& tb : tx.meta.postTokenBalances) {
            if (map[tb.accountIndex].owner.isEmpty())
                map[tb.accountIndex].owner = tb.owner;
            map[tb.accountIndex].mint = tb.mint;
            map[tb.accountIndex].post = tb.amount.uiAmount;
        }

        QList<TokenDelta> result;
        for (auto it = map.begin(); it != map.end(); ++it) {
            double d = it.value().post - it.value().pre;
            if (std::abs(d) > 1e-12)
                result.append({it.value().owner, it.value().mint, d});
        }
        return result;
    }

    // ── Classify ─────────────────────────────────────────────────

    inline Classification classify(const TransactionResponse& tx,
                                   const QString& walletAddress = {}) {
        using namespace SolanaPrograms;
        Classification cls;

        // ── Rule 0: Failed ──
        if (tx.meta.hasError) {
            cls.type = TxType::Failed;
            cls.label = QCoreApplication::translate("TxClassifier", "Failed");
            return cls;
        }

        // Find signer (first signer in accountKeys)
        QString signer;
        for (const auto& ak : tx.message.accountKeys) {
            if (ak.signer) {
                signer = ak.pubkey;
                break;
            }
        }

        // Collect meaningful top-level instructions (skip housekeeping)
        QList<const Instruction*> meaningful;
        QSet<QString> topPrograms;
        for (const auto& ix : tx.message.instructions) {
            if (ix.programId == ComputeBudget)
                continue;
            // AdvanceNonce is bookkeeping, not part of the logical operation
            if (ix.programId == SystemProgram && ix.isParsed() && ix.type == "advanceNonce")
                continue;
            meaningful.append(&ix);
            topPrograms.insert(ix.programId);
        }

        // ── Rule 1: Stake / Unstake ──
        for (const auto* ix : meaningful) {
            if (ix->programId == StakeProgram && ix->isParsed()) {
                if (ix->type == "delegate" || ix->type == "initialize") {
                    cls.type = TxType::Stake;
                    cls.label = QCoreApplication::translate("TxClassifier", "Stake");
                    return cls;
                }
                if (ix->type == "deactivate" || ix->type == "withdraw") {
                    cls.type = TxType::Unstake;
                    cls.label = QCoreApplication::translate("TxClassifier", "Unstake");
                    return cls;
                }
            }
        }

        // ── Rule 2: Swap (bidirectional token flow) ──
        {
            // Check for known DEX program (for naming)
            bool hasDex = false;
            QString dexPid;
            for (const auto& pid : topPrograms) {
                if (isDexProgram(pid)) {
                    hasDex = true;
                    dexPid = pid;
                    break;
                }
            }
            if (!hasDex) {
                for (const auto& inner : tx.meta.innerInstructions) {
                    for (const auto& ix : inner.instructions) {
                        if (isDexProgram(ix.programId)) {
                            hasDex = true;
                            dexPid = ix.programId;
                            break;
                        }
                    }
                    if (hasDex)
                        break;
                }
            }

            // Always check for bidirectional token flow on the signer
            auto deltas = computeTokenDeltas(tx);

            bool hasIncrease = false, hasDecrease = false;
            TokenDelta bestIn{}, bestOut{};
            for (const auto& d : deltas) {
                if (d.owner == signer) {
                    if (d.delta > 0) {
                        hasIncrease = true;
                        if (d.delta > bestOut.delta)
                            bestOut = d;
                    }
                    if (d.delta < 0) {
                        hasDecrease = true;
                        if (d.delta < bestIn.delta)
                            bestIn = d;
                    }
                }
            }

            // Check SOL balance for wrapped SOL swaps
            qint64 solChange = 0;
            if (!hasIncrease || !hasDecrease) {
                for (int i = 0; i < tx.message.accountKeys.size(); ++i) {
                    if (tx.message.accountKeys[i].pubkey == signer &&
                        i < tx.meta.preBalances.size() && i < tx.meta.postBalances.size()) {
                        solChange = tx.meta.postBalances[i] - tx.meta.preBalances[i] + tx.meta.fee;
                        break;
                    }
                }
                // Subtract explicit top-level system transfers — these are
                // intentional SOL payments (fee funding, tips), not implicit
                // wSOL wrap/unwrap from a swap (which happen in inner ixs).
                for (const auto* ix : meaningful) {
                    if (ix->program == "system" && ix->type == "transfer") {
                        auto lam = static_cast<qint64>(ix->info.value("lamports").toDouble());
                        if (ix->info.value("source").toString() == signer)
                            solChange += lam;
                        else if (ix->info.value("destination").toString() == signer)
                            solChange -= lam;
                    }
                }
                if (solChange < -10000 && hasIncrease)
                    hasDecrease = true;
                if (solChange > 10000 && hasDecrease)
                    hasIncrease = true;
            }

            // Swap if: (known DEX + bidirectional) OR (different-mint bidirectional)
            bool bidirectional = hasIncrease && hasDecrease;
            bool differentMints =
                bestIn.mint != bestOut.mint || bestIn.mint.isEmpty() || bestOut.mint.isEmpty();

            if (bidirectional && (hasDex || differentMints)) {
                cls.type = TxType::Swap;

                // If no known DEX, find the main program (the router/DEX)
                if (!hasDex) {
                    for (const auto* ix : meaningful) {
                        if (ix->programId != SystemProgram && ix->programId != TokenProgram &&
                            ix->programId != Token2022Program &&
                            ix->programId != AssociatedTokenAccount) {
                            dexPid = ix->programId;
                            break;
                        }
                    }
                }
                cls.dexProgramId = dexPid;

                // Token sent (in)
                if (!bestIn.mint.isEmpty()) {
                    cls.amountIn = std::abs(bestIn.delta);
                    cls.mintIn = bestIn.mint;
                } else if (solChange < -10000) {
                    cls.amountIn = std::abs(solChange) / 1e9;
                    cls.mintIn = WSOL_MINT;
                }

                // Token received (out)
                if (!bestOut.mint.isEmpty()) {
                    cls.amountOut = bestOut.delta;
                    cls.mintOut = bestOut.mint;
                } else if (solChange > 10000) {
                    cls.amountOut = solChange / 1e9;
                    cls.mintOut = WSOL_MINT;
                }

                // Same mint in and out = circular arbitrage
                cls.label = (!cls.mintIn.isEmpty() && cls.mintIn == cls.mintOut)
                                ? QCoreApplication::translate("TxClassifier", "Swap Arbitrage")
                                : QCoreApplication::translate("TxClassifier", "Swap");
                return cls;
            }

            // ── Same-token arbitrage: known DEX but no bidirectional net flow
            //    (tokens go out and back to the same account, so pre/post shows
            //    only the net gain). Scan inner instruction transfers to find
            //    the gross in/out amounts.
            if (hasDex && (hasIncrease || hasDecrease)) {
                // Build account→mint and mint→decimals maps from token balances
                QSet<QString> signerTokenAccounts;
                QMap<QString, QString> accountToMint;
                QMap<QString, int> mintDecimals;

                auto collectFromBalances = [&](const auto& balances) {
                    for (const auto& tb : balances) {
                        if (tb.accountIndex < tx.message.accountKeys.size()) {
                            QString addr = tx.message.accountKeys[tb.accountIndex].pubkey;
                            accountToMint[addr] = tb.mint;
                            mintDecimals[tb.mint] = tb.amount.decimals;
                            if (tb.owner == signer)
                                signerTokenAccounts.insert(addr);
                        }
                    }
                };
                collectFromBalances(tx.meta.preTokenBalances);
                collectFromBalances(tx.meta.postTokenBalances);

                // Scan inner instructions for token transfers to/from signer
                double grossOut = 0, grossIn = 0;
                QString outMint, inMint;

                for (const auto& inner : tx.meta.innerInstructions) {
                    for (const auto& ix : inner.instructions) {
                        if ((ix.program == "spl-token" || ix.program == "spl-token-2022") &&
                            (ix.type == "transfer" || ix.type == "transferChecked")) {
                            QString src = ix.info.value("source").toString();
                            QString dst = ix.info.value("destination").toString();

                            double amt = 0;
                            QString mint;
                            if (ix.type == "transferChecked") {
                                // transferChecked: has uiAmount + mint
                                amt = ix.info["tokenAmount"].toObject()["uiAmount"].toDouble();
                                mint = ix.info["mint"].toString();
                            } else {
                                // Plain transfer: raw integer amount, no mint field
                                double raw = ix.info.value("amount").toString().toDouble();
                                mint = accountToMint.value(src);
                                if (mint.isEmpty())
                                    mint = accountToMint.value(dst);
                                int dec = mintDecimals.value(mint, 6);
                                amt = raw / std::pow(10.0, dec);
                            }

                            if (signerTokenAccounts.contains(src) && amt > grossOut) {
                                grossOut = amt;
                                outMint = mint;
                            }
                            if (signerTokenAccounts.contains(dst) && amt > grossIn) {
                                grossIn = amt;
                                inMint = mint;
                            }
                        }
                    }
                }

                // If we found transfers in both directions, it's an arbitrage swap
                if (grossOut > 0 && grossIn > 0) {
                    cls.type = TxType::Swap;
                    cls.dexProgramId = dexPid;
                    cls.amountIn = grossOut;
                    cls.mintIn =
                        outMint.isEmpty() ? (hasDecrease ? bestIn.mint : bestOut.mint) : outMint;
                    cls.amountOut = grossIn;
                    cls.mintOut =
                        inMint.isEmpty() ? (hasIncrease ? bestOut.mint : bestIn.mint) : inMint;
                    cls.label = (!cls.mintIn.isEmpty() && cls.mintIn == cls.mintOut)
                                    ? QCoreApplication::translate("TxClassifier", "Swap Arbitrage")
                                    : QCoreApplication::translate("TxClassifier", "Swap");
                    return cls;
                }
            }
        }

        // ── Rule 3: Token Transfer ──
        {
            bool hasTokenTransfer = false;
            for (const auto* ix : meaningful) {
                if ((ix->program == "spl-token" || ix->program == "spl-token-2022") &&
                    (ix->type == "transfer" || ix->type == "transferChecked")) {
                    hasTokenTransfer = true;
                    break;
                }
            }
            if (!hasTokenTransfer) {
                for (const auto& inner : tx.meta.innerInstructions) {
                    for (const auto& ix : inner.instructions) {
                        if ((ix.program == "spl-token" || ix.program == "spl-token-2022") &&
                            (ix.type == "transfer" || ix.type == "transferChecked")) {
                            hasTokenTransfer = true;
                            break;
                        }
                    }
                    if (hasTokenTransfer)
                        break;
                }
            }

            if (hasTokenTransfer) {
                auto deltas = computeTokenDeltas(tx);
                // Use wallet perspective when available, but if the selected
                // wallet is not actually involved in the token deltas, fall
                // back to a real participant to avoid rendering "Receive — 0".
                QString perspective = walletAddress.isEmpty() ? signer : walletAddress;
                bool walletDecreased = false, walletIncreased = false;
                bool foundPerspectiveDelta = false;
                TokenDelta walletDelta{};

                auto tryPerspective = [&](const QString& candidate) {
                    bool candidateDecreased = false;
                    bool candidateIncreased = false;
                    TokenDelta candidateDelta{};
                    bool found = false;

                    for (const auto& d : deltas) {
                        if (d.owner != candidate) {
                            continue;
                        }
                        if (d.delta < 0) {
                            candidateDecreased = true;
                            candidateDelta = d;
                            found = true;
                        }
                        if (d.delta > 0) {
                            candidateIncreased = true;
                            candidateDelta = d;
                            found = true;
                        }
                    }

                    if (found) {
                        perspective = candidate;
                        walletDecreased = candidateDecreased;
                        walletIncreased = candidateIncreased;
                        walletDelta = candidateDelta;
                        foundPerspectiveDelta = true;
                    }
                };

                tryPerspective(perspective);
                if (!foundPerspectiveDelta && !signer.isEmpty() && signer != perspective) {
                    tryPerspective(signer);
                }
                if (!foundPerspectiveDelta && !deltas.isEmpty()) {
                    tryPerspective(deltas.first().owner);
                }
                if (foundPerspectiveDelta) {
                    cls.type = TxType::TokenTransfer;
                    cls.amount = std::abs(walletDelta.delta);
                    cls.mint = walletDelta.mint;
                    cls.from = perspective;

                    // Find the counterparty (other owner with opposite delta on same mint)
                    for (const auto& d : deltas) {
                        if (d.owner != perspective && d.mint == walletDelta.mint) {
                            if (walletDecreased && d.delta > 0)
                                cls.to = d.owner;
                            if (walletIncreased && d.delta < 0)
                                cls.from = d.owner;
                        }
                    }
                    if (cls.to.isEmpty() && walletIncreased)
                        cls.to = perspective;

                    cls.label = walletDecreased
                                    ? QCoreApplication::translate("TxClassifier", "Token Send")
                                    : QCoreApplication::translate("TxClassifier", "Token Receive");
                    return cls;
                }
            }
        }

        // ── Rule 4: SOL Transfer ──
        // Collect all system transfers. Transactions may contain multiple
        // transfer instructions (batch sends) or extra program instructions
        // from multisig wallets (Squads, etc.).
        {
            QList<const Instruction*> solTransfers;
            for (const auto* ix : meaningful) {
                if (ix->program == "system" && ix->type == "transfer") {
                    solTransfers.append(ix);
                }
            }
            if (!solTransfers.isEmpty()) {
                cls.type = TxType::SolTransfer;
                cls.from = solTransfers.first()->info.value("source").toString();
                cls.to = solTransfers.first()->info.value("destination").toString();

                double firstLamports = solTransfers.first()->info.value("lamports").toDouble();
                cls.toAmount = firstLamports / 1e9;

                double totalLamports = 0;
                for (const auto* ix : solTransfers) {
                    totalLamports += ix->info.value("lamports").toDouble();
                }
                cls.amount = totalLamports / 1e9;

                // Record extra recipients beyond the first
                for (int i = 1; i < solTransfers.size(); ++i) {
                    double lam = solTransfers[i]->info.value("lamports").toDouble();
                    cls.extraRecipients.append(
                        {solTransfers[i]->info.value("destination").toString(), lam / 1e9});
                }

                QString perspective = walletAddress.isEmpty() ? signer : walletAddress;
                cls.label = (cls.from == perspective)
                                ? QCoreApplication::translate("TxClassifier", "SOL Send")
                                : QCoreApplication::translate("TxClassifier", "SOL Receive");
                return cls;
            }
        }

        // ── Rule 5: Token operations (mint/burn/close) ──
        // Checked before account creation, since createIdempotent is often
        // just a prerequisite for the real operation (e.g. mint).
        for (const auto* ix : meaningful) {
            if (ix->program != "spl-token" && ix->program != "spl-token-2022")
                continue;
            if (ix->type == "mintTo" || ix->type == "mintToChecked") {
                cls.type = TxType::TokenMint;
                cls.label = QCoreApplication::translate("TxClassifier", "Token Mint");
                return cls;
            }
            if (ix->type == "burn" || ix->type == "burnChecked") {
                cls.type = TxType::TokenBurn;
                cls.label = QCoreApplication::translate("TxClassifier", "Token Burn");
                return cls;
            }
            if (ix->type == "closeAccount") {
                cls.type = TxType::CloseTokenAccount;
                cls.label = QCoreApplication::translate("TxClassifier", "Close Account");
                return cls;
            }
        }

        // ── Rule 6: Create Account ──
        for (const auto* ix : meaningful) {
            if (ix->programId == AssociatedTokenAccount &&
                (ix->type == "create" || ix->type == "createIdempotent")) {
                cls.type = TxType::CreateTokenAccount;
                cls.label = QCoreApplication::translate("TxClassifier", "Create Token Account");
                return cls;
            }
            if (ix->program == "system" && ix->type == "createAccount") {
                cls.type = TxType::CreateAccount;
                cls.label = QCoreApplication::translate("TxClassifier", "Create Account");
                return cls;
            }
        }

        // ── Rule 7: Unknown ──
        cls.type = TxType::Unknown;
        cls.label = QCoreApplication::translate("TxClassifier", "Unknown");
        return cls;
    }

} // namespace TxClassifier

#endif // TXCLASSIFIER_H
