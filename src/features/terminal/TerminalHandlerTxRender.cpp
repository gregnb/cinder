#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "services/IdlRegistry.h"
#include "services/model/TransactionResponse.h"
#include "tx/InstructionDecoder.h"
#include "tx/KnownTokens.h"
#include "tx/TxClassifier.h"
#include "tx/TxParseUtils.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

using namespace terminal;

void TerminalHandler::renderTxByMode(const QJsonObject& rawJson, TxDisplayMode mode) {
    TransactionResponse tx = TransactionResponse::fromJson(rawJson);
    InstructionDecoder::decodeAll(tx, m_idlRegistry);

    switch (mode) {
        case TxDisplayMode::Classify:
            renderTxClassify(tx);
            return;
        case TxDisplayMode::Deltas:
            renderTxDeltas(tx);
            return;
        case TxDisplayMode::Cu:
            renderTxComputeUnits(tx);
            return;
        case TxDisplayMode::Inspect:
            renderTxInspect(tx);
            return;
    }
}

void TerminalHandler::renderTxClassify(const TransactionResponse& tx) {
    auto cls = TxClassifier::classify(tx);
    emitOutput("  Type:     " + cls.label);
    if (!cls.dexProgramId.isEmpty()) {
        emitOutput("  DEX:      " + TxClassifier::dexName(cls.dexProgramId), kDimColor);
    }
    if (cls.type == TxClassifier::TxType::Swap || cls.type == TxClassifier::TxType(3)) {
        if (cls.amountIn > 0) {
            emitOutput("  In:       " + QString::number(cls.amountIn, 'f', 6) + " " + cls.symbolIn);
        }
        if (cls.amountOut > 0) {
            emitOutput("  Out:      " + QString::number(cls.amountOut, 'f', 6) + " " +
                       cls.symbolOut);
        }
    } else if (cls.amount > 0) {
        emitOutput("  Amount:   " + QString::number(cls.amount, 'f', 6) + " " + cls.tokenSymbol);
        if (!cls.from.isEmpty()) {
            emitOutput("  From:     " + truncAddr(cls.from), kDimColor);
        }
        if (!cls.to.isEmpty()) {
            emitOutput("  To:       " + truncAddr(cls.to), kDimColor);
        }
    }
}

void TerminalHandler::renderTxDeltas(const TransactionResponse& tx) {
    auto deltas = TxClassifier::computeTokenDeltas(tx);
    if (deltas.isEmpty()) {
        emitOutput("  No token balance changes.", kDimColor);
        return;
    }
    emitOutput(padRight("  Owner", 18) + padRight("Token", 14) + "Delta", kDimColor);
    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
    for (const auto& d : deltas) {
        KnownToken kt = resolveKnownToken(d.mint);
        QString sym = kt.symbol.isEmpty() ? truncAddr(d.mint) : kt.symbol;
        QString sign = d.delta >= 0 ? "+" : "";
        QColor c = d.delta >= 0 ? kPromptColor : kErrorColor;
        emitOutput("  " + padRight(truncAddr(d.owner), 16) + padRight(sym, 14) + sign +
                       QString::number(d.delta, 'f', 6),
                   c);
    }
}

void TerminalHandler::renderTxComputeUnits(const TransactionResponse& tx) {
    auto breakdown =
        TxParseUtils::parseCuBreakdown(tx.meta.logMessages, tx.meta.computeUnitsConsumed);
    emitOutput(padRight("  #", 5) + padRight("Program", 28) + "CU Used", kDimColor);
    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
    for (int i = 0; i < breakdown.size(); ++i) {
        QString name = TxParseUtils::friendlyProgramName(breakdown[i].programId);
        if (name.isEmpty()) {
            name = truncAddr(breakdown[i].programId);
        }
        emitOutput("  " + padRight(QString::number(i + 1), 4) + padRight(name, 28) +
                   QString::number(breakdown[i].units));
    }
    emitOutput("  Total: " + QString::number(tx.meta.computeUnitsConsumed), kDimColor);
}

void TerminalHandler::renderTxInspect(const TransactionResponse& tx) {
    QMap<QString, AccountKey> akMap;
    for (const auto& ak : tx.message.accountKeys) {
        akMap[ak.pubkey] = ak;
    }

    QString feePayer;
    for (const auto& ak : tx.message.accountKeys) {
        if (ak.signer) {
            feePayer = ak.pubkey;
            break;
        }
    }

    QMap<int, QList<Instruction>> innerMap;
    for (const auto& innerSet : tx.meta.innerInstructions) {
        innerMap[innerSet.index] = innerSet.instructions;
    }

    emitOutput("  OVERVIEW", kAccentColor);
    emitOutput("  " + QString(50, QChar(0x2500)), kDimColor);
    emitOutput("  Slot:         " + QString::number(tx.slot));
    emitOutput("  Time:         " +
               QDateTime::fromSecsSinceEpoch(tx.blockTime).toString("yyyy-MM-dd hh:mm:ss") +
               " UTC");
    QString status = tx.meta.hasError ? "✗ FAILED" : "✓ SUCCESS";
    emitOutput("  Status:       " + status, tx.meta.hasError ? kErrorColor : kPromptColor);
    emitOutput("  Fee:          " + formatSol(tx.meta.fee));

    int numSigners = 0;
    for (const auto& ak : tx.message.accountKeys) {
        if (ak.signer) {
            ++numSigners;
        }
    }
    qint64 priorityLamports = tx.meta.fee - (numSigners * 5000LL);
    if (priorityLamports > 0) {
        emitOutput("  Priority Fee: " + formatSol(priorityLamports) + "  (" +
                       QString::number(priorityLamports) + " lamports)",
                   kDimColor);
    }

    emitOutput("  Compute:      " + QString::number(tx.meta.computeUnitsConsumed) + " CU");
    emitOutput("  Version:      " + tx.version, kDimColor);
    emitOutput("  Blockhash:    " + truncAddr(tx.message.recentBlockhash), kDimColor);
    emitOutput("  Fee Payer:    " + feePayer, kDimColor);

    emitOutput("");
    emitOutput("  SUMMARY", kAccentColor);
    emitOutput("  " + QString(50, QChar(0x2500)), kDimColor);
    auto cls = TxClassifier::classify(tx);
    emitOutput("  Type:         " + cls.label);
    if (!cls.dexProgramId.isEmpty()) {
        emitOutput("  DEX:          " + TxClassifier::dexName(cls.dexProgramId), kDimColor);
    }
    if (cls.type == TxClassifier::TxType::Swap || cls.type == TxClassifier::TxType(3)) {
        if (cls.amountIn > 0) {
            KnownToken ktIn = resolveKnownToken(cls.mintIn);
            QString symIn = ktIn.symbol.isEmpty() ? cls.symbolIn : ktIn.symbol;
            emitOutput("  Sold:         " + QString::number(cls.amountIn, 'f', 6) + " " + symIn,
                       kErrorColor);
        }
        if (cls.amountOut > 0) {
            KnownToken ktOut = resolveKnownToken(cls.mintOut);
            QString symOut = ktOut.symbol.isEmpty() ? cls.symbolOut : ktOut.symbol;
            emitOutput("  Received:     " + QString::number(cls.amountOut, 'f', 6) + " " + symOut,
                       kPromptColor);
        }
    } else if (cls.amount > 0) {
        emitOutput("  Amount:       " + QString::number(cls.amount, 'f', 6) + " " +
                   cls.tokenSymbol);
        if (!cls.from.isEmpty()) {
            emitOutput("  From:         " + cls.from, kDimColor);
        }
        if (!cls.to.isEmpty()) {
            emitOutput("  To:           " + cls.to, kDimColor);
        }
    }

    emitOutput("");
    emitOutput("  INSTRUCTIONS (" + QString::number(tx.message.instructions.size()) + ")",
               kAccentColor);
    emitOutput("  " + QString(50, QChar(0x2500)), kDimColor);

    auto renderIxInfo = [this, &akMap, &feePayer](const Instruction& ix, const QString& indent) {
        if (ix.isParsed()) {
            QString mint = ix.info.value("mint").toString();
            for (const QString& key : ix.info.keys()) {
                QJsonValue jv = ix.info[key];
                QString label = fmtKeyName(key);
                if (jv.isString()) {
                    QString sv = jv.toString();
                    if (looksLikeAddr(sv)) {
                        QString badges;
                        if (akMap.contains(sv)) {
                            if (akMap[sv].writable) {
                                badges += " [W]";
                            }
                            if (akMap[sv].signer) {
                                badges += " [S]";
                            }
                            if (sv == feePayer) {
                                badges += " [FEE]";
                            }
                        }
                        KnownToken kt = resolveKnownToken(sv);
                        QString resolved;
                        if (!kt.symbol.isEmpty()) {
                            resolved = " (" + kt.symbol + ")";
                        } else {
                            QString pn = TxParseUtils::friendlyProgramName(sv);
                            if (!pn.isEmpty() && !pn.contains("...")) {
                                resolved = " (" + pn + ")";
                            }
                        }
                        emitOutput(indent + padRight(label + ":", 20) + sv + resolved + badges,
                                   kDimColor);
                    } else {
                        emitOutput(indent + padRight(label + ":", 20) + sv, kDimColor);
                    }
                } else if (jv.isDouble()) {
                    double dv = jv.toDouble();
                    if (key.toLower().contains("lamport") && key != "microLamports") {
                        qint64 lam = static_cast<qint64>(dv);
                        emitOutput(indent + padRight(label + ":", 20) +
                                       QString::number(lam / 1e9, 'f', 9) + " SOL",
                                   kDimColor);
                    } else {
                        qint64 iv = static_cast<qint64>(dv);
                        emitOutput(indent + padRight(label + ":", 20) +
                                       (static_cast<double>(iv) == dv
                                            ? QString::number(iv)
                                            : QString::number(dv, 'g', 10)),
                                   kDimColor);
                    }
                } else if (jv.isObject()) {
                    QJsonObject obj = jv.toObject();
                    if (obj.contains("uiAmountString")) {
                        QString amt = obj["uiAmountString"].toString();
                        KnownToken kt = resolveKnownToken(mint);
                        QString sym = kt.symbol.isEmpty() ? "" : " " + kt.symbol;
                        emitOutput(indent + padRight(label + ":", 20) + amt + sym, kDimColor);
                    } else {
                        emitOutput(indent + padRight(label + ":", 20) +
                                       QJsonDocument(obj).toJson(QJsonDocument::Compact),
                                   kDimColor);
                    }
                } else if (jv.isBool()) {
                    emitOutput(indent + padRight(label + ":", 20) +
                                   (jv.toBool() ? "true" : "false"),
                               kDimColor);
                }
            }
        } else {
            if (!ix.data.isEmpty()) {
                QString data = ix.data;
                if (data.length() > 60) {
                    data = data.left(57) + "...";
                }
                emitOutput(indent + padRight("Data:", 20) + data, kDimColor);
            }
            for (int a = 0; a < ix.accounts.size(); ++a) {
                QString acct = ix.accounts[a].toString();
                QString badges;
                if (akMap.contains(acct)) {
                    if (akMap[acct].writable) {
                        badges += " [W]";
                    }
                    if (akMap[acct].signer) {
                        badges += " [S]";
                    }
                }
                emitOutput(indent + padRight("Account #" + QString::number(a + 1) + ":", 20) +
                               truncAddr(acct) + badges,
                           kDimColor);
            }
        }
    };

    for (int i = 0; i < tx.message.instructions.size(); ++i) {
        const Instruction& ix = tx.message.instructions[i];
        QString progName = TxParseUtils::friendlyProgramName(ix.programId);
        if (progName.isEmpty() || progName.contains("...")) {
            progName = truncAddr(ix.programId);
        }
        QString typeName = ix.isParsed() ? fmtTypeName(ix.type) : "(unparsed)";
        emitOutput("");
        emitOutput("  #" + QString::number(i + 1) + "  " + progName + ": " + typeName,
                   kPromptColor);

        emitOutput("    " + padRight("Program:", 20) + ix.programId, kDimColor);
        renderIxInfo(ix, "    ");

        if (innerMap.contains(i)) {
            const auto& inners = innerMap[i];
            emitOutput("");
            emitOutput("      INNER INSTRUCTIONS (" + QString::number(inners.size()) + ")",
                       kWarnColor);
            for (int j = 0; j < inners.size(); ++j) {
                const Instruction& inner = inners[j];
                QString innerProg = TxParseUtils::friendlyProgramName(inner.programId);
                if (innerProg.isEmpty() || innerProg.contains("...")) {
                    innerProg = truncAddr(inner.programId);
                }
                QString innerType = inner.isParsed() ? fmtTypeName(inner.type) : "(unparsed)";
                emitOutput("      #" + QString::number(i + 1) + "." + QString::number(j + 1) +
                           "  " + innerProg + ": " + innerType);
                emitOutput("        " + padRight("Program:", 20) + inner.programId, kDimColor);
                renderIxInfo(inner, "        ");
            }
        }
    }

    auto deltas = TxClassifier::computeTokenDeltas(tx);
    if (!deltas.isEmpty()) {
        emitOutput("");
        emitOutput("  BALANCE CHANGES", kAccentColor);
        emitOutput("  " + QString(50, QChar(0x2500)), kDimColor);
        for (const auto& d : deltas) {
            KnownToken kt = resolveKnownToken(d.mint);
            QString sym = kt.symbol.isEmpty() ? truncAddr(d.mint) : kt.symbol;
            QString sign = d.delta >= 0 ? "+" : "";
            QColor c = d.delta >= 0 ? kPromptColor : kErrorColor;
            emitOutput("  " + padRight(truncAddr(d.owner), 14) + padRight(sym, 12) + sign +
                           QString::number(d.delta, 'f', 6),
                       c);
        }
    }
}
