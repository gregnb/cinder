#include "TerminalHandler.h"
#include "TerminalHandlerCommon.h"

#include "services/JupiterApi.h"
#include "services/PriceService.h"
#include "services/model/JupiterQuote.h"
#include "services/model/PriceData.h"
#include "tx/KnownTokens.h"

using namespace terminal;

void TerminalHandler::cmdSwap(const TerminalParsedCommand& cmd) {
    const auto& args = cmd.parts;
    const auto ctx = commandContext(args);

    switch (cmd.sub) {
        case TerminalSubcommand::None:
            emitOutput("Usage: swap quote <input_mint> <output_mint> <amount>", kDimColor);
            break;

        case TerminalSubcommand::SwapQuote: {
            if (!ctx.requireArgs(5, "Usage: swap quote <input_mint> <output_mint> <amount>")) {
                emitOutput("  amount is in input token's smallest unit (lamports for SOL)",
                           kDimColor);
                break;
            }

            const QString& inputMint = args[2];
            const QString& outputMint = args[3];
            bool ok = false;
            quint64 amount = args[4].toULongLong(&ok);
            if (!ok || amount == 0) {
                emitOutput("Invalid amount.", kErrorColor);
                break;
            }

            if (!m_jupiterApi) {
                m_jupiterApi = new JupiterApi(this);
            }

            emitOutput("Fetching swap quote...", kDimColor);
            auto op = asyncOp();
            op.watch(connect(
                m_jupiterApi, &JupiterApi::quoteReady, this, [this](const JupiterQuote& quote) {
                    cancelPending();
                    KnownToken ktIn = resolveKnownToken(quote.inputMint);
                    KnownToken ktOut = resolveKnownToken(quote.outputMint);
                    QString symIn =
                        ktIn.symbol.isEmpty() ? truncAddr(quote.inputMint) : ktIn.symbol;
                    QString symOut =
                        ktOut.symbol.isEmpty() ? truncAddr(quote.outputMint) : ktOut.symbol;

                    emitOutput("  SWAP QUOTE", kAccentColor);
                    emitOutput("  " + QString(44, QChar(0x2500)), kDimColor);
                    emitOutput("  Input:          " + QString::number(quote.inAmount) + " " +
                               symIn + " (raw)");
                    emitOutput("  Output:         " + QString::number(quote.outAmount) + " " +
                                   symOut + " (raw)",
                               kPromptColor);
                    emitOutput(
                        "  Price Impact:   " + QString::number(quote.priceImpactPct, 'f', 4) + "%",
                        kDimColor);
                    emitOutput("  Slippage:       " + QString::number(quote.slippageBps) + " bps",
                               kDimColor);

                    if (!quote.routePlan.isEmpty()) {
                        emitOutput("");
                        emitOutput("  ROUTE (" + QString::number(quote.routePlan.size()) + " hops)",
                                   kAccentColor);
                        for (int i = 0; i < quote.routePlan.size(); ++i) {
                            const auto& step = quote.routePlan[i];
                            KnownToken ki = resolveKnownToken(step.inputMint);
                            KnownToken ko = resolveKnownToken(step.outputMint);
                            QString si =
                                ki.symbol.isEmpty() ? truncAddr(step.inputMint) : ki.symbol;
                            QString so =
                                ko.symbol.isEmpty() ? truncAddr(step.outputMint) : ko.symbol;
                            emitOutput("    " + QString::number(i + 1) + ". " + step.dexLabel +
                                           ": " + si + " -> " + so + " (" +
                                           QString::number(step.percent) + "%)",
                                       kDimColor);
                        }
                    }
                }));
            op.watch(connect(m_jupiterApi, &JupiterApi::requestFailed, this,
                             [this](const QString& endpoint, const QString& error) {
                                 Q_UNUSED(endpoint);
                                 cancelPending();
                                 emitOutput("Jupiter API error: " + error, kErrorColor);
                             }));
            op.run([this, inputMint, outputMint, amount]() {
                m_jupiterApi->fetchQuote(inputMint, outputMint, amount);
            });
            break;
        }

        default:
            break;
    }
}

void TerminalHandler::cmdPrice(const QStringList& args) {
    const auto ctx = commandContext(args);
    if (!ctx.requireArgs(2, "Usage: price <mint>")) {
        return;
    }
    const QString& mint = args[1];

    QString cgId = PriceService::cachedCoingeckoId(mint);
    if (cgId.isEmpty()) {
        emitOutput("Resolving token ID for " + truncAddr(mint) + "...", kDimColor);
        auto opResolve = asyncOp();
        opResolve.watch(connect(
            m_priceService, &PriceService::coingeckoIdResolved, this,
            [this, mint](const QString& resolvedMint, const QString& coingeckoId) {
                if (resolvedMint != mint) {
                    return;
                }
                cancelPending();
                if (coingeckoId.isEmpty()) {
                    emitOutput("  Could not resolve token. Try CoinGecko ID directly.",
                               kErrorColor);
                    return;
                }
                emitOutput("  Fetching price...", kDimColor);
                auto opPrices = asyncOp();
                opPrices.watch(connect(
                    m_priceService, &PriceService::pricesReady, this,
                    [this, mint, coingeckoId](const QList<TokenPrice>& prices) {
                        cancelPending();
                        for (const auto& p : prices) {
                            if (p.coingeckoId == coingeckoId) {
                                KnownToken kt = resolveKnownToken(mint);
                                QString sym = kt.symbol.isEmpty() ? truncAddr(mint) : kt.symbol;
                                emitOutput("  " + sym + "  $" +
                                               QString::number(p.priceUsd, 'f', 2) + " USD",
                                           kPromptColor);
                                if (p.change24h != 0) {
                                    QString sign = p.change24h >= 0 ? "+" : "";
                                    QColor c = p.change24h >= 0 ? kPromptColor : kErrorColor;
                                    emitOutput("  24h Change:  " + sign +
                                                   QString::number(p.change24h, 'f', 2) + "%",
                                               c);
                                }
                                return;
                            }
                        }
                        emitOutput("  No price data returned.", kDimColor);
                    }));
                opPrices.run([this, coingeckoId]() { m_priceService->fetchPrices({coingeckoId}); });
            }));
        opResolve.run([this, mint]() { m_priceService->resolveCoingeckoId(mint); });
        return;
    }

    emitOutput("Fetching price...", kDimColor);
    auto op = asyncOp();
    op.watch(connect(m_priceService, &PriceService::pricesReady, this,
                     [this, mint, cgId](const QList<TokenPrice>& prices) {
                         cancelPending();
                         for (const auto& p : prices) {
                             if (p.coingeckoId == cgId) {
                                 KnownToken kt = resolveKnownToken(mint);
                                 QString sym = kt.symbol.isEmpty() ? truncAddr(mint) : kt.symbol;
                                 emitOutput("  " + sym + "  $" +
                                                QString::number(p.priceUsd, 'f', 2) + " USD",
                                            kPromptColor);
                                 if (p.change24h != 0) {
                                     QString sign = p.change24h >= 0 ? "+" : "";
                                     QColor c = p.change24h >= 0 ? kPromptColor : kErrorColor;
                                     emitOutput("  24h Change:  " + sign +
                                                    QString::number(p.change24h, 'f', 2) + "%",
                                                c);
                                 }
                                 return;
                             }
                         }
                         emitOutput("  No price data returned.", kDimColor);
                     }));
    op.run([this, cgId]() { m_priceService->fetchPrices({cgId}); });
}
