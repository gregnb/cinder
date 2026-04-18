#ifndef TOKENOPERATIONBUILDER_H
#define TOKENOPERATIONBUILDER_H

#include "tx/TransactionInstruction.h"

#include <QList>
#include <QString>
#include <QtGlobal>

struct TransferInstructionRecipient {
    QString address;
    double amount = 0.0;
};

struct TransferInstructionBuildInput {
    QString walletAddress;
    QString mint;
    QString sourceTokenAccount;
    int decimals = 0;
    QString tokenProgram;
    QList<TransferInstructionRecipient> recipients;
    quint16 transferFeeBasisPoints = 0;
    quint64 transferFeeMax = 0;
    int maxRecipients = 20;
};

struct TransferInstructionBuildResult {
    bool ok = false;
    QString error;
    QList<TransactionInstruction> instructions;
};

struct CreateTokenInstructionBuildInput {
    QString walletAddress;
    QString mintAddress;
    QString name;
    QString symbol;
    QString uri;
    QString freezeAuthority;
    int decimals = 0;
    quint64 rawSupply = 0;
    bool hasTransferFee = false;
    quint16 feeBasisPoints = 0;
    quint64 feeMaxRaw = 0;
    bool hasNonTransferable = false;
    bool hasMintClose = false;
    bool hasPermanentDelegate = false;
    quint64 mintAccountSize = 0;
    quint64 rentLamports = 0;
};

struct CreateTokenInstructionBuildResult {
    bool ok = false;
    QString error;
    QList<TransactionInstruction> instructions;
};

struct MintInstructionBuildInput {
    QString walletAddress;
    QString mint;
    QString tokenProgram;
    quint64 rawAmount = 0;
};

struct MintInstructionBuildResult {
    bool ok = false;
    QString error;
    QList<TransactionInstruction> instructions;
};

struct BurnInstructionBuildInput {
    QString walletAddress;
    QString mint;
    QString sourceTokenAccount;
    QString tokenProgram;
    quint64 rawAmount = 0;
    quint8 decimals = 0;
};

struct BurnInstructionBuildResult {
    bool ok = false;
    QString error;
    QList<TransactionInstruction> instructions;
};

class TokenOperationBuilder {
  public:
    static TransferInstructionBuildResult buildTransfer(const TransferInstructionBuildInput& input);
    static CreateTokenInstructionBuildResult
    buildCreateToken(const CreateTokenInstructionBuildInput& input);
    static MintInstructionBuildResult buildMint(const MintInstructionBuildInput& input);
    static BurnInstructionBuildResult buildBurn(const BurnInstructionBuildInput& input);
};

#endif // TOKENOPERATIONBUILDER_H
