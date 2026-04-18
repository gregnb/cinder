#ifndef TERMINALCOMMANDCONTEXT_H
#define TERMINALCOMMANDCONTEXT_H

#include <QColor>
#include <QString>
#include <QStringList>
#include <functional>

class TerminalCommandContext {
  public:
    TerminalCommandContext(const QStringList& args,
                           const std::function<void(const QString&, const QColor&)>& emitOutput);

    bool requireArgs(int minCount, const QString& usage) const;
    bool requireWallet(const QString& walletAddress,
                       const QString& message = "No wallet loaded.") const;
    bool
    requireSigner(const void* signer,
                  const QString& message = "No keypair loaded. Unlock the wallet first.") const;
    bool requireNoPending(bool hasPending,
                          const QString& message = "Another request is pending.") const;

  private:
    const QStringList& m_args;
    std::function<void(const QString&, const QColor&)> m_emitOutput;
};

#endif // TERMINALCOMMANDCONTEXT_H
