#include "features/sendreceive/BurnTokenHandler.h"

#include "tx/TokenEncodingUtils.h"
#include "tx/TokenOperationBuilder.h"

#include <QCoreApplication>

BurnTokenHandler::BurnTokenHandler(SendReceiveHandler* sharedHandler)
    : m_sharedHandler(sharedHandler) {}

bool BurnTokenHandler::isValidBurnAmount(const QString& text) const {
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    return ok && value > 0.0;
}

quint64 BurnTokenHandler::parseBurnAmountRaw(const QString& text, int decimals, bool* ok) const {
    if (ok) {
        *ok = false;
    }
    bool amountOk = false;
    const double amount = text.trimmed().toDouble(&amountOk);
    if (!amountOk || amount <= 0.0 || decimals < 0) {
        return 0;
    }

    quint64 raw = 0;
    if (TokenAmountCodec::toRaw(amount, decimals, &raw)) {
        if (ok) {
            *ok = true;
        }
    }
    return raw;
}

BurnTokenHandler::BurnBuildResult
BurnTokenHandler::buildBurnInstructions(const QString& walletAddress,
                                        const SendReceiveTokenMeta& meta, quint64 rawAmount) const {
    BurnBuildResult result;

    if (walletAddress.isEmpty() || meta.mint.isEmpty() || meta.tokenProgram.isEmpty() ||
        meta.accountAddress.isEmpty() || rawAmount == 0) {
        result.error = "invalid_input";
        return result;
    }

    BurnInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mint = meta.mint;
    input.sourceTokenAccount = meta.accountAddress;
    input.tokenProgram = meta.tokenProgram;
    input.rawAmount = rawAmount;
    input.decimals = static_cast<quint8>(meta.decimals);
    const BurnInstructionBuildResult buildResult = TokenOperationBuilder::buildBurn(input);
    result.ok = buildResult.ok;
    result.error = buildResult.error;
    result.instructions = buildResult.instructions;
    return result;
}

void BurnTokenHandler::executeBurnFlow(
    const QString& walletAddress, const SendReceiveTokenMeta& meta, quint64 rawAmount,
    SolanaApi* solanaApi, Signer* signer, QObject* context,
    const SendReceiveHandler::ExecuteSendCallbacks& callbacks) const {
    if (!m_sharedHandler) {
        return;
    }

    const BurnBuildResult build = buildBurnInstructions(walletAddress, meta, rawAmount);
    if (!build.ok) {
        if (callbacks.onStatus) {
            callbacks.onStatus(QCoreApplication::translate(
                                   "SendReceivePage", "Error: could not prepare burn transaction."),
                               true);
        }
        if (callbacks.onFinished) {
            callbacks.onFinished();
        }
        return;
    }

    SendReceiveExecutionRequest request;
    request.walletAddress = walletAddress;
    request.instructions = build.instructions;
    request.nonceEnabled = false;
    m_sharedHandler->executeSendFlow(request, solanaApi, signer, context, callbacks);
}
