#ifndef CLOSETOKENACCOUNTSHANDLER_H
#define CLOSETOKENACCOUNTSHANDLER_H

#include "features/sendreceive/SendReceiveHandler.h"
#include "tx/TransactionInstruction.h"

#include <QList>
#include <QString>

class QObject;
class SolanaApi;
class Signer;

struct CloseTokenAccountEntry {
    QString symbol;
    QString accountAddress;
    QString balance;
    QString tokenProgram;
};

class CloseTokenAccountsHandler {
  public:
    static constexpr int kMaxClosePerTx = 20;

    explicit CloseTokenAccountsHandler(SendReceiveHandler* sharedHandler);

    QList<CloseTokenAccountEntry> loadAccountEntries(const QString& ownerAddress) const;
    QString summaryText(int selectedCount) const;
    bool hasSelection(int selectedCount) const;

    QList<TransactionInstruction>
    buildCloseInstructions(const QString& walletAddress,
                           const QList<CloseTokenAccountEntry>& entries) const;

    void executeCloseFlow(const QString& walletAddress,
                          const QList<CloseTokenAccountEntry>& entries, SolanaApi* solanaApi,
                          Signer* signer, QObject* context,
                          const SendReceiveHandler::ExecuteSendCallbacks& callbacks) const;

  private:
    SendReceiveHandler* m_sharedHandler = nullptr;
};

#endif // CLOSETOKENACCOUNTSHANDLER_H
