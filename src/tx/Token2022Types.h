#ifndef TOKEN2022TYPES_H
#define TOKEN2022TYPES_H

#include <QtGlobal>

namespace Token2022 {

    // ── Authority types for SetAuthority (index 6) ───────────
    // Base Token Program recognizes 0-3. Token-2022 extends to 0-14.

    namespace AuthorityType {
        inline constexpr quint8 MintTokens = 0;
        inline constexpr quint8 FreezeAccount = 1;
        inline constexpr quint8 AccountOwner = 2;
        inline constexpr quint8 CloseAccount = 3;
        inline constexpr quint8 TransferFeeConfig = 4;
        inline constexpr quint8 WithheldWithdraw = 5;
        inline constexpr quint8 CloseMint = 6;
        inline constexpr quint8 InterestRate = 7;
        inline constexpr quint8 PermanentDelegate = 8;
        inline constexpr quint8 ConfidentialTransferMint = 9;
        inline constexpr quint8 TransferHookProgramId = 10;
        inline constexpr quint8 ConfidentialTransferFeeConfig = 11;
        inline constexpr quint8 MetadataPointer = 12;
        inline constexpr quint8 GroupPointer = 13;
        inline constexpr quint8 GroupMemberPointer = 14;
    } // namespace AuthorityType

    // ── Extension types (u16) for Reallocate / GetAccountDataSize ──

    namespace ExtensionType {
        inline constexpr quint16 TransferFeeConfig = 1;
        inline constexpr quint16 TransferFeeAmount = 2;
        inline constexpr quint16 MintCloseAuthority = 3;
        inline constexpr quint16 ConfidentialTransferMint = 4;
        inline constexpr quint16 ConfidentialTransferAccount = 5;
        inline constexpr quint16 DefaultAccountState = 6;
        inline constexpr quint16 ImmutableOwner = 7;
        inline constexpr quint16 MemoTransfer = 8;
        inline constexpr quint16 NonTransferable = 9;
        inline constexpr quint16 InterestBearingConfig = 10;
        inline constexpr quint16 CpiGuard = 11;
        inline constexpr quint16 PermanentDelegate = 12;
        inline constexpr quint16 NonTransferableAccount = 13;
        inline constexpr quint16 TransferHook = 14;
        inline constexpr quint16 TransferHookAccount = 15;
        inline constexpr quint16 ConfidentialTransferFeeConfig = 16;
        inline constexpr quint16 ConfidentialTransferFeeAmount = 17;
        inline constexpr quint16 MetadataPointer = 18;
        inline constexpr quint16 TokenMetadata = 19;
        inline constexpr quint16 GroupPointer = 20;
        inline constexpr quint16 TokenGroup = 21;
        inline constexpr quint16 GroupMemberPointer = 22;
        inline constexpr quint16 TokenGroupMember = 23;
    } // namespace ExtensionType

    // ── Account state (u8) for DefaultAccountState extension ──

    namespace AccountState {
        inline constexpr quint8 Uninitialized = 0;
        inline constexpr quint8 Initialized = 1;
        inline constexpr quint8 Frozen = 2;
    } // namespace AccountState

    // ── Metadata field variants (Borsh enum, u8 tag) ──────

    namespace MetadataField {
        inline constexpr quint8 Name = 0;
        inline constexpr quint8 Symbol = 1;
        inline constexpr quint8 Uri = 2;
        // Variant 3 = Key(String): tag 3 followed by Borsh string
    } // namespace MetadataField

} // namespace Token2022

#endif // TOKEN2022TYPES_H
