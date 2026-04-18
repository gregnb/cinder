#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "services/SolanaApi.h"
#include "services/model/SignatureInfo.h"
#include "services/model/SimulationResponse.h"
#include "services/model/TransactionResponse.h"
#include "tx/InstructionDecoder.h"
#include "tx/ProgramIds.h"
#include <QDateTime>
#include <QJsonDocument>

using namespace terminal;

void TerminalHandler::fetchTx(const QString& sig,
                              const std::function<void(const QString&, const QJsonObject&)>& cb) {
    emitOutput("Fetching transaction...", kDimColor);
    auto op = asyncOp();
    op.watch(connect(m_api, &SolanaApi::transactionReady, this,
                     [this, sig, cb](const QString& retSig, const TransactionResponse& tx) {
                         if (retSig != sig) {
                             return;
                         }
                         cancelPending();
                         cb(retSig, tx.rawJson);
                     }));
    op.run([this, sig]() { m_api->fetchTransaction(sig); });
}

void TerminalHandler::cmdTx(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: tx <classify|deltas|cu|signatures|decode|simulate>", kDimColor);
            break;

        case TerminalSubcommand::TxSignatures: {
            if (args.size() < 3) {
                emitOutput("Usage: tx signatures <address> [limit]", kDimColor);
                break;
            }
            const QString& addr = args[2];
            int limit = (args.size() > 3) ? args[3].toInt() : 10;
            if (limit <= 0) {
                limit = 10;
            }

            emitOutput("Fetching signatures...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(
                m_api, &SolanaApi::signaturesReady, this,
                [this, addr, limit](const QString& retAddr, const QList<SignatureInfo>& sigs) {
                    if (retAddr != addr) {
                        return;
                    }
                    cancelPending();
                    const int show = qMin(limit, sigs.size());
                    for (int i = 0; i < show; ++i) {
                        const auto& s = sigs[i];
                        const QString date =
                            QDateTime::fromSecsSinceEpoch(s.blockTime).toString("yyyy-MM-dd hh:mm");
                        const QString status = s.hasError ? "✗" : "✓";
                        const QColor c = s.hasError ? kErrorColor : QColor(255, 255, 255);
                        emitOutput("  " + padRight(QString::number(i + 1) + ".", 5) +
                                       padRight(truncAddr(s.signature), 14) + date + "  " + status,
                                   c);
                    }
                    if (sigs.isEmpty()) {
                        emitOutput("  No signatures found.", kDimColor);
                    }
                }));
            op.run([this, addr, limit]() { m_api->fetchSignatures(addr, limit); });
            break;
        }

        case TerminalSubcommand::TxSimulate: {
            if (args.size() < 3) {
                emitOutput("Usage: tx simulate <base64_encoded_tx>", kDimColor);
                break;
            }
            const QByteArray txData = QByteArray::fromBase64(args[2].toLatin1());
            if (txData.isEmpty()) {
                emitOutput("Invalid base64 data.", kErrorColor);
                break;
            }

            emitOutput("Simulating transaction...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(
                m_api, &SolanaApi::simulationReady, this, [this](const SimulationResponse& sim) {
                    cancelPending();
                    if (sim.success) {
                        emitOutput("  ✓ Simulation succeeded", kPromptColor);
                    } else {
                        emitOutput("  ✗ Simulation failed", kErrorColor);
                        if (!sim.err.isNull()) {
                            emitOutput("  Error: " + QJsonDocument(sim.err.toObject())
                                                         .toJson(QJsonDocument::Compact),
                                       kErrorColor);
                        }
                    }
                    emitOutput("  Compute units: " + QString::number(sim.unitsConsumed), kDimColor);
                    if (!sim.logs.isEmpty()) {
                        emitOutput("");
                        emitOutput("  LOGS", kAccentColor);
                        const int shown = qMin(sim.logs.size(), 30);
                        for (int i = 0; i < shown; ++i) {
                            const QColor c =
                                sim.logs[i].contains("failed") ? kErrorColor : kDimColor;
                            emitOutput("    " + sim.logs[i], c);
                        }
                        if (sim.logs.size() > 30) {
                            emitOutput("    ... (" + QString::number(sim.logs.size() - 30) +
                                           " more lines)",
                                       kDimColor);
                        }
                    }
                }));
            op.run([this, txData]() { m_api->simulateTransaction(txData); });
            break;
        }

        case TerminalSubcommand::TxDecode: {
            if (args.size() < 3) {
                emitOutput("Usage: tx decode <base58_data>", kDimColor);
                break;
            }
            Instruction ix;
            ix.data = args[2];
            ix.programId = SolanaPrograms::ComputeBudget;
            if (InstructionDecoder::decodeComputeBudget(ix)) {
                emitOutput("  Program:  ComputeBudget");
                emitOutput("  Type:     " + ix.type);
                break;
            }
            ix.programId = SolanaPrograms::SystemProgram;
            if (InstructionDecoder::decodeSystemProgram(ix)) {
                emitOutput("  Program:  System Program");
                emitOutput("  Type:     " + ix.type);
                break;
            }
            emitOutput("  Could not decode instruction data.", kDimColor);
            break;
        }

        case TerminalSubcommand::TxClassify:
        case TerminalSubcommand::TxDeltas:
        case TerminalSubcommand::TxCu: {
            if (!ctx.requireArgs(3, "Usage: tx " + cmd.subcommandToken + " <signature>")) {
                break;
            }
            TxDisplayMode mode = (cmd.sub == TerminalSubcommand::TxClassify)
                                     ? TxDisplayMode::Classify
                                 : (cmd.sub == TerminalSubcommand::TxDeltas) ? TxDisplayMode::Deltas
                                                                             : TxDisplayMode::Cu;
            fetchTx(args[2], [this, mode](const QString&, const QJsonObject& rawJson) {
                renderTxByMode(rawJson, mode);
            });
            break;
        }

        default:
            break;
    }
}
