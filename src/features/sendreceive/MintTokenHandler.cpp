#include "features/sendreceive/MintTokenHandler.h"

#include "tx/TokenEncodingUtils.h"
#include "tx/TokenOperationBuilder.h"

#include <QCoreApplication>

MintTokenHandler::MintTokenHandler(SendReceiveHandler* sharedHandler)
    : m_sharedHandler(sharedHandler) {}

bool MintTokenHandler::isValidMintAmount(const QString& text) const {
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    return ok && value > 0.0;
}

quint64 MintTokenHandler::parseMintAmountRaw(const QString& text, int decimals, bool* ok) const {
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

MintTokenHandler::MintBuildResult
MintTokenHandler::buildMintInstructions(const QString& walletAddress,
                                        const SendReceiveTokenMeta& meta, quint64 rawAmount) const {
    MintBuildResult result;

    if (walletAddress.isEmpty() || meta.mint.isEmpty() || meta.tokenProgram.isEmpty() ||
        rawAmount == 0) {
        result.error = "invalid_input";
        return result;
    }

    MintInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mint = meta.mint;
    input.tokenProgram = meta.tokenProgram;
    input.rawAmount = rawAmount;
    const MintInstructionBuildResult buildResult = TokenOperationBuilder::buildMint(input);
    result.ok = buildResult.ok;
    result.error = buildResult.error;
    result.instructions = buildResult.instructions;
    return result;
}

void MintTokenHandler::executeMintFlow(
    const QString& walletAddress, const SendReceiveTokenMeta& meta, quint64 rawAmount,
    SolanaApi* solanaApi, Signer* signer, QObject* context,
    const SendReceiveHandler::ExecuteSendCallbacks& callbacks) const {
    if (!m_sharedHandler) {
        return;
    }

    const MintBuildResult build = buildMintInstructions(walletAddress, meta, rawAmount);
    if (!build.ok) {
        if (callbacks.onStatus) {
            callbacks.onStatus(QCoreApplication::translate(
                                   "SendReceivePage", "Error: could not prepare mint transaction."),
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
