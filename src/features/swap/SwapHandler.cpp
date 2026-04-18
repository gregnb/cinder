#include "features/swap/SwapHandler.h"

#include "Theme.h"
#include "crypto/Signer.h"
#include "db/TokenAccountDb.h"
#include "services/JupiterApi.h"
#include "services/SolanaApi.h"
#include "tx/KnownTokens.h"

#include <QCoreApplication>
#include <QtMath>

namespace {
    struct SwapTokenDef {
        QString mint;
        QString symbol;
        QString displayName;
        QString iconPath;
        int decimals;
    };

    static const QList<SwapTokenDef> kSwapTokens = {
        {"So11111111111111111111111111111111111111112", "SOL", "SOL  — Solana",
         ":/icons/tokens/sol.png", 9},
        {"EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", "USDC", "USDC  — USD Coin",
         ":/icons/tokens/usdc.png", 6},
        {"Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB", "USDT", "USDT  — Tether",
         ":/icons/tokens/usdt.png", 6},
        {"DezXAZ8z7PnrnRJjz3wXBoRgixCa6xjnB7YaB1pPB263", "BONK", "BONK", ":/icons/tokens/bonk.png",
         5},
        {"jtojtomepa8beP8AuQc6eXt5FriJwfFMwQx2v2f9mCL", "JTO", "JTO  — Jito",
         ":/icons/tokens/jto.png", 9},
    };

    constexpr int kDefaultSlippageBps = 50;
} // namespace

SwapHandler::SwapHandler(QObject* parent) : QObject(parent), m_jupiterApi(new JupiterApi(this)) {
    connect(m_jupiterApi, &JupiterApi::quoteReady, this, &SwapHandler::onQuoteReady);
    connect(
        m_jupiterApi, &JupiterApi::swapTransactionReady, this,
        [this](const QByteArray& serializedTx, quint64) { onSwapTransactionReady(serializedTx); });
    connect(m_jupiterApi, &JupiterApi::requestFailed, this, &SwapHandler::onJupiterRequestFailed);
}

void SwapHandler::setSolanaApi(SolanaApi* api) {
    if (m_solanaApi == api) {
        return;
    }

    if (m_solanaApi) {
        disconnect(m_solanaApi, nullptr, this, nullptr);
    }

    m_solanaApi = api;
    if (!m_solanaApi) {
        return;
    }

    connect(m_solanaApi, &SolanaApi::transactionSent, this, &SwapHandler::onTransactionSent);
    connect(m_solanaApi, &SolanaApi::requestFailed, this, &SwapHandler::onSolanaRequestFailed);
}

void SwapHandler::setSigner(Signer* signer) { m_signer = signer; }

void SwapHandler::setWalletAddress(const QString& address) { m_walletAddress = address; }

void SwapHandler::refreshBalances() {
    QList<SwapTokenOption> toOptions;
    QList<SwapTokenOption> fromOptions;
    m_tokenDisplayToMint.clear();

    for (const auto& token : kSwapTokens) {
        SwapTokenOption opt{token.iconPath, token.displayName, QString(), token.mint};
        toOptions.append(opt);
        m_tokenDisplayToMint[opt.displayName] = opt.mint;
    }

    QString solBalanceText = QStringLiteral("0.00 SOL");

    if (!m_walletAddress.isEmpty()) {
        const auto accounts = TokenAccountDb::getAccountsByOwnerRecords(m_walletAddress);

        for (const auto& acct : accounts) {
            if (acct.tokenAddress == WSOL_MINT) {
                const double bal = acct.balance.toDouble();
                solBalanceText =
                    QString::number(bal, 'f', bal >= 1 ? 2 : 6) + QStringLiteral(" SOL");
                break;
            }
        }

        for (const auto& acct : accounts) {
            const QString mint = acct.tokenAddress;
            if (mint == WSOL_MINT) {
                continue;
            }

            const double bal = acct.balance.toDouble();
            if (bal <= 0.0) {
                continue;
            }

            QString symbol = acct.symbol;
            QString name = acct.name;
            const KnownToken known = resolveKnownToken(mint);
            if (!known.symbol.isEmpty()) {
                symbol = known.symbol;
            }

            const QString iconPath = known.iconPath.isEmpty()
                                         ? QStringLiteral(":/icons/tokens/sol.png")
                                         : known.iconPath;
            const QString displayName =
                symbol + QStringLiteral("  —  ") +
                (name.isEmpty() ? mint.left(8) + QStringLiteral("...") : name);
            const QString balanceText =
                QString::number(bal, 'f', bal >= 1 ? 2 : 6) + QStringLiteral(" ") + symbol;

            fromOptions.append({iconPath, displayName, balanceText, mint});
            m_tokenDisplayToMint[displayName] = mint;
        }
    }

    const SwapTokenOption solFrom{QStringLiteral(":/icons/tokens/sol.png"),
                                  QStringLiteral("SOL  —  Solana"), solBalanceText, WSOL_MINT};
    fromOptions.prepend(solFrom);
    m_tokenDisplayToMint[solFrom.displayName] = solFrom.mint;

    const QString toDefault = (kSwapTokens.size() >= 2) ? kSwapTokens[1].displayName : QString();
    emit tokenOptionsReady(fromOptions, solFrom.displayName, toOptions, toDefault);
}

void SwapHandler::invalidateQuote() {
    setQuoteValid(false);
    emit quoteCleared();
}

void SwapHandler::requestQuote(const QString& fromDisplay, const QString& toDisplay,
                               const QString& amountText) {
    const QString fromMint = mintForDisplay(fromDisplay);
    const QString toMint = mintForDisplay(toDisplay);
    const QString amountStr = amountText.trimmed();

    if (fromMint.isEmpty() || toMint.isEmpty() || amountStr.isEmpty()) {
        return;
    }

    if (fromMint == toMint) {
        emit statusChanged(QCoreApplication::translate("SwapPage", "Select different tokens"),
                           true);
        return;
    }

    bool ok = false;
    const double amount = amountStr.toDouble(&ok);
    if (!ok || amount <= 0) {
        return;
    }

    const int decimals = decimalsForMint(fromMint);
    const quint64 rawAmount = static_cast<quint64>(amount * qPow(10.0, decimals));

    emit statusChanged(QCoreApplication::translate("SwapPage", "Fetching quote..."), false);
    m_jupiterApi->fetchQuote(fromMint, toMint, rawAmount, kDefaultSlippageBps);
}

void SwapHandler::executeSwap() {
    if (!m_quoteValid || m_walletAddress.isEmpty() || !m_solanaApi || !m_signer) {
        return;
    }

    setSwapInProgress(true);
    emit statusChanged(QCoreApplication::translate("SwapPage", "Building swap transaction..."),
                       false);
    m_jupiterApi->fetchSwapTransaction(m_currentQuote.rawResponse, m_walletAddress);
}

bool SwapHandler::quoteValid() const { return m_quoteValid; }

bool SwapHandler::swapInProgress() const { return m_swapInProgress; }

bool SwapHandler::hasWalletAddress() const { return !m_walletAddress.isEmpty(); }

void SwapHandler::setQuoteValid(bool valid) {
    if (m_quoteValid == valid) {
        return;
    }
    m_quoteValid = valid;
    emit quoteValidityChanged(m_quoteValid);
}

void SwapHandler::setSwapInProgress(bool inProgress) {
    if (m_swapInProgress == inProgress) {
        return;
    }
    m_swapInProgress = inProgress;
    emit swapInProgressChanged(m_swapInProgress);
}

void SwapHandler::onQuoteReady(const JupiterQuote& quote) {
    m_currentQuote = quote;
    setQuoteValid(true);

    const QString outMint = quote.outputMint;
    const int outDec = decimalsForMint(outMint);
    const double outVal = static_cast<double>(quote.outAmount) / qPow(10.0, outDec);

    const QString outSym = symbolForMint(outMint);
    const QString inSym = symbolForMint(quote.inputMint);

    SwapQuoteView view;
    view.estimatedOutput = QString("%1 %2").arg(outVal, 0, 'f', qMin(outDec, 6)).arg(outSym);

    const int inDec = decimalsForMint(quote.inputMint);
    const double inVal = static_cast<double>(quote.inAmount) / qPow(10.0, inDec);
    if (inVal > 0.0) {
        const double rate = outVal / inVal;
        view.rateText =
            QString("1 %1 ≈ %2 %3").arg(inSym).arg(rate, 0, 'f', rate >= 1 ? 2 : 6).arg(outSym);
    } else {
        view.rateText = QStringLiteral("--");
    }

    QStringList dexes;
    for (const auto& step : quote.routePlan) {
        if (!step.dexLabel.isEmpty()) {
            dexes.append(step.dexLabel);
        }
    }
    view.routeText = dexes.isEmpty() ? QStringLiteral("--") : dexes.join(QStringLiteral(" → "));

    if (quote.priceImpactPct < 0.01) {
        view.priceImpactText = QStringLiteral("< 0.01%");
        view.highPriceImpact = false;
    } else {
        view.priceImpactText = QString("%1%").arg(quote.priceImpactPct, 0, 'f', 2);
        view.highPriceImpact = quote.priceImpactPct > 1.0;
    }

    const double minVal = static_cast<double>(quote.otherAmountThreshold) / qPow(10.0, outDec);
    view.minReceivedText = QString("%1 %2").arg(minVal, 0, 'f', qMin(outDec, 6)).arg(outSym);
    view.slippageText =
        QString("%1%").arg(static_cast<double>(quote.slippageBps) / 100.0, 0, 'f', 1);

    emit quoteUpdated(view);
    emit statusChanged(QString(), false);
}

void SwapHandler::onSwapTransactionReady(const QByteArray& serializedTx) {
    if (serializedTx.size() < 66) {
        emit statusChanged(
            QCoreApplication::translate("SwapPage", "Invalid transaction from Jupiter"), true);
        setSwapInProgress(false);
        return;
    }

    const int numSigs = static_cast<unsigned char>(serializedTx[0]);
    const int messageOffset = 1 + (numSigs * 64);
    if (messageOffset >= serializedTx.size()) {
        emit statusChanged(QCoreApplication::translate("SwapPage", "Invalid transaction structure"),
                           true);
        setSwapInProgress(false);
        return;
    }

    const QByteArray message = serializedTx.mid(messageOffset);
    m_signer->signAsync(
        message, this, [this, serializedTx](const QByteArray& signature, const QString& error) {
            if (signature.isEmpty()) {
                const QString fallback = QCoreApplication::translate("SwapPage", "Signing failed");
                emit statusChanged(error.isEmpty() ? fallback : error, true);
                setSwapInProgress(false);
                return;
            }

            QByteArray signedTx = serializedTx;
            signedTx.replace(1, 64, signature);

            emit statusChanged(QCoreApplication::translate("SwapPage", "Submitting transaction..."),
                               false);
            m_solanaApi->sendTransaction(signedTx, false, "confirmed", 3);
        });
}

void SwapHandler::onJupiterRequestFailed(const QString& endpoint, const QString& error) {
    setQuoteValid(false);
    emit quoteCleared();

    switch (endpointFromString(endpoint)) {
        case JupiterEndpoint::Swap:
            setSwapInProgress(false);
            break;
        case JupiterEndpoint::Quote:
        case JupiterEndpoint::Unknown:
            break;
    }

    emit statusChanged(error, true);
}

void SwapHandler::onTransactionSent(const QString& signature) {
    if (!m_swapInProgress) {
        return;
    }

    setSwapInProgress(false);
    setQuoteValid(false);
    emit quoteCleared();
    emit statusChanged(QCoreApplication::translate("SwapPage", "Swap submitted! Signature: %1")
                           .arg(signature.left(16) + QStringLiteral("...")),
                       false);
}

void SwapHandler::onSolanaRequestFailed(const QString& method, const QString& error) {
    if (method != QStringLiteral("sendTransaction") || !m_swapInProgress) {
        return;
    }

    setSwapInProgress(false);
    emit statusChanged(QCoreApplication::translate("SwapPage", "Transaction failed: %1").arg(error),
                       true);
}

QString SwapHandler::mintForDisplay(const QString& display) const {
    return m_tokenDisplayToMint.value(display);
}

int SwapHandler::decimalsForMint(const QString& mint) const {
    for (const auto& token : kSwapTokens) {
        if (token.mint == mint) {
            return token.decimals;
        }
    }

    if (!m_walletAddress.isEmpty()) {
        const auto acct = TokenAccountDb::getAccountRecord(m_walletAddress, mint);
        if (acct.has_value()) {
            return acct->decimals;
        }
    }

    return 9;
}

QString SwapHandler::symbolForMint(const QString& mint) const {
    for (const auto& token : kSwapTokens) {
        if (token.mint == mint) {
            return token.symbol;
        }
    }

    const KnownToken known = resolveKnownToken(mint);
    if (!known.symbol.isEmpty()) {
        return known.symbol;
    }

    return QStringLiteral("?");
}

SwapHandler::JupiterEndpoint SwapHandler::endpointFromString(const QString& endpoint) {
    if (endpoint == QStringLiteral("quote")) {
        return JupiterEndpoint::Quote;
    }
    if (endpoint == QStringLiteral("swap")) {
        return JupiterEndpoint::Swap;
    }
    return JupiterEndpoint::Unknown;
}
