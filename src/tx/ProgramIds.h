#ifndef PROGRAMIDS_H
#define PROGRAMIDS_H

#include <QString>

namespace SolanaPrograms {

    // Core
    inline const QString SystemProgram = "11111111111111111111111111111111";
    inline const QString ComputeBudget = "ComputeBudget111111111111111111111111111111";

    // Token
    inline const QString TokenProgram = "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
    inline const QString Token2022Program = "TokenzQdBNbLqP5VEhdkAS6EPFLC1PHnBqCXEpPxuEb";
    inline const QString AssociatedTokenAccount = "ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL";

    // Utility
    inline const QString MemoProgram = "MemoSq4gqABAXKb96qnH8TysNcWxMyWCqXgDLGmfcHr";
    inline const QString AddressLookupTable = "AddressLookupTab1e1111111111111111111111111";

    // Staking / Governance
    inline const QString StakeProgram = "Stake11111111111111111111111111111111111111";
    inline const QString VoteProgram = "Vote111111111111111111111111111111111111111";

    // Metaplex
    inline const QString MetaplexTokenMetadata = "metaqbxxUerdq28cj1RbAWkYQm3ybzjb6a8bt518x1s";

    // Loaders
    inline const QString BpfLoader = "BPFLoader2111111111111111111111111111111111";
    inline const QString BpfUpgradeableLoader = "BPFLoaderUpgradeab1e11111111111111111111111";

    // Sysvars (used in nonce and staking instructions)
    inline const QString RecentBlockhashesSysvar = "SysvarRecentB1ockHashes11111111111111111111";
    inline const QString RentSysvar = "SysvarRent111111111111111111111111111111111";
    inline const QString ClockSysvar = "SysvarC1ock11111111111111111111111111111111";
    inline const QString StakeHistorySysvar = "SysvarStakeHistory1111111111111111111111111";
    inline const QString StakeConfigAddress = "StakeConfig11111111111111111111111111111111";

    // Returns true if the programId is one of the two SPL Token programs
    inline bool isTokenProgram(const QString& programId) {
        return programId == TokenProgram || programId == Token2022Program;
    }

    // Returns true if programId is Token-2022 (Token Extensions)
    inline bool isToken2022(const QString& programId) { return programId == Token2022Program; }

} // namespace SolanaPrograms

#endif // PROGRAMIDS_H
