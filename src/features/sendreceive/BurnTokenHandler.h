#ifndef BURNTOKENHANDLER_H
#define BURNTOKENHANDLER_H

#include "features/sendreceive/SendReceiveHandler.h"
#include "models/SendReceive.h"

class QObject;
class SolanaApi;
class Signer;

class BurnTokenHandler {
  public:
    struct BurnBuildResult {
        bool ok = false;
        QString error;
        QList<TransactionInstruction> instructions;
    };

    explicit BurnTokenHandler(SendReceiveHandler* sharedHandler);

    bool isValidBurnAmount(const QString& text) const;
    quint64 parseBurnAmountRaw(const QString& text, int decimals, bool* ok) const;
    BurnBuildResult buildBurnInstructions(const QString& walletAddress,
                                          const SendReceiveTokenMeta& meta,
                                          quint64 rawAmount) const;
    void executeBurnFlow(const QString& walletAddress, const SendReceiveTokenMeta& meta,
                         quint64 rawAmount, SolanaApi* solanaApi, Signer* signer, QObject* context,
                         const SendReceiveHandler::ExecuteSendCallbacks& callbacks) const;

  private:
    SendReceiveHandler* m_sharedHandler = nullptr;
};

#endif // BURNTOKENHANDLER_H
