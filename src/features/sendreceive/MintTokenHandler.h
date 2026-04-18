#ifndef MINTTOKENHANDLER_H
#define MINTTOKENHANDLER_H

#include "features/sendreceive/SendReceiveHandler.h"
#include "models/SendReceive.h"

class QObject;
class SolanaApi;
class Signer;

class MintTokenHandler {
  public:
    struct MintBuildResult {
        bool ok = false;
        QString error;
        QList<TransactionInstruction> instructions;
    };

    explicit MintTokenHandler(SendReceiveHandler* sharedHandler);

    bool isValidMintAmount(const QString& text) const;
    quint64 parseMintAmountRaw(const QString& text, int decimals, bool* ok) const;
    MintBuildResult buildMintInstructions(const QString& walletAddress,
                                          const SendReceiveTokenMeta& meta,
                                          quint64 rawAmount) const;
    void executeMintFlow(const QString& walletAddress, const SendReceiveTokenMeta& meta,
                         quint64 rawAmount, SolanaApi* solanaApi, Signer* signer, QObject* context,
                         const SendReceiveHandler::ExecuteSendCallbacks& callbacks) const;

  private:
    SendReceiveHandler* m_sharedHandler = nullptr;
};

#endif // MINTTOKENHANDLER_H
