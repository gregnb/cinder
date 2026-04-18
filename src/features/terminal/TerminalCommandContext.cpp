#include "TerminalCommandContext.h"

#include "TerminalHandlerCommon.h"

using namespace terminal;

TerminalCommandContext::TerminalCommandContext(
    const QStringList& args, const std::function<void(const QString&, const QColor&)>& emitOutput)
    : m_args(args), m_emitOutput(emitOutput) {}

bool TerminalCommandContext::requireArgs(int minCount, const QString& usage) const {
    if (m_args.size() >= minCount) {
        return true;
    }
    m_emitOutput(usage, kDimColor);
    return false;
}

bool TerminalCommandContext::requireWallet(const QString& walletAddress,
                                           const QString& message) const {
    if (!walletAddress.isEmpty()) {
        return true;
    }
    m_emitOutput(message, kErrorColor);
    return false;
}

bool TerminalCommandContext::requireSigner(const void* signer, const QString& message) const {
    if (signer != nullptr) {
        return true;
    }
    m_emitOutput(message, kErrorColor);
    return false;
}

bool TerminalCommandContext::requireNoPending(bool hasPending, const QString& message) const {
    if (!hasPending) {
        return true;
    }
    m_emitOutput(message, kErrorColor);
    return false;
}
