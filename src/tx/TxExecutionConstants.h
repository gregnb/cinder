#ifndef TXEXECUTIONCONSTANTS_H
#define TXEXECUTIONCONSTANTS_H

#include <QtGlobal>

namespace TxExecutionConstants {
    inline constexpr int RpcLookupTimeoutMs = 15000;
    inline constexpr int RpcShortLookupTimeoutMs = 10000;
    inline constexpr int TxConfirmationTimeoutMs = 30000;

    inline constexpr quint64 NonceAccountSpaceBytes = 80;
    inline constexpr quint64 StakeAccountSpaceBytes = 200;

    inline constexpr char FinalizedCommitment[] = "finalized";
    inline constexpr char MethodSendTransaction[] = "sendTransaction";
    inline constexpr char MethodLatestBlockhash[] = "getLatestBlockhash";
    inline constexpr char MethodMinimumBalance[] = "getMinimumBalanceForRentExemption";
    inline constexpr char MethodAccountInfo[] = "getAccountInfo";
    inline constexpr char MethodGetTransaction[] = "getTransaction";
    inline constexpr char MethodGetSignatureStatuses[] = "getSignatureStatuses";

    inline constexpr char ConfirmationConfirmed[] = "confirmed";
    inline constexpr char ConfirmationFinalized[] = "finalized";
} // namespace TxExecutionConstants

#endif // TXEXECUTIONCONSTANTS_H
