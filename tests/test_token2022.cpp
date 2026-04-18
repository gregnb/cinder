#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/Token2022Instruction.h"
#include "tx/Token2022Types.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TokenGroupInstruction.h"
#include "tx/TokenInstruction.h"
#include "tx/TokenMetadataInstruction.h"
#include "tx/TransactionBuilder.h"
#include <QCoreApplication>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── Deterministic keys (same as generate_token2022_payloads.mjs) ──

static const QString T22_PAYER = "BrQgEgNfMCRBJHxSKafDxPqHWRdzmMujSVkbEGiYR3hC";
static const QString T22_RECIPIENT = "7xKXtg2CW87d97TXJSDpbD5jBkheTqA83TZRuJosgAsU";
static const QString T22_MINT = "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v";
static const QString T22_SOURCE_ATA = "DShWnroshVbeUp28oopA3Mc5YhceihJ7rLMv9HXLM36e";
static const QString T22_DEST_ATA = "4fYNwLBaVso1CJFDnGhNMQWNrveNyGprpjFzF6z3uFQG";
static const QString T22_BLOCKHASH = "EkSnNWid2cvwEVnVx9aBqawnmiCNiDgp3gUdkDPTKN1N";
static const QString T22_AUTHORITY = T22_PAYER;
static const QString T22_DELEGATE = T22_RECIPIENT;
static const QString T22_HOOK_PROG = "Hook111111111111111111111111111111111111111";

static QByteArray fromHex(const char* hex) { return QByteArray::fromHex(QByteArray(hex)); }

// ══════════════════════════════════════════════════════════
// Encoding Helper Tests
// ══════════════════════════════════════════════════════════

TEST(TokenEncodingTest, EncodeU8) {
    EXPECT_EQ(TokenEncoding::encodeU8(0), QByteArray(1, '\0'));
    EXPECT_EQ(TokenEncoding::encodeU8(255), QByteArray(1, '\xFF'));
}

TEST(TokenEncodingTest, EncodeU16LE) {
    QByteArray result = TokenEncoding::encodeU16(0x0100);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(static_cast<uchar>(result[0]), 0x00);
    EXPECT_EQ(static_cast<uchar>(result[1]), 0x01);
}

TEST(TokenEncodingTest, EncodeI16LE) {
    QByteArray result = TokenEncoding::encodeI16(500);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result, fromHex("f401"));

    QByteArray neg = TokenEncoding::encodeI16(-1);
    EXPECT_EQ(neg, fromHex("ffff"));
}

TEST(TokenEncodingTest, EncodeU32LE) {
    QByteArray result = TokenEncoding::encodeU32(7);
    EXPECT_EQ(result, fromHex("07000000"));
}

TEST(TokenEncodingTest, EncodeU64LE) {
    QByteArray result = TokenEncoding::encodeU64(1000000);
    EXPECT_EQ(result, fromHex("40420f0000000000"));
}

TEST(TokenEncodingTest, COptionPubkeyFull_None) {
    QByteArray result = TokenEncoding::encodeCOptionPubkeyNone();
    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result, QByteArray(4, '\0'));
}

TEST(TokenEncodingTest, COptionPubkeyFull_Some) {
    QByteArray result = TokenEncoding::encodeCOptionPubkey(T22_PAYER);
    ASSERT_EQ(result.size(), 36);
    EXPECT_EQ(static_cast<uchar>(result[0]), 0x01);
    EXPECT_EQ(static_cast<uchar>(result[1]), 0x00);
    EXPECT_EQ(static_cast<uchar>(result[2]), 0x00);
    EXPECT_EQ(static_cast<uchar>(result[3]), 0x00);
    EXPECT_EQ(result.mid(4, 32), Base58::decode(T22_PAYER));
}

TEST(TokenEncodingTest, InstructionPubkeyOption_None) {
    QByteArray result = TokenEncoding::encodeInstructionPubkeyOptionNone();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(static_cast<uchar>(result[0]), 0x00);
}

TEST(TokenEncodingTest, InstructionPubkeyOption_Some) {
    QByteArray result = TokenEncoding::encodeInstructionPubkeyOption(T22_PAYER);
    ASSERT_EQ(result.size(), 33);
    EXPECT_EQ(static_cast<uchar>(result[0]), 0x01);
    EXPECT_EQ(result.mid(1, 32), Base58::decode(T22_PAYER));
}

TEST(TokenEncodingTest, OptionalNonZeroPubkey_None) {
    QByteArray result = TokenEncoding::encodeOptionalNonZeroPubkeyNone();
    ASSERT_EQ(result.size(), 32);
    EXPECT_EQ(result, QByteArray(32, '\0'));
}

TEST(TokenEncodingTest, OptionalNonZeroPubkey_Some) {
    QByteArray result = TokenEncoding::encodeOptionalNonZeroPubkey(T22_PAYER);
    ASSERT_EQ(result.size(), 32);
    EXPECT_EQ(result, Base58::decode(T22_PAYER));
}

TEST(TokenEncodingTest, OptionalNonZeroPubkeyOpt_Empty) {
    EXPECT_EQ(TokenEncoding::encodeOptionalNonZeroPubkeyOpt(QString()), QByteArray(32, '\0'));
}

TEST(TokenEncodingTest, BorshString) {
    QByteArray result = TokenEncoding::encodeBorshString("MyToken");
    // u32 LE length (7) + "MyToken"
    EXPECT_EQ(result, fromHex("070000004d79546f6b656e"));
}

TEST(TokenEncodingTest, BorshOptionU64_None) {
    QByteArray result = TokenEncoding::encodeBorshOptionU64(std::nullopt);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(static_cast<uchar>(result[0]), 0x00);
}

TEST(TokenEncodingTest, BorshOptionU64_Some) {
    QByteArray result = TokenEncoding::encodeBorshOptionU64(quint64(42));
    ASSERT_EQ(result.size(), 9);
    EXPECT_EQ(static_cast<uchar>(result[0]), 0x01);
    EXPECT_EQ(result.mid(1), TokenEncoding::encodeU64(42));
}

// ══════════════════════════════════════════════════════════
// Base Instruction Structural Tests
// ══════════════════════════════════════════════════════════

TEST(BaseInstructionTest, InitializeMint2_DataLayout) {
    auto ix = TokenInstruction::initializeMint2(T22_MINT, 6, T22_PAYER, T22_PAYER,
                                                SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.accounts.size(), 1);
    // tag(1) + decimals(1) + mintAuth(32) + COption Some(1+32) = 67
    EXPECT_EQ(ix.data.size(), 67);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x14);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 6); // decimals
}

TEST(BaseInstructionTest, InitializeMint2_NoFreeze) {
    auto ix = TokenInstruction::initializeMint2(T22_MINT, 9, T22_PAYER, QString(),
                                                SolanaPrograms::Token2022Program);
    // tag(1) + decimals(1) + mintAuth(32) + COption None(1) = 35
    EXPECT_EQ(ix.data.size(), 35);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x14);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 9);
    EXPECT_EQ(static_cast<uchar>(ix.data[34]), 0x00); // COption None
}

TEST(BaseInstructionTest, SetAuthority_DataLayout) {
    auto ix =
        TokenInstruction::setAuthority(T22_MINT, T22_PAYER, Token2022::AuthorityType::CloseMint,
                                       T22_RECIPIENT, SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.accounts.size(), 2);
    // tag(1) + authType(1) + COption Some(1+32) = 35
    EXPECT_EQ(ix.data.size(), 35);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x06);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), Token2022::AuthorityType::CloseMint);
    EXPECT_EQ(static_cast<uchar>(ix.data[2]), 0x01); // COption Some
}

TEST(BaseInstructionTest, SetAuthority_Revoke) {
    auto ix =
        TokenInstruction::setAuthority(T22_MINT, T22_PAYER, Token2022::AuthorityType::MintTokens,
                                       QString(), SolanaPrograms::Token2022Program);
    // tag(1) + authType(1) + COption None(1) = 3
    EXPECT_EQ(ix.data.size(), 3);
    EXPECT_EQ(static_cast<uchar>(ix.data[2]), 0x00); // COption None
}

TEST(BaseInstructionTest, Revoke_DataLayout) {
    auto ix = TokenInstruction::revoke(T22_SOURCE_ATA, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x05);
    EXPECT_EQ(ix.accounts.size(), 2);
}

TEST(BaseInstructionTest, MintTo_DataLayout) {
    auto ix = TokenInstruction::mintTo(T22_MINT, T22_DEST_ATA, T22_PAYER, 1000000);
    EXPECT_EQ(ix.data.size(), 9); // tag(1) + u64(8)
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x07);
}

TEST(BaseInstructionTest, FreezeAccount_DataLayout) {
    auto ix = TokenInstruction::freezeAccount(T22_SOURCE_ATA, T22_MINT, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x0A);
    EXPECT_EQ(ix.accounts.size(), 3);
}

TEST(BaseInstructionTest, ThawAccount_DataLayout) {
    auto ix = TokenInstruction::thawAccount(T22_SOURCE_ATA, T22_MINT, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x0B);
}

TEST(BaseInstructionTest, ApproveChecked_DataLayout) {
    auto ix =
        TokenInstruction::approveChecked(T22_SOURCE_ATA, T22_MINT, T22_DELEGATE, T22_PAYER, 500, 6);
    EXPECT_EQ(ix.data.size(), 10); // tag(1) + u64(8) + u8(1)
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x0D);
    EXPECT_EQ(static_cast<uchar>(ix.data[9]), 6);
    EXPECT_EQ(ix.accounts.size(), 4);
}

TEST(BaseInstructionTest, MintToChecked_DataLayout) {
    auto ix = TokenInstruction::mintToChecked(T22_MINT, T22_DEST_ATA, T22_PAYER, 1000000, 6);
    EXPECT_EQ(ix.data.size(), 10);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x0E);
}

TEST(BaseInstructionTest, BurnChecked_DataLayout) {
    auto ix = TokenInstruction::burnChecked(T22_SOURCE_ATA, T22_MINT, T22_PAYER, 1000000, 6);
    EXPECT_EQ(ix.data.size(), 10);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x0F);
}

TEST(BaseInstructionTest, SyncNative_DataLayout) {
    auto ix = TokenInstruction::syncNative(T22_SOURCE_ATA);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x11);
    EXPECT_EQ(ix.accounts.size(), 1);
}

TEST(BaseInstructionTest, InitializeAccount3_DataLayout) {
    auto ix = TokenInstruction::initializeAccount3(T22_SOURCE_ATA, T22_MINT, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 33); // tag(1) + pubkey(32)
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x12);
    EXPECT_EQ(ix.accounts.size(), 2);
}

// ══════════════════════════════════════════════════════════
// Token-2022 Standalone Instruction Tests
// ══════════════════════════════════════════════════════════

TEST(Token2022InstructionTest, InitializeMintCloseAuthority_DataLayout) {
    auto ix = Token2022Instruction::initializeMintCloseAuthority(T22_MINT, T22_PAYER);
    EXPECT_EQ(ix.programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.accounts.size(), 1);
    // tag(1) + COption Some(1+32) = 34
    EXPECT_EQ(ix.data.size(), 34);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x19);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x01); // Some
}

TEST(Token2022InstructionTest, InitializeMintCloseAuthority_NoAuth) {
    auto ix = Token2022Instruction::initializeMintCloseAuthority(T22_MINT);
    // tag(1) + COption None(1) = 2
    EXPECT_EQ(ix.data.size(), 2);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
}

TEST(Token2022InstructionTest, InitializeImmutableOwner) {
    auto ix = Token2022Instruction::initializeImmutableOwner(T22_SOURCE_ATA);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x16);
}

TEST(Token2022InstructionTest, InitializeNonTransferableMint) {
    auto ix = Token2022Instruction::initializeNonTransferableMint(T22_MINT);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x20);
}

TEST(Token2022InstructionTest, InitializePermanentDelegate) {
    auto ix = Token2022Instruction::initializePermanentDelegate(T22_MINT, T22_DELEGATE);
    EXPECT_EQ(ix.data.size(), 33); // tag(1) + pubkey(32)
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x23);
    EXPECT_EQ(ix.data.mid(1, 32), Base58::decode(T22_DELEGATE));
}

TEST(Token2022InstructionTest, Reallocate_DataLayout) {
    auto ix = Token2022Instruction::reallocate(
        T22_SOURCE_ATA, T22_PAYER, T22_PAYER,
        {Token2022::ExtensionType::MemoTransfer, Token2022::ExtensionType::CpiGuard});
    EXPECT_EQ(ix.programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.accounts.size(), 4); // account, payer, system, owner
    // tag(1) + 2 * u16(2) = 5
    EXPECT_EQ(ix.data.size(), 5);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x1D);
    EXPECT_EQ(ix.data.mid(1, 2), fromHex("0800")); // MemoTransfer = 8
    EXPECT_EQ(ix.data.mid(3, 2), fromHex("0b00")); // CpiGuard = 11
}

TEST(Token2022InstructionTest, WithdrawExcessLamports) {
    auto ix = Token2022Instruction::withdrawExcessLamports(T22_SOURCE_ATA, T22_DEST_ATA, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x26);
    EXPECT_EQ(ix.accounts.size(), 3);
}

// ══════════════════════════════════════════════════════════
// Extension Sub-Instruction Tests
// ══════════════════════════════════════════════════════════

TEST(TransferFeeTest, InitConfig_DataLayout) {
    auto ix = Token2022Instruction::TransferFee::initializeTransferFeeConfig(
        T22_MINT, T22_PAYER, T22_PAYER, 100, 1000000);
    // tag(1) + sub(1) + 2*OptionalNonZeroPubkey(32) + u16(2) + u64(8) = 76
    EXPECT_EQ(ix.data.size(), 76);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x1A);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
}

TEST(TransferFeeTest, TransferCheckedWithFee_DataLayout) {
    auto ix = Token2022Instruction::TransferFee::transferCheckedWithFee(
        T22_SOURCE_ATA, T22_MINT, T22_DEST_ATA, T22_PAYER, 1000000, 6, 100);
    // tag(1) + sub(1) + u64(8) + u8(1) + u64(8) = 19
    EXPECT_EQ(ix.data.size(), 19);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x1A);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x01);
    EXPECT_EQ(ix.accounts.size(), 4);
}

TEST(TransferFeeTest, WithdrawFromMint) {
    auto ix = Token2022Instruction::TransferFee::withdrawWithheldTokensFromMint(
        T22_MINT, T22_DEST_ATA, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 2);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x02);
}

TEST(TransferFeeTest, WithdrawFromAccounts) {
    auto ix = Token2022Instruction::TransferFee::withdrawWithheldTokensFromAccounts(
        T22_MINT, T22_DEST_ATA, T22_PAYER, {T22_SOURCE_ATA});
    EXPECT_EQ(ix.data.size(), 3);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x03);
    EXPECT_EQ(static_cast<uchar>(ix.data[2]), 1); // 1 source account
    EXPECT_EQ(ix.accounts.size(), 4);             // mint, dest, auth, source
}

TEST(TransferFeeTest, HarvestToMint) {
    auto ix = Token2022Instruction::TransferFee::harvestWithheldTokensToMint(
        T22_MINT, {T22_SOURCE_ATA, T22_DEST_ATA});
    EXPECT_EQ(ix.data.size(), 2);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x04);
    EXPECT_EQ(ix.accounts.size(), 3); // mint + 2 sources
}

TEST(TransferFeeTest, SetTransferFee_DataLayout) {
    auto ix = Token2022Instruction::TransferFee::setTransferFee(T22_MINT, T22_PAYER, 200, 5000000);
    // tag(1) + sub(1) + u16(2) + u64(8) = 12
    EXPECT_EQ(ix.data.size(), 12);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x05);
}

TEST(DefaultAccountStateTest, Initialize_Frozen) {
    auto ix = Token2022Instruction::DefaultAccountState::initialize(
        T22_MINT, Token2022::AccountState::Frozen);
    EXPECT_EQ(ix.data.size(), 3);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x1C);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
    EXPECT_EQ(static_cast<uchar>(ix.data[2]), Token2022::AccountState::Frozen);
}

TEST(DefaultAccountStateTest, Update) {
    auto ix = Token2022Instruction::DefaultAccountState::update(
        T22_MINT, T22_PAYER, Token2022::AccountState::Initialized);
    EXPECT_EQ(ix.data.size(), 3);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x01);
}

TEST(MemoTransferTest, Enable) {
    auto ix = Token2022Instruction::MemoTransfer::enable(T22_SOURCE_ATA, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 2);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x1E);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
}

TEST(MemoTransferTest, Disable) {
    auto ix = Token2022Instruction::MemoTransfer::disable(T22_SOURCE_ATA, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 2);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x01);
}

TEST(InterestBearingMintTest, Initialize) {
    auto ix = Token2022Instruction::InterestBearingMint::initialize(T22_MINT, T22_PAYER, 500);
    // tag(1) + sub(1) + OptNZP(32) + i16(2) = 36
    EXPECT_EQ(ix.data.size(), 36);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x21);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
    EXPECT_EQ(ix.data.mid(34, 2), fromHex("f401")); // 500 as i16 LE
}

TEST(InterestBearingMintTest, UpdateRate) {
    auto ix = Token2022Instruction::InterestBearingMint::updateRate(T22_MINT, T22_PAYER, 750);
    // tag(1) + sub(1) + i16(2) = 4
    EXPECT_EQ(ix.data.size(), 4);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x01);
}

TEST(CpiGuardTest, Enable) {
    auto ix = Token2022Instruction::CpiGuard::enable(T22_SOURCE_ATA, T22_PAYER);
    EXPECT_EQ(ix.data.size(), 2);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x22);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
}

TEST(CpiGuardTest, Disable) {
    auto ix = Token2022Instruction::CpiGuard::disable(T22_SOURCE_ATA, T22_PAYER);
    EXPECT_EQ(ix.data, fromHex("2201"));
}

TEST(TransferHookTest, Initialize) {
    auto ix = Token2022Instruction::TransferHook::initialize(T22_MINT, T22_PAYER, T22_HOOK_PROG);
    // tag(1) + sub(1) + 2*OptNZP(32) = 66
    EXPECT_EQ(ix.data.size(), 66);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x24);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
}

TEST(TransferHookTest, Update) {
    auto ix = Token2022Instruction::TransferHook::update(T22_MINT, T22_PAYER, T22_HOOK_PROG);
    // tag(1) + sub(1) + OptNZP(32) = 34
    EXPECT_EQ(ix.data.size(), 34);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x01);
}

TEST(MetadataPointerTest, Initialize) {
    auto ix = Token2022Instruction::MetadataPointer::initialize(T22_MINT, T22_PAYER, T22_MINT);
    // tag(1) + sub(1) + 2*OptNZP(32) = 66
    EXPECT_EQ(ix.data.size(), 66);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x27);
    EXPECT_EQ(static_cast<uchar>(ix.data[1]), 0x00);
}

TEST(GroupPointerTest, Initialize) {
    auto ix = Token2022Instruction::GroupPointer::initialize(T22_MINT, T22_PAYER, T22_MINT);
    EXPECT_EQ(ix.data.size(), 66);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x28);
}

TEST(GroupMemberPointerTest, Initialize) {
    auto ix = Token2022Instruction::GroupMemberPointer::initialize(T22_MINT, T22_PAYER, T22_MINT);
    EXPECT_EQ(ix.data.size(), 66);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x29);
}

// ══════════════════════════════════════════════════════════
// Token Metadata Interface Tests
// ══════════════════════════════════════════════════════════

TEST(TokenMetadataTest, Initialize_Discriminator) {
    auto ix = TokenMetadataInstruction::initialize(T22_MINT, T22_PAYER, T22_MINT, T22_PAYER,
                                                   "MyToken", "MTK", "https://example.com");
    EXPECT_EQ(ix.data.left(8), fromHex("d2e11ea258b84d8d"));
    EXPECT_EQ(ix.accounts.size(), 4);
}

TEST(TokenMetadataTest, Initialize_DataEncoding) {
    auto ix = TokenMetadataInstruction::initialize(T22_MINT, T22_PAYER, T22_MINT, T22_PAYER,
                                                   "MyToken", "MTK", "https://example.com");
    // disc(8) + "MyToken"(4+7) + "MTK"(4+3) + "https://example.com"(4+19)
    EXPECT_EQ(ix.data.size(), 8 + 11 + 7 + 23);
    EXPECT_EQ(ix.data.mid(8, 4), fromHex("07000000")); // name length
}

TEST(TokenMetadataTest, UpdateField_Name) {
    auto ix = TokenMetadataInstruction::updateField(T22_MINT, T22_PAYER,
                                                    Token2022::MetadataField::Name, "NewName");
    EXPECT_EQ(ix.data.left(8), fromHex("dde9312db5cadcc8"));
    // disc(8) + variant(1) + "NewName"(4+7) = 20
    EXPECT_EQ(ix.data.size(), 20);
    EXPECT_EQ(static_cast<uchar>(ix.data[8]), 0x00); // Name = variant 0
}

TEST(TokenMetadataTest, UpdateFieldCustomKey) {
    auto ix = TokenMetadataInstruction::updateFieldCustomKey(T22_MINT, T22_PAYER, "website",
                                                             "https://example.com");
    EXPECT_EQ(ix.data.left(8), fromHex("dde9312db5cadcc8"));
    EXPECT_EQ(static_cast<uchar>(ix.data[8]), 0x03); // Key = variant 3
}

TEST(TokenMetadataTest, RemoveKey_Discriminator) {
    auto ix = TokenMetadataInstruction::removeKey(T22_MINT, T22_PAYER, true, "custom_key");
    EXPECT_EQ(ix.data.left(8), fromHex("ea122038598d25b5"));
    // disc(8) + bool(1) + "custom_key"(4+10) = 23
    EXPECT_EQ(ix.data.size(), 23);
    EXPECT_EQ(static_cast<uchar>(ix.data[8]), 0x01); // idempotent = true
}

TEST(TokenMetadataTest, UpdateAuthority_Discriminator) {
    auto ix = TokenMetadataInstruction::updateAuthority(T22_MINT, T22_PAYER, T22_RECIPIENT);
    EXPECT_EQ(ix.data.left(8), fromHex("d7e4a6e45464567b"));
    // disc(8) + OptNZP(32) = 40
    EXPECT_EQ(ix.data.size(), 40);
}

TEST(TokenMetadataTest, Emit_Discriminator) {
    auto ix = TokenMetadataInstruction::emitMetadata(T22_MINT);
    EXPECT_EQ(ix.data.left(8), fromHex("faa6b4fa0d0cb846"));
    // disc(8) + Option<u64> None(1) + Option<u64> None(1) = 10
    EXPECT_EQ(ix.data.size(), 10);
}

// ══════════════════════════════════════════════════════════
// Token Group Interface Tests
// ══════════════════════════════════════════════════════════

TEST(TokenGroupTest, InitializeGroup_Discriminator) {
    auto ix = TokenGroupInstruction::initializeGroup(T22_MINT, T22_MINT, T22_PAYER, T22_PAYER, 100);
    EXPECT_EQ(ix.data.left(8), fromHex("79716c2736330004"));
    // disc(8) + OptNZP(32) + u64(8) = 48
    EXPECT_EQ(ix.data.size(), 48);
    EXPECT_EQ(ix.accounts.size(), 3);
}

TEST(TokenGroupTest, UpdateMaxSize_Discriminator) {
    auto ix = TokenGroupInstruction::updateGroupMaxSize(T22_MINT, T22_PAYER, 200);
    EXPECT_EQ(ix.data.left(8), fromHex("6c25ab8ff81e126e"));
    // disc(8) + u64(8) = 16
    EXPECT_EQ(ix.data.size(), 16);
}

TEST(TokenGroupTest, UpdateAuthority_Discriminator) {
    auto ix = TokenGroupInstruction::updateGroupAuthority(T22_MINT, T22_PAYER, T22_RECIPIENT);
    EXPECT_EQ(ix.data.left(8), fromHex("a1695801edddd8cb"));
    // disc(8) + OptNZP(32) = 40
    EXPECT_EQ(ix.data.size(), 40);
}

TEST(TokenGroupTest, InitializeMember_Discriminator) {
    auto ix =
        TokenGroupInstruction::initializeMember(T22_MINT, T22_MINT, T22_PAYER, T22_MINT, T22_PAYER);
    EXPECT_EQ(ix.data.left(8), fromHex("9820deb0dfed7486"));
    EXPECT_EQ(ix.data.size(), 8); // discriminator only
    EXPECT_EQ(ix.accounts.size(), 5);
}

// ══════════════════════════════════════════════════════════
// Byte-for-byte Reference Payload Tests
// Generated by: node tests/generate_token2022_payloads.mjs
// ══════════════════════════════════════════════════════════

TEST(Token2022ReferenceTest, InitializeMint2_WithFreeze) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01020101431406a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9"
                                  "851157b970e0f301a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7d"
                                  "d9851157b970e0f3");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(TokenInstruction::initializeMint2(
                             T22_MINT, 6, T22_PAYER, T22_PAYER, SolanaPrograms::Token2022Program))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, InitializeMint2_NoFreeze) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "010201012314"
                "09a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f300");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(TokenInstruction::initializeMint2(
                             T22_MINT, 9, T22_PAYER, QString(), SolanaPrograms::Token2022Program))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, SetAuthority_CloseMint) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "01020201002306"
                "06016752055c20b3e9d8746656ddf73855507f87ab6d87523e4c76a7fa36096a99eb");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(TokenInstruction::setAuthority(
                             T22_MINT, T22_PAYER, Token2022::AuthorityType::CloseMint,
                             T22_RECIPIENT, SolanaPrograms::Token2022Program))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, SetAuthority_Revoke) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "0102020100030600"
                                  "00");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(TokenInstruction::setAuthority(
                             T22_MINT, T22_PAYER, Token2022::AuthorityType::MintTokens, QString(),
                             SolanaPrograms::Token2022Program))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, InitializeMintCloseAuthority) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "0102010122"
                "1901a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::initializeMintCloseAuthority(T22_MINT, T22_PAYER))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, InitializeNonTransferableMint) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "0102010101"
                                  "20");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::initializeNonTransferableMint(T22_MINT))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, InitializePermanentDelegate) {
    // Full hex from JS SDK (single string to avoid split errors)
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "0102010121"
                "236752055c20b3e9d8746656ddf73855507f87ab6d87523e4c76a7fa36096a99eb");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::initializePermanentDelegate(
                             T22_MINT, T22_DELEGATE))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, TransferFee_InitConfig) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "010201014c"
                                  "1a00"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "640040420f0000000000");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::TransferFee::initializeTransferFeeConfig(
                T22_MINT, T22_PAYER, T22_PAYER, 100, 1000000))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, TransferFee_TransferCheckedWithFee) {
    // Account order differs from JS SDK: our QMap sorts alphabetically by base58
    QByteArray expected = fromHex("01000205"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "36729c69d5da6cf3c112b102ff11aeb6abe80ea251619204b648c9d3f08cdf5d"
                                  "b8e1927f2da9f65b5f42129930d617930db3d1982d0ce22b4effe6ba1566b09f"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01040402030100131a0140420f0000000000066400000000000000");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::TransferFee::transferCheckedWithFee(
                             T22_SOURCE_ATA, T22_MINT, T22_DEST_ATA, T22_PAYER, 1000000, 6, 100))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, DefaultAccountState_InitFrozen) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "0102010103"
                                  "1c0002");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::DefaultAccountState::initialize(
                             T22_MINT, Token2022::AccountState::Frozen))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, MemoTransfer_Enable) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "b8e1927f2da9f65b5f42129930d617930db3d1982d0ce22b4effe6ba1566b09f"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "0102020100021e00");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::MemoTransfer::enable(T22_SOURCE_ATA, T22_PAYER))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, InterestBearingMint_Init) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01020101242100"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "f401");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::InterestBearingMint::initialize(
                             T22_MINT, T22_PAYER, 500))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, CpiGuard_Enable) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "b8e1927f2da9f65b5f42129930d617930db3d1982d0ce22b4effe6ba1566b09f"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "0102020100022200");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::CpiGuard::enable(T22_SOURCE_ATA, T22_PAYER))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, TransferHook_Init) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "01020101422400"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "044e376e72b82297855fe613503d4a7dc619ab5045d6562b3b137d8000000000");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::TransferHook::initialize(
                             T22_MINT, T22_PAYER, T22_HOOK_PROG))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, MetadataPointer_Init) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "01020101422700"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::MetadataPointer::initialize(
                             T22_MINT, T22_PAYER, T22_MINT))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, GroupPointer_Init) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "01020101422800"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::GroupPointer::initialize(
                             T22_MINT, T22_PAYER, T22_MINT))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, GroupMemberPointer_Init) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "01020101422900"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(Token2022Instruction::GroupMemberPointer::initialize(
                             T22_MINT, T22_PAYER, T22_MINT))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, TokenMetadata_Initialize) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "0102040100010031"
                                  "d2e11ea258b84d8d"
                                  "070000004d79546f6b656e"
                                  "030000004d544b"
                                  "1300000068747470733a2f2f6578616d706c652e636f6d");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(TokenMetadataInstruction::initialize(
                T22_MINT, T22_PAYER, T22_MINT, T22_PAYER, "MyToken", "MTK", "https://example.com"))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, TokenMetadata_UpdateField_Name) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "010202010014"
                                  "dde9312db5cadcc8"
                                  "00"
                                  "070000004e65774e616d65");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(TokenMetadataInstruction::updateField(
                             T22_MINT, T22_PAYER, Token2022::MetadataField::Name, "NewName"))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, TokenMetadata_RemoveKey) {
    QByteArray expected = fromHex("01000103"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "010202010017"
                                  "ea122038598d25b5"
                                  "01"
                                  "0a000000637573746f6d5f6b6579");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(T22_PAYER)
                         .setRecentBlockhash(T22_BLOCKHASH)
                         .addInstruction(TokenMetadataInstruction::removeKey(T22_MINT, T22_PAYER,
                                                                             true, "custom_key"))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, Reallocate_MemoAndCpiGuard) {
    // Account order differs from JS SDK: our QMap sorts alphabetically by base58
    QByteArray expected = fromHex("01000204"
                                  "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                                  "b8e1927f2da9f65b5f42129930d617930db3d1982d0ce22b4effe6ba1566b09f"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01030401000200051d08000b00");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::reallocate(
                T22_SOURCE_ATA, T22_PAYER, T22_PAYER,
                {Token2022::ExtensionType::MemoTransfer, Token2022::ExtensionType::CpiGuard}))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

TEST(Token2022ReferenceTest, MultiIx_CreateMintWithExtensions) {
    QByteArray expected =
        fromHex("01000103"
                "a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                "05"
                "020101221901a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "020101422700a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f3"
                "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                "0201010120"
                "020101231400a13d1998e9ed2e5d1e94a2787def8e77704ac8420a955d7dd9851157b970e0f300"
                "02040100010041"
                "d2e11ea258b84d8d"
                "09000000536f756c426f756e64"
                "03000000534254"
                "2100000068747470733a2f2f7362742e6578616d706c652e636f6d2f6d6574612e6a736f6e");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(T22_PAYER)
            .setRecentBlockhash(T22_BLOCKHASH)
            .addInstruction(Token2022Instruction::initializeMintCloseAuthority(T22_MINT, T22_PAYER))
            .addInstruction(
                Token2022Instruction::MetadataPointer::initialize(T22_MINT, T22_PAYER, T22_MINT))
            .addInstruction(Token2022Instruction::initializeNonTransferableMint(T22_MINT))
            .addInstruction(TokenInstruction::initializeMint2(T22_MINT, 0, T22_PAYER, QString(),
                                                              SolanaPrograms::Token2022Program))
            .addInstruction(TokenMetadataInstruction::initialize(
                T22_MINT, T22_PAYER, T22_MINT, T22_PAYER, "SoulBound", "SBT",
                "https://sbt.example.com/meta.json"))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}
