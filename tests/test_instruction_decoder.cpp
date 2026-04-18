#include "tx/Base58.h"
#include "tx/ComputeBudgetInstruction.h"
#include "tx/InstructionDecoder.h"
#include "tx/ProgramIds.h"
#include "tx/SystemInstruction.h"
#include "tx/TxParseUtils.h"
#include <QCoreApplication>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── ComputeBudget: SetComputeUnitLimit ───────────────────

TEST(InstructionDecoderTest, DecodeSetComputeUnitLimit) {
    auto txIx = ComputeBudgetInstruction::setComputeUnitLimit(200000);

    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = Base58::encode(txIx.data);

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_TRUE(ix.isParsed());
    EXPECT_EQ(ix.program, "compute-budget");
    EXPECT_EQ(ix.type, "setComputeUnitLimit");
    EXPECT_EQ(ix.info["units"].toInteger(), 200000);
}

TEST(InstructionDecoderTest, DecodeSetComputeUnitLimitMax) {
    auto txIx = ComputeBudgetInstruction::setComputeUnitLimit(1400000);

    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = Base58::encode(txIx.data);

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_EQ(ix.info["units"].toInteger(), 1400000);
}

// ── ComputeBudget: SetComputeUnitPrice ───────────────────

TEST(InstructionDecoderTest, DecodeSetComputeUnitPrice) {
    auto txIx = ComputeBudgetInstruction::setComputeUnitPrice(50000);

    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = Base58::encode(txIx.data);

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_TRUE(ix.isParsed());
    EXPECT_EQ(ix.program, "compute-budget");
    EXPECT_EQ(ix.type, "setComputeUnitPrice");
    EXPECT_EQ(ix.info["microLamports"].toInteger(), 50000);
}

// ── Real transaction data from mainnet ───────────────────

TEST(InstructionDecoderTest, DecodeRealComputeUnitPrice) {
    // "3axL5qdEKYoR" is from tx 4Wv6ngC3aMZp...
    // Decodes to disc=0x03, microLamports=2000000
    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = "3axL5qdEKYoR";

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_EQ(ix.program, "compute-budget");
    EXPECT_EQ(ix.type, "setComputeUnitPrice");
    EXPECT_EQ(ix.info["microLamports"].toInteger(), 2000000);
}

// ── ComputeBudget: RequestHeapFrame ──────────────────────

TEST(InstructionDecoderTest, DecodeRequestHeapFrame) {
    // Build disc=0x00 + u32 LE 262144 (256 KiB)
    QByteArray raw(5, '\0');
    raw[0] = 0x00;
    quint32 bytes = 262144;
    qToLittleEndian(bytes, reinterpret_cast<uchar*>(raw.data() + 1));

    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = Base58::encode(raw);

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_EQ(ix.type, "requestHeapFrame");
    EXPECT_EQ(ix.info["bytes"].toInteger(), 262144);
}

// ── ComputeBudget: SetLoadedAccountsDataSizeLimit ────────

TEST(InstructionDecoderTest, DecodeSetLoadedAccountsDataSizeLimit) {
    QByteArray raw(5, '\0');
    raw[0] = 0x01;
    quint32 bytes = 64000;
    qToLittleEndian(bytes, reinterpret_cast<uchar*>(raw.data() + 1));

    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = Base58::encode(raw);

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_EQ(ix.type, "setLoadedAccountsDataSizeLimit");
    EXPECT_EQ(ix.info["bytes"].toInteger(), 64000);
}

// ── System Program: Transfer ─────────────────────────────

TEST(InstructionDecoderTest, DecodeSystemTransfer) {
    QByteArray raw(12, '\0');
    quint32 disc = 2;
    quint64 lamports = 1000000000;
    qToLittleEndian(disc, reinterpret_cast<uchar*>(raw.data()));
    qToLittleEndian(lamports, reinterpret_cast<uchar*>(raw.data() + 4));

    Instruction ix;
    ix.programId = SolanaPrograms::SystemProgram;
    ix.data = Base58::encode(raw);
    ix.accounts = QJsonArray({"7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7",
                              "9B227TPFG5SLANT1r34hT23XCqnWmWwTG7oExmDVqVzK"});

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_TRUE(ix.isParsed());
    EXPECT_EQ(ix.program, "system");
    EXPECT_EQ(ix.type, "transfer");
    EXPECT_EQ(ix.info["lamports"].toInteger(), 1000000000);
    EXPECT_EQ(ix.info["source"].toString(), "7Ci23i82UMa8RpfVbdMjTytiDi2VoZS8uLyHhZBV2Qy7");
    EXPECT_EQ(ix.info["destination"].toString(), "9B227TPFG5SLANT1r34hT23XCqnWmWwTG7oExmDVqVzK");
}

// ── System Program: CreateAccount ────────────────────────

TEST(InstructionDecoderTest, DecodeSystemCreateAccount) {
    // disc=0 (u32 LE) + lamports (u64 LE) + space (u64 LE) + owner (32 bytes)
    QByteArray raw(52, '\0');
    quint32 disc = 0;
    quint64 lamports = 2039280;
    quint64 space = 165;
    qToLittleEndian(disc, reinterpret_cast<uchar*>(raw.data()));
    qToLittleEndian(lamports, reinterpret_cast<uchar*>(raw.data() + 4));
    qToLittleEndian(space, reinterpret_cast<uchar*>(raw.data() + 12));
    // owner = Token Program
    QByteArray ownerBytes = Base58::decode(SolanaPrograms::TokenProgram);
    memcpy(raw.data() + 20, ownerBytes.constData(), 32);

    Instruction ix;
    ix.programId = SolanaPrograms::SystemProgram;
    ix.data = Base58::encode(raw);
    ix.accounts = QJsonArray({"SourcePubkey", "NewAccountPubkey"});

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_EQ(ix.program, "system");
    EXPECT_EQ(ix.type, "createAccount");
    EXPECT_EQ(ix.info["lamports"].toInteger(), 2039280);
    EXPECT_EQ(ix.info["space"].toInteger(), 165);
    EXPECT_EQ(ix.info["owner"].toString(), SolanaPrograms::TokenProgram);
    EXPECT_EQ(ix.info["source"].toString(), "SourcePubkey");
    EXPECT_EQ(ix.info["newAccount"].toString(), "NewAccountPubkey");
}

// ── System Program: AdvanceNonce ─────────────────────────

TEST(InstructionDecoderTest, DecodeSystemAdvanceNonce) {
    QByteArray raw(4, '\0');
    quint32 disc = 4;
    qToLittleEndian(disc, reinterpret_cast<uchar*>(raw.data()));

    Instruction ix;
    ix.programId = SolanaPrograms::SystemProgram;
    ix.data = Base58::encode(raw);
    ix.accounts = QJsonArray({"NonceAccount", "RecentBlockhashes", "NonceAuthority"});

    ASSERT_TRUE(InstructionDecoder::tryDecode(ix));
    EXPECT_EQ(ix.type, "advanceNonce");
    EXPECT_EQ(ix.info["nonceAccount"].toString(), "NonceAccount");
    EXPECT_EQ(ix.info["nonceAuthority"].toString(), "NonceAuthority");
}

// ── Edge cases ───────────────────────────────────────────

TEST(InstructionDecoderTest, AlreadyParsedIsSkipped) {
    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.program = "compute-budget";
    ix.type = "setComputeUnitPrice";
    ix.data = "3axL5qdEKYoR";

    EXPECT_FALSE(InstructionDecoder::tryDecode(ix));
}

TEST(InstructionDecoderTest, EmptyDataReturnsFalse) {
    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;

    EXPECT_FALSE(InstructionDecoder::tryDecode(ix));
}

TEST(InstructionDecoderTest, UnknownProgramReturnsFalse) {
    Instruction ix;
    ix.programId = "JUP6LkbZbjS1jKKwapdHNy74zcZ3tLUZoi5QNyVTaV4";
    ix.data = "3axL5qdEKYoR";

    EXPECT_FALSE(InstructionDecoder::tryDecode(ix));
}

TEST(InstructionDecoderTest, InvalidBase58ReturnsFalse) {
    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = "0OIl"; // invalid base58 chars

    EXPECT_FALSE(InstructionDecoder::tryDecode(ix));
}

TEST(InstructionDecoderTest, WrongSizeComputeBudgetReturnsFalse) {
    // 3 bytes is too short for any ComputeBudget instruction
    QByteArray raw(3, '\x02');

    Instruction ix;
    ix.programId = SolanaPrograms::ComputeBudget;
    ix.data = Base58::encode(raw);

    EXPECT_FALSE(InstructionDecoder::tryDecode(ix));
}

// ── decodeAll ────────────────────────────────────────────

TEST(InstructionDecoderTest, DecodeAllProcessesAllInstructions) {
    TransactionResponse tx;

    // Unparsed ComputeBudget instruction
    Instruction ix1;
    ix1.programId = SolanaPrograms::ComputeBudget;
    auto txIx1 = ComputeBudgetInstruction::setComputeUnitLimit(300000);
    ix1.data = Base58::encode(txIx1.data);

    // Unparsed ComputeBudget instruction
    Instruction ix2;
    ix2.programId = SolanaPrograms::ComputeBudget;
    auto txIx2 = ComputeBudgetInstruction::setComputeUnitPrice(100);
    ix2.data = Base58::encode(txIx2.data);

    // Already-parsed instruction (should not be modified)
    Instruction ix3;
    ix3.programId = SolanaPrograms::SystemProgram;
    ix3.program = "system";
    ix3.type = "transfer";
    ix3.info = QJsonObject{{"lamports", 500}};

    tx.message.instructions = {ix1, ix2, ix3};

    InstructionDecoder::decodeAll(tx);

    EXPECT_TRUE(tx.message.instructions[0].isParsed());
    EXPECT_EQ(tx.message.instructions[0].type, "setComputeUnitLimit");
    EXPECT_EQ(tx.message.instructions[0].info["units"].toInteger(), 300000);

    EXPECT_TRUE(tx.message.instructions[1].isParsed());
    EXPECT_EQ(tx.message.instructions[1].type, "setComputeUnitPrice");
    EXPECT_EQ(tx.message.instructions[1].info["microLamports"].toInteger(), 100);

    // Already-parsed remains unchanged
    EXPECT_EQ(tx.message.instructions[2].type, "transfer");
    EXPECT_EQ(tx.message.instructions[2].info["lamports"].toInteger(), 500);
}

TEST(InstructionDecoderTest, DecodeAllProcessesInnerInstructions) {
    TransactionResponse tx;

    // One top-level parsed instruction
    Instruction topIx;
    topIx.programId = SolanaPrograms::TokenProgram;
    topIx.program = "spl-token";
    topIx.type = "transferChecked";
    tx.message.instructions = {topIx};

    // One inner unparsed System Transfer
    Instruction innerIx;
    innerIx.programId = SolanaPrograms::SystemProgram;
    QByteArray raw(12, '\0');
    quint32 disc = 2;
    quint64 lamports = 5000;
    qToLittleEndian(disc, reinterpret_cast<uchar*>(raw.data()));
    qToLittleEndian(lamports, reinterpret_cast<uchar*>(raw.data() + 4));
    innerIx.data = Base58::encode(raw);

    InnerInstructionSet innerSet;
    innerSet.index = 0;
    innerSet.instructions = {innerIx};
    tx.meta.innerInstructions = {innerSet};

    InstructionDecoder::decodeAll(tx);

    EXPECT_TRUE(tx.meta.innerInstructions[0].instructions[0].isParsed());
    EXPECT_EQ(tx.meta.innerInstructions[0].instructions[0].type, "transfer");
    EXPECT_EQ(tx.meta.innerInstructions[0].instructions[0].info["lamports"].toInteger(), 5000);
}

// ── CU Breakdown (TxParseUtils::parseCuBreakdown) ────────

TEST(CuBreakdownTest, RealTransactionLogs) {
    // Actual logs from tx 4Wv6ngC3aMZp...
    // ComputeBudget and System don't emit "consumed" lines,
    // only Token does (6199). Total is 6499.
    QStringList logs = {
        "Program ComputeBudget111111111111111111111111111111 invoke [1]",
        "Program ComputeBudget111111111111111111111111111111 success",
        "Program 11111111111111111111111111111111 invoke [1]",
        "Program 11111111111111111111111111111111 success",
        "Program TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA invoke [1]",
        "Program log: Instruction: TransferChecked",
        "Program TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA consumed 6199 of 205700 compute units",
        "Program TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA success"};

    auto entries = TxParseUtils::parseCuBreakdown(logs, 6499);

    // Should find 3 instructions (not just the one with "consumed")
    ASSERT_EQ(entries.size(), 3);

    // ComputeBudget: (6499 - 6199) / 2 = 150
    EXPECT_EQ(entries[0].programId, "ComputeBudget111111111111111111111111111111");
    EXPECT_EQ(entries[0].units, 150);

    // System: 150
    EXPECT_EQ(entries[1].programId, "11111111111111111111111111111111");
    EXPECT_EQ(entries[1].units, 150);

    // Token: 6199 (explicitly logged)
    EXPECT_EQ(entries[2].programId, "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
    EXPECT_EQ(entries[2].units, 6199);
}

TEST(CuBreakdownTest, AllInstructionsHaveConsumedLogs) {
    QStringList logs = {
        "Program 11111111111111111111111111111111 invoke [1]",
        "Program 11111111111111111111111111111111 consumed 300 of 200000 compute units",
        "Program 11111111111111111111111111111111 success",
        "Program TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA invoke [1]",
        "Program TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA consumed 6199 of 199700 compute units",
        "Program TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA success"};

    auto entries = TxParseUtils::parseCuBreakdown(logs, 6499);

    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].units, 300);
    EXPECT_EQ(entries[1].units, 6199);
}

TEST(CuBreakdownTest, EmptyLogsReturnsEmpty) {
    QStringList logs;
    auto entries = TxParseUtils::parseCuBreakdown(logs, 1000);
    EXPECT_TRUE(entries.isEmpty());
}
