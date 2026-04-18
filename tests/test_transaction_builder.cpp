#include "tx/AssociatedTokenInstruction.h"
#include "tx/Base58.h"
#include "tx/CompactU16.h"
#include "tx/ComputeBudgetInstruction.h"
#include "tx/ProgramIds.h"
#include "tx/SystemInstruction.h"
#include "tx/TokenInstruction.h"
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

// ── Base58 Tests ─────────────────────────────────────────

TEST(Base58Test, SystemProgramDecodesToZeros) {
    QByteArray decoded = Base58::decode("11111111111111111111111111111111");
    ASSERT_EQ(decoded.size(), 32);
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(static_cast<uchar>(decoded[i]), 0);
    }
}

TEST(Base58Test, RoundTrip) {
    // Known pubkey
    QString original = "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
    QByteArray decoded = Base58::decode(original);
    ASSERT_EQ(decoded.size(), 32);
    QString reencoded = Base58::encode(decoded);
    EXPECT_EQ(reencoded, original);
}

TEST(Base58Test, InvalidCharReturnsEmpty) {
    QByteArray result = Base58::decode("0OIl"); // 0, O, I, l not in base58
    EXPECT_TRUE(result.isEmpty());
}

// ── CompactU16 Tests ─────────────────────────────────────

TEST(CompactU16Test, EncodeSingleByte) {
    QByteArray out;
    CompactU16::encode(0, out);
    ASSERT_EQ(out.size(), 1);
    EXPECT_EQ(static_cast<uchar>(out[0]), 0);

    out.clear();
    CompactU16::encode(127, out);
    ASSERT_EQ(out.size(), 1);
    EXPECT_EQ(static_cast<uchar>(out[0]), 127);
}

TEST(CompactU16Test, EncodeTwoBytes) {
    QByteArray out;
    CompactU16::encode(128, out);
    ASSERT_EQ(out.size(), 2);

    out.clear();
    CompactU16::encode(16383, out);
    ASSERT_EQ(out.size(), 2);
}

TEST(CompactU16Test, EncodeThreeBytes) {
    QByteArray out;
    CompactU16::encode(16384, out);
    ASSERT_EQ(out.size(), 3);

    out.clear();
    CompactU16::encode(65535, out);
    ASSERT_EQ(out.size(), 3);
}

TEST(CompactU16Test, RoundTrip) {
    uint16_t values[] = {0, 1, 127, 128, 255, 16383, 16384, 65535};
    for (uint16_t val : values) {
        QByteArray buf;
        CompactU16::encode(val, buf);
        int offset = 0;
        int decoded = CompactU16::decode(buf, offset);
        EXPECT_EQ(decoded, val) << "Failed for value " << val;
        EXPECT_EQ(offset, buf.size());
    }
}

// ── TransactionBuilder Tests ─────────────────────────────

// Use deterministic fake pubkeys for testing
static const QString PAYER = "11111111111111111111111111111112";
static const QString RECIPIENT = "11111111111111111111111111111113";
// Use a fake blockhash (any 32-byte-decodable base58 string)
static const QString BLOCKHASH = "11111111111111111111111111111114";

TEST(TransactionBuilderTest, MissingFeePayerFails) {
    TransactionBuilder builder;
    builder.setRecentBlockhash(BLOCKHASH).addInstruction(
        SystemInstruction::transfer(PAYER, RECIPIENT, 1000));
    QByteArray msg = builder.serializeMessage();
    EXPECT_TRUE(msg.isEmpty());
    EXPECT_FALSE(builder.lastError().isEmpty());
}

TEST(TransactionBuilderTest, MissingBlockhashFails) {
    TransactionBuilder builder;
    builder.setFeePayer(PAYER).addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 1000));
    QByteArray msg = builder.serializeMessage();
    EXPECT_TRUE(msg.isEmpty());
}

TEST(TransactionBuilderTest, NoInstructionsFails) {
    TransactionBuilder builder;
    builder.setFeePayer(PAYER).setRecentBlockhash(BLOCKHASH);
    QByteArray msg = builder.serializeMessage();
    EXPECT_TRUE(msg.isEmpty());
}

TEST(TransactionBuilderTest, SolTransferSerializes) {
    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(PAYER)
                         .setRecentBlockhash(BLOCKHASH)
                         .addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 1000000000))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();

    // Legacy message starts with header byte (numRequiredSignatures)
    // PAYER is the only signer
    EXPECT_EQ(static_cast<uchar>(msg[0]), 1); // numRequiredSignatures
    EXPECT_EQ(static_cast<uchar>(msg[1]), 0); // numReadonlySignedAccounts
    // readonlyUnsigned: System Program is readonly non-signer
    EXPECT_EQ(static_cast<uchar>(msg[2]), 1); // numReadonlyUnsignedAccounts

    // Verify total accounts = 3 (PAYER, RECIPIENT, SystemProgram)
    int offset = 3;
    int numKeys = CompactU16::decode(msg, offset);
    EXPECT_EQ(numKeys, 3);
}

TEST(TransactionBuilderTest, AccountDeduplication) {
    // PAYER appears in fee payer AND as transfer source (signer+writable)
    // It should appear only once in the compiled account list
    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(PAYER)
                         .setRecentBlockhash(BLOCKHASH)
                         .addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 100))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty());

    int offset = 3;
    int numKeys = CompactU16::decode(msg, offset);
    EXPECT_EQ(numKeys, 3); // PAYER, RECIPIENT, SystemProgram — no duplicates
}

TEST(TransactionBuilderTest, FeePayerIsFirstAccount) {
    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(PAYER)
                         .setRecentBlockhash(BLOCKHASH)
                         .addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 100))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty());

    // Skip header (3 bytes) + compact-u16 key count (1 byte for small count)
    int offset = 3;
    CompactU16::decode(msg, offset);

    // First 32 bytes after the key count should be the fee payer's decoded key
    QByteArray expectedPayer = Base58::decode(PAYER);
    QByteArray actualFirst = msg.mid(offset, 32);
    EXPECT_EQ(actualFirst, expectedPayer);
}

TEST(TransactionBuilderTest, V0MessageStartsWithPrefix) {
    TransactionBuilder builder;
    QByteArray msg = builder.setVersion(TransactionBuilder::Version::V0)
                         .setFeePayer(PAYER)
                         .setRecentBlockhash(BLOCKHASH)
                         .addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 100))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty());
    EXPECT_EQ(static_cast<uchar>(msg[0]), 0x80); // V0 prefix

    // Legacy header follows after the prefix
    EXPECT_EQ(static_cast<uchar>(msg[1]), 1); // numRequiredSignatures
}

TEST(TransactionBuilderTest, UseNoncePrependsAdvanceInstruction) {
    QString noncePubkey = "11111111111111111111111111111115";
    QString authority = "11111111111111111111111111111116";
    QString nonceValue = BLOCKHASH; // any valid 32-byte base58

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(PAYER)
                         .useNonce(noncePubkey, authority, nonceValue)
                         .addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 100))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();

    // Should have 2 signers: PAYER (fee payer) and authority (nonce authority)
    EXPECT_EQ(static_cast<uchar>(msg[0]), 2); // numRequiredSignatures
}

TEST(TransactionBuilderTest, BuildSignedProducesCorrectFormat) {
    TransactionBuilder builder;
    builder.setFeePayer(PAYER).setRecentBlockhash(BLOCKHASH).addInstruction(
        SystemInstruction::transfer(PAYER, RECIPIENT, 100));

    QByteArray msgBytes = builder.serializeMessage();
    ASSERT_FALSE(msgBytes.isEmpty());

    EXPECT_EQ(builder.numRequiredSignatures(), 1);

    // Create a fake 64-byte signature
    QByteArray fakeSig(64, '\xAB');
    QByteArray fullTx = builder.buildSigned({fakeSig});

    ASSERT_FALSE(fullTx.isEmpty()) << builder.lastError().toStdString();

    // First byte: compact-u16 signature count = 1
    EXPECT_EQ(static_cast<uchar>(fullTx[0]), 1);

    // Next 64 bytes: the signature
    EXPECT_EQ(fullTx.mid(1, 64), fakeSig);

    // Remaining bytes: the message
    EXPECT_EQ(fullTx.mid(65), msgBytes);
}

TEST(TransactionBuilderTest, BuildSignedWrongCountFails) {
    TransactionBuilder builder;
    builder.setFeePayer(PAYER).setRecentBlockhash(BLOCKHASH).addInstruction(
        SystemInstruction::transfer(PAYER, RECIPIENT, 100));

    // Pass wrong number of signatures
    QByteArray result = builder.buildSigned({});
    EXPECT_TRUE(result.isEmpty());

    // Pass wrong size signature
    result = builder.buildSigned({QByteArray(32, '\0')});
    EXPECT_TRUE(result.isEmpty());
}

// ── Instruction Factory Tests ────────────────────────────

TEST(InstructionTest, ComputeBudgetUnitLimit) {
    auto ix = ComputeBudgetInstruction::setComputeUnitLimit(200000);
    EXPECT_EQ(ix.programId, SolanaPrograms::ComputeBudget);
    EXPECT_TRUE(ix.accounts.isEmpty());
    EXPECT_EQ(ix.data.size(), 5); // 1 discriminator + 4 u32
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x02);
}

TEST(InstructionTest, ComputeBudgetUnitPrice) {
    auto ix = ComputeBudgetInstruction::setComputeUnitPrice(50000);
    EXPECT_EQ(ix.programId, SolanaPrograms::ComputeBudget);
    EXPECT_TRUE(ix.accounts.isEmpty());
    EXPECT_EQ(ix.data.size(), 9); // 1 discriminator + 8 u64
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x03);
}

TEST(InstructionTest, TokenTransferChecked) {
    auto ix = TokenInstruction::transferChecked("source", "mint", "dest", "owner", 1000000, 6,
                                                SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(ix.accounts.size(), 4);
    EXPECT_EQ(ix.data.size(), 10); // 1 disc + 8 u64 + 1 decimals
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x0C);
    EXPECT_EQ(static_cast<uchar>(ix.data[9]), 6); // decimals
}

TEST(InstructionTest, AssociatedTokenCreateIdempotent) {
    auto ix = AssociatedTokenInstruction::createIdempotent("payer", "ata", "owner", "mint");
    EXPECT_EQ(ix.programId, SolanaPrograms::AssociatedTokenAccount);
    EXPECT_EQ(ix.accounts.size(), 6);
    EXPECT_EQ(ix.data.size(), 1);
    EXPECT_EQ(static_cast<uchar>(ix.data[0]), 0x01);
}

TEST(InstructionTest, SystemCreateNonceAccountReturnsTwoInstructions) {
    auto ixs = SystemInstruction::createNonceAccount(PAYER, "nonce", "authority", 1500000);
    EXPECT_EQ(ixs.size(), 2);
    // First: createAccount, second: nonceInitialize
    EXPECT_EQ(ixs[0].programId, SolanaPrograms::SystemProgram);
    EXPECT_EQ(ixs[1].programId, SolanaPrograms::SystemProgram);
}

// ── Multi-instruction Transaction ────────────────────────

TEST(TransactionBuilderTest, MultipleInstructionsWithPriorityFees) {
    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(PAYER)
                         .setRecentBlockhash(BLOCKHASH)
                         .addInstruction(ComputeBudgetInstruction::setComputeUnitLimit(200000))
                         .addInstruction(ComputeBudgetInstruction::setComputeUnitPrice(50000))
                         .addInstruction(SystemInstruction::transfer(PAYER, RECIPIENT, 1000000000))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();

    // Parse header
    EXPECT_EQ(static_cast<uchar>(msg[0]), 1); // only PAYER signs

    // Count keys: PAYER, RECIPIENT, SystemProgram, ComputeBudget = 4
    int offset = 3;
    int numKeys = CompactU16::decode(msg, offset);
    EXPECT_EQ(numKeys, 4);

    // Skip keys (4 * 32 = 128) + blockhash (32) = 160 bytes
    offset += numKeys * 32 + 32;

    // Instruction count should be 3
    int numIx = CompactU16::decode(msg, offset);
    EXPECT_EQ(numIx, 3);
}

// ══════════════════════════════════════════════════════════
// Byte-for-byte Reference Tests
//
// These tests verify that our C++ TransactionBuilder produces
// byte-identical output to @solana/web3.js v1 (the canonical
// Solana SDK). Reference payloads generated by:
//   node tests/generate_reference_payloads.mjs
// ══════════════════════════════════════════════════════════

// Real pubkeys used consistently across all reference tests
static const QString REF_PAYER = "BrEi3xFm1bQ3K1yGoCbykP3yzGL42MjcxZ9UjGfqKjBo";
static const QString REF_RECIPIENT = "7v91N7iZ9mNicL8WfG6cgSCKyRXydQjLh6UYBWwm6y1Q";
static const QString REF_BLOCKHASH = "EkSnNWid2cvwEVnVx9aBqawnmiCNiDgp3gUdkDPTKN1N";
static const QString REF_MINT = "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v";
static const QString REF_SOURCE_ATA = "3EpVCPUgyjq2MfGeCttyey6bs5zya5wjYZ2BE6yDg6bm";
static const QString REF_DEST_ATA = "J2FLvHxMsRm4c4W3P8MeRZGHGgVMZDCFeCpjtCFJTagt";
static const QString REF_NONCE_KEY = "GNnEBsGMfJCgPkXtwPbJL8nFRjzPEpJGJNmjJA4vi54u";
static const QString REF_NONCE_AUTH = "5ZWj7a1f8tWkjBESHKgrLmXshuXxqeY9SYcfbshpAqPG";

static QByteArray fromHex(const char* hex) { return QByteArray::fromHex(QByteArray(hex)); }

// ── 1. Legacy SOL Transfer ───────────────────────────────

TEST(ReferencePayloadTest, LegacySolTransfer) {
    QByteArray expected = fromHex("01000103"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "66c2f508c9c555cacc9fb26d88e88dd54e210bb5a8bce5687f60d7e75c4cd07f"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01020200010c0200000000ca9a3b00000000");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(REF_PAYER)
            .setRecentBlockhash(REF_BLOCKHASH)
            .addInstruction(SystemInstruction::transfer(REF_PAYER, REF_RECIPIENT, 1000000000))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 2. V0 SOL Transfer ──────────────────────────────────

TEST(ReferencePayloadTest, V0SolTransfer) {
    QByteArray expected = fromHex("80"
                                  "01000103"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "66c2f508c9c555cacc9fb26d88e88dd54e210bb5a8bce5687f60d7e75c4cd07f"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01020200010c0200000000ca9a3b00000000"
                                  "00"); // empty address table lookups

    TransactionBuilder builder;
    QByteArray msg =
        builder.setVersion(TransactionBuilder::Version::V0)
            .setFeePayer(REF_PAYER)
            .setRecentBlockhash(REF_BLOCKHASH)
            .addInstruction(SystemInstruction::transfer(REF_PAYER, REF_RECIPIENT, 1000000000))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size());
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 3. SOL Transfer with Priority Fees ───────────────────

TEST(ReferencePayloadTest, LegacyWithPriorityFees) {
    QByteArray expected = fromHex("01000204"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "66c2f508c9c555cacc9fb26d88e88dd54e210bb5a8bce5687f60d7e75c4cd07f"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "0306466fe5211732ffecadba72c39be7bc8ce5bbc5f7126b2c439b3a40000000"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "03"
                                  "03000502400d0300"
                                  "0300090350c3000000000000"
                                  "020200010c0200000000ca9a3b00000000");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(REF_PAYER)
            .setRecentBlockhash(REF_BLOCKHASH)
            .addInstruction(ComputeBudgetInstruction::setComputeUnitLimit(200000))
            .addInstruction(ComputeBudgetInstruction::setComputeUnitPrice(50000))
            .addInstruction(SystemInstruction::transfer(REF_PAYER, REF_RECIPIENT, 1000000000))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 4. SPL Token TransferChecked ─────────────────────────

TEST(ReferencePayloadTest, SplTokenTransferChecked) {
    QByteArray expected = fromHex("01000205"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "21419dfbe06b0ab41171d012ac511e058f2d036333e003f0c2e6003ac8dea12c"
                                  "fce87a0a8483d1c3c2179ec0777b4ec36a880cdeea4a1d9d8677e97d81428fcd"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1d765a193d9cbe146ceeb79ac1cb485ed5f5b37913a8cf5857eff00a9"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01"
                                  "0404010302000a0c40420f000000000006");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(REF_PAYER)
                         .setRecentBlockhash(REF_BLOCKHASH)
                         .addInstruction(TokenInstruction::transferChecked(
                             REF_SOURCE_ATA, REF_MINT, REF_DEST_ATA, REF_PAYER, 1000000, 6,
                             SolanaPrograms::TokenProgram))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 5. Token-2022 TransferChecked ────────────────────────

TEST(ReferencePayloadTest, Token2022TransferChecked) {
    QByteArray expected = fromHex("01000205"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "21419dfbe06b0ab41171d012ac511e058f2d036333e003f0c2e6003ac8dea12c"
                                  "fce87a0a8483d1c3c2179ec0777b4ec36a880cdeea4a1d9d8677e97d81428fcd"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01"
                                  "0404010302000a0c40420f000000000006");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(REF_PAYER)
                         .setRecentBlockhash(REF_BLOCKHASH)
                         .addInstruction(TokenInstruction::transferChecked(
                             REF_SOURCE_ATA, REF_MINT, REF_DEST_ATA, REF_PAYER, 1000000, 6,
                             SolanaPrograms::Token2022Program))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 6. Create Associated Token Account (Idempotent) ──────

TEST(ReferencePayloadTest, CreateAtaIdempotent) {
    QByteArray expected = fromHex("01000507"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "fce87a0a8483d1c3c2179ec0777b4ec36a880cdeea4a1d9d8677e97d81428fcd"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "66c2f508c9c555cacc9fb26d88e88dd54e210bb5a8bce5687f60d7e75c4cd07f"
                                  "8c97258f4e2489f1bb3d1029148e0d830b5a1399daff1084048e7bd8dbe9f859"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1d765a193d9cbe146ceeb79ac1cb485ed5f5b37913a8cf5857eff00a9"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "01"
                                  "04060001030502060101");

    TransactionBuilder builder;
    QByteArray msg = builder.setFeePayer(REF_PAYER)
                         .setRecentBlockhash(REF_BLOCKHASH)
                         .addInstruction(AssociatedTokenInstruction::createIdempotent(
                             REF_PAYER, REF_DEST_ATA, REF_RECIPIENT, REF_MINT))
                         .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 7. Nonce-based SOL Transfer ──────────────────────────

TEST(ReferencePayloadTest, NonceSolTransfer) {
    QByteArray expected = fromHex("02010206"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "43c2f1eb1c05d1ebf83a962ada472bb1b73e3760d8293b927984df18276d03a3"
                                  "66c2f508c9c555cacc9fb26d88e88dd54e210bb5a8bce5687f60d7e75c4cd07f"
                                  "e473a3bf0fd045b7c7e657aeb6bcf01a1e04e8f9f9c0c3b2d142ccba7fe3b3ca"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "06a7d517192c568ee08a845f73d29788cf035c3145b21ab344d8062ea9400000"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "02"
                                  "04030305010404000000"
                                  "040200020c020000000065cd1d00000000");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(REF_PAYER)
            .useNonce(REF_NONCE_KEY, REF_NONCE_AUTH, REF_BLOCKHASH)
            .addInstruction(SystemInstruction::transfer(REF_PAYER, REF_RECIPIENT, 500000000))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── 8. Create ATA + Token-2022 TransferChecked ───────────

TEST(ReferencePayloadTest, AtaPlusToken2022Transfer) {
    QByteArray expected = fromHex("01000508"
                                  "a131d40df554fe8312a7dc4e5510ca09debb16ca13cc1afd591eebf2baa733aa"
                                  "21419dfbe06b0ab41171d012ac511e058f2d036333e003f0c2e6003ac8dea12c"
                                  "fce87a0a8483d1c3c2179ec0777b4ec36a880cdeea4a1d9d8677e97d81428fcd"
                                  "0000000000000000000000000000000000000000000000000000000000000000"
                                  "66c2f508c9c555cacc9fb26d88e88dd54e210bb5a8bce5687f60d7e75c4cd07f"
                                  "8c97258f4e2489f1bb3d1029148e0d830b5a1399daff1084048e7bd8dbe9f859"
                                  "c6fa7af3bedbad3a3d65f36aabc97431b1bbe4c2d2f6e0e47ca60203452f5d61"
                                  "06ddf6e1ee758fde18425dbce46ccddab61afc4d83b90d27febdf928d8a18bfc"
                                  "cc490e928cd2e3873bb343fc95da33179ca60f4dbf46c2c36e91299d55d4e6b9"
                                  "02"
                                  "050600020406030701010704010602000a0c404b4c000000000006");

    TransactionBuilder builder;
    QByteArray msg =
        builder.setFeePayer(REF_PAYER)
            .setRecentBlockhash(REF_BLOCKHASH)
            .addInstruction(AssociatedTokenInstruction::createIdempotent(
                REF_PAYER, REF_DEST_ATA, REF_RECIPIENT, REF_MINT, SolanaPrograms::Token2022Program))
            .addInstruction(TokenInstruction::transferChecked(REF_SOURCE_ATA, REF_MINT,
                                                              REF_DEST_ATA, REF_PAYER, 5000000, 6,
                                                              SolanaPrograms::Token2022Program))
            .serializeMessage();

    ASSERT_FALSE(msg.isEmpty()) << builder.lastError().toStdString();
    ASSERT_EQ(msg.size(), expected.size())
        << "Size mismatch: got " << msg.size() << ", expected " << expected.size();
    EXPECT_EQ(msg, expected) << "Bytes differ.\n  Got:    " << msg.toHex().toStdString()
                             << "\n  Expect: " << expected.toHex().toStdString();
}

// ── ATA Derivation Tests ─────────────────────────────────────
// Reference values generated with @solana/web3.js PublicKey.findProgramAddress

TEST(AtaDerivation, BugCase_Bump255OnCurve) {
    // This was the bug: bump=255 IS on the ed25519 curve, so the correct ATA
    // uses bump=254. The old code (crypto_sign_ed25519_pk_to_curve25519) wrongly
    // accepted bump=255 because it treats small-order / non-main-subgroup points
    // as "not on curve".
    QString ata = AssociatedTokenInstruction::deriveAddress(
        "893oLrLYW8LfRRVTortkm599nmB3gvJTB6QUwro1XYuT",
        "DitHyRMQiSDhn5cnKMJV2CDDt6sVct96YrECiM49pump", SolanaPrograms::TokenProgram);
    EXPECT_EQ(ata, "EEAjsqFxdEg83Mb2Nc6bQ94hYC1WbuuoAU8hazMLGVek");
}

TEST(AtaDerivation, NormalCase_Bump251) {
    QString ata = AssociatedTokenInstruction::deriveAddress(
        "BDBY9tWqj1Pe4qVJxmhjLYZoALfwSNNEG91HrbgZ73VH",
        "DitHyRMQiSDhn5cnKMJV2CDDt6sVct96YrECiM49pump", SolanaPrograms::TokenProgram);
    EXPECT_EQ(ata, "CcUjFrU72P8SUyQ9xbGTyJvizFeFeFmPoyL6gNZFbVT5");
}

TEST(AtaDerivation, USDC_Bump253) {
    QString ata = AssociatedTokenInstruction::deriveAddress(
        "vines1vzrYbzLMRdu58ou5XTby4qAqVRLmqo36NKPTg",
        "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", SolanaPrograms::TokenProgram);
    EXPECT_EQ(ata, "9jrTvdU2Am3UmgSAX8EKS9wwrSufkWRbZzEukDogqpQg");
}

TEST(AtaDerivation, Token2022_Bump255) {
    QString ata = AssociatedTokenInstruction::deriveAddress(
        "BDBY9tWqj1Pe4qVJxmhjLYZoALfwSNNEG91HrbgZ73VH",
        "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v", SolanaPrograms::Token2022Program);
    EXPECT_EQ(ata, "DVTuY1wg4MshuLmu3tz27HvM812kbGKqUBStFjyKSFJM");
}

TEST(AtaDerivation, SystemProgramOwner_Bump254) {
    QString ata = AssociatedTokenInstruction::deriveAddress(
        "11111111111111111111111111111111", "So11111111111111111111111111111111111111112",
        SolanaPrograms::TokenProgram);
    EXPECT_EQ(ata, "aqxoAhCwpy3oB1BpNw9hL1HdLYLgPpbPjzxDrrQj3Fs");
}

TEST(AtaDerivation, InvalidInputsReturnEmpty) {
    EXPECT_TRUE(AssociatedTokenInstruction::deriveAddress("", "abc", SolanaPrograms::TokenProgram)
                    .isEmpty());
    EXPECT_TRUE(AssociatedTokenInstruction::deriveAddress("abc", "", SolanaPrograms::TokenProgram)
                    .isEmpty());
}
