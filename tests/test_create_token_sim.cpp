#include "crypto/Keypair.h"
#include "features/sendreceive/SendReceiveHandler.h"
#include "models/SendReceive.h"
#include "services/SolanaApi.h"
#include "services/model/SimulationResponse.h"
#include "tx/Base58.h"
#include "tx/ProgramIds.h"
#include "tx/Token2022Instruction.h"
#include "tx/TokenEncodingUtils.h"
#include "tx/TokenInstruction.h"
#include "tx/TokenOperationBuilder.h"
#include "tx/TransactionBuilder.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── Helper: wait for signal with timeout ─────────────────────

template <typename Signal> bool waitForSignal(QObject* obj, Signal sig, int timeoutMs = 15000) {
    QEventLoop loop;
    bool received = false;
    QObject::connect(obj, sig, &loop, [&]() {
        received = true;
        loop.quit();
    });
    QTimer::singleShot(timeoutMs, &loop, [&]() { loop.quit(); });
    loop.exec();
    return received;
}

// ── Account size computation tests (pure, no network) ────────

class MintAccountSizeTest : public ::testing::Test {
  protected:
    SendReceiveHandler handler;
};

TEST_F(MintAccountSizeTest, BasicTokenNoExtensions) {
    // Token-2022 mint with MetadataPointer + TokenMetadata, no optional extensions
    SendReceiveMintSizeInput input{"TestToken", "TST", "", false, false, false, false};
    quint64 size = handler.computeMintAccountSize(input);

    // Expected: 166 (base) + 68 (MetadataPointer TLV) + 4+32+32+4+9+4+3+4+0+4 (TokenMetadata TLV)
    // TokenMetadata data: 32(updateAuth) + 32(mint) + 4+9(name) + 4+3(symbol) + 4+0(uri) +
    // 4(addlKV) = 92 TLV: 4 + 92 = 96 Total: 166 + 68 + 96 = 330
    EXPECT_EQ(size, 330u) << "Basic token with name=TestToken, symbol=TST, uri=empty";
}

TEST_F(MintAccountSizeTest, TokenFromScreenshot) {
    // Match the exact inputs from the user's screenshot
    SendReceiveMintSizeInput input{"token201010", "token20100", "", false, false, false, false};
    quint64 size = handler.computeMintAccountSize(input);

    // name=11 bytes, symbol=10 bytes, uri=0 bytes
    // TokenMetadata data: 32+32+4+11+4+10+4+0+4 = 101
    // TLV: 4 + 101 = 105
    // Total: 166 + 68 + 105 = 339
    EXPECT_EQ(size, 339u) << "Token with name=token201010, symbol=token20100";

    // Verify rent
    quint64 expectedRent = (size + 128) * 6960; // Solana rent formula
    EXPECT_EQ(expectedRent, 3250320u) << "Rent should be 0.00325032 SOL (3250320 lamports)";
}

TEST_F(MintAccountSizeTest, TokenWithAllExtensions) {
    SendReceiveMintSizeInput input{"MyToken", "MTK", "https://example.com/meta.json", true, true,
                                   true,      true};
    quint64 size = handler.computeMintAccountSize(input);

    // Base: 166
    // MetadataPointer: 4 + 64 = 68
    // TokenMetadata: 4 + 32 + 32 + 4+7 + 4+3 + 4+29 + 4 = 4 + 119 = 123
    // TransferFee: 4 + 108 = 112
    // NonTransferable: 4 + 0 = 4
    // MintClose: 4 + 32 = 36
    // PermanentDelegate: 4 + 32 = 36
    // Total: 166 + 68 + 123 + 112 + 4 + 36 + 36 = 545
    EXPECT_EQ(size, 545u);
}

// ── Instruction encoding size tests ──────────────────────────

TEST(InstructionEncodingTest, InitializeMint2_NoFreezeAuth) {
    auto ix = TokenInstruction::initializeMint2("11111111111111111111111111111111", 9,
                                                "11111111111111111111111111111111", QString(),
                                                SolanaPrograms::Token2022Program);

    // Expected: 1(index) + 1(decimals) + 32(mintAuth) + 1(None tag) = 35
    EXPECT_EQ(ix.data.size(), 35) << "InitializeMint2 without freeze authority should be 35 bytes";
    EXPECT_EQ(static_cast<quint8>(ix.data[0]), 0x14) << "Instruction index should be 20 (0x14)";
    EXPECT_EQ(static_cast<quint8>(ix.data[34]), 0x00) << "Option tag = None (1 byte)";
}

TEST(InstructionEncodingTest, InitializeMint2_WithFreezeAuth) {
    auto ix = TokenInstruction::initializeMint2(
        "11111111111111111111111111111111", 9, "11111111111111111111111111111111",
        "11111111111111111111111111111111", SolanaPrograms::Token2022Program);

    // Expected: 1(index) + 1(decimals) + 32(mintAuth) + 1(Some tag) + 32(freezeAuth) = 67
    EXPECT_EQ(ix.data.size(), 67) << "InitializeMint2 with freeze authority should be 67 bytes";

    // Check option tag is 1 byte: [0x01] (matching SPL Token's pack_pubkey_option)
    EXPECT_EQ(static_cast<quint8>(ix.data[34]), 0x01) << "Option tag = Some (1 byte)";
    // Freeze authority starts at byte 35 (not 38)
    EXPECT_EQ(ix.data.mid(35, 32), ix.data.mid(2, 32))
        << "Freeze auth == mint auth (both system program)";
}

TEST(InstructionEncodingTest, SetAuthority_WithNewAuth) {
    auto ix = TokenInstruction::setAuthority("11111111111111111111111111111111",
                                             "11111111111111111111111111111111",
                                             0, // authorityType
                                             "11111111111111111111111111111111");

    // Expected: 1(index) + 1(authType) + 1(Some tag) + 32(pubkey) = 35
    EXPECT_EQ(ix.data.size(), 35) << "SetAuthority with new authority should be 35 bytes";
}

TEST(InstructionEncodingTest, SetAuthority_RevokeAuth) {
    auto ix = TokenInstruction::setAuthority("11111111111111111111111111111111",
                                             "11111111111111111111111111111111",
                                             0); // authorityType, no new authority → None

    // Expected: 1(index) + 1(authType) + 1(None tag) = 3
    EXPECT_EQ(ix.data.size(), 3) << "SetAuthority revoking should be 3 bytes";
}

TEST(InstructionEncodingTest, MintCloseAuthority) {
    auto ix = Token2022Instruction::initializeMintCloseAuthority(
        "11111111111111111111111111111111", "11111111111111111111111111111111");

    // Expected: 1(index) + 1(Some tag) + 32(pubkey) = 34
    EXPECT_EQ(ix.data.size(), 34) << "MintCloseAuthority with authority should be 34 bytes";
}

TEST(InstructionEncodingTest, TransferFeeConfig) {
    auto ix = Token2022Instruction::TransferFee::initializeTransferFeeConfig(
        "11111111111111111111111111111111", "11111111111111111111111111111111",
        "11111111111111111111111111111111", 100, 1000000);

    // Expected: 1(index) + 1(sub) + 32(configAuth OptionalNonZero) + 32(withdrawAuth
    // OptionalNonZero)
    //         + 2(bps) + 8(maxFee) = 76
    EXPECT_EQ(ix.data.size(), 76) << "TransferFeeConfig with both authorities should be 76 bytes";
}

// ── Compiled transaction structure tests (no network) ────────

class CreateTokenStructureTest : public ::testing::Test {
  protected:
    void SetUp() override {
        walletKeypair = Keypair::generate();
        walletAddress = walletKeypair.address();
        mintKeypair = Keypair::generate();
    }

    Keypair walletKeypair;
    QString walletAddress;
    Keypair mintKeypair;
};

TEST_F(CreateTokenStructureTest, WithInitialSupply_InstructionStructure) {
    // Match the live app scenario: name=tookokok, symbol=yokokok, decimals=9,
    // freeze auth=enabled, supply=10000000 (> 0), no optional extensions
    SendReceiveHandler handler;
    SendReceiveMintSizeInput sizeInput{"tookokok", "yokokok", "", false, false, false, false};
    quint64 accountSize = handler.computeMintAccountSize(sizeInput);
    quint64 rentLamports = (accountSize + 128) * 6960;

    CreateTokenInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mintAddress = mintKeypair.address();
    input.name = "tookokok";
    input.symbol = "yokokok";
    input.uri = "";
    input.freezeAuthority = walletAddress;
    input.decimals = 9;
    input.rawSupply = 10000000ULL * 1000000000ULL; // 10M tokens * 10^9 decimals
    input.hasTransferFee = false;
    input.hasNonTransferable = false;
    input.hasMintClose = false;
    input.hasPermanentDelegate = false;
    input.mintAccountSize = accountSize;
    input.rentLamports = rentLamports;

    auto result = TokenOperationBuilder::buildCreateToken(input);
    ASSERT_TRUE(result.ok) << "buildCreateToken failed: " << result.error.toStdString();

    // With supply > 0, should have 6 instructions:
    // 0: createAccount, 1: MetadataPointer, 2: initializeMint2,
    // 3: TokenMetadata, 4: createIdempotent ATA, 5: mintTo
    ASSERT_EQ(result.instructions.size(), 6) << "Token with supply should have 6 instructions";

    // Verify instruction data sizes
    EXPECT_EQ(result.instructions[0].data.size(), 52) << "createAccount: 4+8+8+32";
    EXPECT_EQ(result.instructions[1].data.size(), 66) << "MetadataPointer: 2+32+32";
    EXPECT_EQ(result.instructions[2].data.size(), 67)
        << "initializeMint2 with freeze auth: 1+1+32+(1+32)=67";
    // TokenMetadata: disc(8) + name(4+8) + symbol(4+7) + uri(4+0) = 35
    EXPECT_EQ(result.instructions[3].data.size(), 35) << "TokenMetadata init";
    EXPECT_EQ(result.instructions[4].data.size(), 1) << "createIdempotent ATA";
    EXPECT_EQ(result.instructions[5].data.size(), 9) << "mintTo: 1+8";

    // Verify instruction account counts
    EXPECT_EQ(result.instructions[0].accounts.size(), 2) << "createAccount: from + newAccount";
    EXPECT_EQ(result.instructions[1].accounts.size(), 1) << "MetadataPointer: mint";
    EXPECT_EQ(result.instructions[2].accounts.size(), 1) << "initializeMint2: mint";
    EXPECT_EQ(result.instructions[3].accounts.size(), 4)
        << "TokenMetadata: metadata, updateAuth, mint, mintAuth";
    EXPECT_EQ(result.instructions[4].accounts.size(), 6)
        << "createIdempotent ATA: payer, ata, owner, mint, system, token";
    EXPECT_EQ(result.instructions[5].accounts.size(), 3) << "mintTo: mint, dest, auth";

    // Verify program IDs
    EXPECT_EQ(result.instructions[0].programId, SolanaPrograms::SystemProgram);
    EXPECT_EQ(result.instructions[1].programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(result.instructions[2].programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(result.instructions[3].programId, SolanaPrograms::Token2022Program);
    EXPECT_EQ(result.instructions[4].programId, SolanaPrograms::AssociatedTokenAccount);
    EXPECT_EQ(result.instructions[5].programId, SolanaPrograms::Token2022Program);

    // Verify initializeMint2 data format (instruction 2)
    const QByteArray& mint2Data = result.instructions[2].data;
    EXPECT_EQ(static_cast<quint8>(mint2Data[0]), 0x14) << "Instruction index 20";
    EXPECT_EQ(static_cast<quint8>(mint2Data[1]), 9) << "Decimals = 9";
    // Bytes 2-33: mint authority (wallet pubkey)
    QByteArray walletPubkeyRaw = Base58::decode(walletAddress);
    EXPECT_EQ(mint2Data.mid(2, 32), walletPubkeyRaw) << "Mint authority should be wallet";
    // Byte 34: 1-byte option tag = Some (matches SPL Token pack_pubkey_option)
    EXPECT_EQ(static_cast<quint8>(mint2Data[34]), 0x01) << "Option::Some tag (1 byte)";
    // Bytes 35-66: freeze authority (wallet pubkey)
    EXPECT_EQ(mint2Data.mid(35, 32), walletPubkeyRaw) << "Freeze authority should be wallet";

    // Verify createAccount data (instruction 0): space should be fixed-extensions-only size.
    // Token-2022 InitializeMint2 rejects accounts with trailing zeros after declared TLV
    // extensions. TokenMetadata is auto-realloc'd, so createAccount.space excludes it.
    const QByteArray& createData = result.instructions[0].data;
    // Data: [u32 index=0][u64 lamports][u64 space][32 owner]
    quint64 encodedSpace =
        qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(createData.data() + 12));
    using namespace Token2022AccountSize;
    quint64 expectedCreateSpace = kMintBaseWithExtensions + kTlvHeaderLen + kMetadataPointerDataLen;
    EXPECT_EQ(encodedSpace, expectedCreateSpace)
        << "createAccount space must be fixed-extensions-only (excludes TokenMetadata)";

    // Verify createAccount owner is Token2022Program
    QByteArray encodedOwner = createData.mid(20, 32);
    EXPECT_EQ(encodedOwner, Base58::decode(SolanaPrograms::Token2022Program))
        << "createAccount owner must be Token2022Program";

    qDebug() << "=== 6-instruction transaction structure verified ===";
    qDebug() << "Account size:" << accountSize << "Rent:" << rentLamports;
    for (int i = 0; i < result.instructions.size(); ++i) {
        const auto& ix = result.instructions[i];
        qDebug() << "  Ix" << i << "program:" << ix.programId.left(12) << "..."
                 << "data:" << ix.data.size() << "accounts:" << ix.accounts.size();
    }
}

TEST_F(CreateTokenStructureTest, WithInitialSupply_CompiledMessage) {
    // Build the same transaction and verify compiled message structure
    SendReceiveHandler handler;
    SendReceiveMintSizeInput sizeInput{"tookokok", "yokokok", "", false, false, false, false};
    quint64 accountSize = handler.computeMintAccountSize(sizeInput);
    quint64 rentLamports = (accountSize + 128) * 6960;

    CreateTokenInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mintAddress = mintKeypair.address();
    input.name = "tookokok";
    input.symbol = "yokokok";
    input.uri = "";
    input.freezeAuthority = walletAddress;
    input.decimals = 9;
    input.rawSupply = 10000000ULL * 1000000000ULL;
    input.hasTransferFee = false;
    input.hasNonTransferable = false;
    input.hasMintClose = false;
    input.hasPermanentDelegate = false;
    input.mintAccountSize = accountSize;
    input.rentLamports = rentLamports;

    auto result = TokenOperationBuilder::buildCreateToken(input);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 6);

    // Build transaction
    TransactionBuilder txBuilder;
    txBuilder.setVersion(TransactionBuilder::Version::Legacy);
    txBuilder.setFeePayer(walletAddress);
    txBuilder.setRecentBlockhash("11111111111111111111111111111111");

    for (const auto& ix : result.instructions) {
        txBuilder.addInstruction(ix);
    }

    QByteArray message = txBuilder.serializeMessage();
    ASSERT_FALSE(message.isEmpty()) << txBuilder.lastError().toStdString();

    // Parse the compiled message header
    const auto* msg = reinterpret_cast<const uint8_t*>(message.constData());
    int offset = 0;

    // Header
    EXPECT_EQ(msg[offset], 2) << "numRequiredSignatures should be 2 (wallet + mint)";
    offset++;
    EXPECT_EQ(msg[offset], 0) << "numReadonlySignedAccounts should be 0";
    offset++;
    EXPECT_EQ(msg[offset], 3) << "numReadonlyUnsignedAccounts should be 3 (System, ATA, Token2022)";
    offset++;

    // Account count (compact u16) — expect 6 accounts
    EXPECT_EQ(msg[offset], 6) << "Should have 6 accounts";
    offset++;

    // Read account keys
    QByteArray feePayerKey = message.mid(offset, 32);
    EXPECT_EQ(feePayerKey, Base58::decode(walletAddress))
        << "Account 0 should be wallet (fee payer)";
    offset += 32;

    QByteArray mintKey = message.mid(offset, 32);
    EXPECT_EQ(mintKey, Base58::decode(mintKeypair.address())) << "Account 1 should be mint";
    offset += 32;

    // Account 2 should be the ATA (derived, writable non-signer)
    offset += 32; // skip ATA key (we'll verify it below)

    // Accounts 3-5: readonly non-signers (SystemProgram, ATAProgram, Token2022Program in alpha
    // order)
    QByteArray acc3 = message.mid(offset, 32);
    offset += 32;
    QByteArray acc4 = message.mid(offset, 32);
    offset += 32;
    QByteArray acc5 = message.mid(offset, 32);
    offset += 32;

    EXPECT_EQ(acc3, Base58::decode(SolanaPrograms::SystemProgram))
        << "Account 3 should be SystemProgram";
    EXPECT_EQ(acc4, Base58::decode(SolanaPrograms::AssociatedTokenAccount))
        << "Account 4 should be ATAProgram";
    EXPECT_EQ(acc5, Base58::decode(SolanaPrograms::Token2022Program))
        << "Account 5 should be Token2022Program";

    // Verify number of required signatures
    int numSigners = txBuilder.numRequiredSignatures();
    EXPECT_EQ(numSigners, 2) << "Need wallet + mint signatures";

    // Sign and build full transaction
    QByteArray walletSig = walletKeypair.sign(message);
    QByteArray mintSig = mintKeypair.sign(message);
    ASSERT_EQ(walletSig.size(), 64);
    ASSERT_EQ(mintSig.size(), 64);

    QByteArray serializedTx = txBuilder.buildSigned({walletSig, mintSig});
    ASSERT_FALSE(serializedTx.isEmpty()) << txBuilder.lastError().toStdString();

    // Dump base64 for external inspection (e.g., Solana Explorer TX inspector)
    qDebug() << "=== Compiled message base64 ===";
    qDebug().noquote() << message.toBase64();
    qDebug() << "=== Signed transaction base64 ===";
    qDebug().noquote() << serializedTx.toBase64();
    qDebug() << "Transaction size:" << serializedTx.size() << "bytes";
}

// ── Live simulation test (requires network) ──────────────────

class CreateTokenSimulationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        api = new SolanaApi();
        walletKeypair = Keypair::generate();
        walletAddress = walletKeypair.address();
        mintKeypair = Keypair::generate();
    }
    void TearDown() override { delete api; }

    SolanaApi* api = nullptr;
    Keypair walletKeypair;
    QString walletAddress;
    Keypair mintKeypair;
};

TEST_F(CreateTokenSimulationTest, SimulateWithSupply_VerifyNoStructuralError) {
    // Submit the 6-instruction transaction (with supply > 0) for simulation.
    // Wallet has no SOL, so instruction 0 will fail — but it should fail with
    // "insufficient lamports" or "AccountNotFound", NOT "invalid account data".
    // This verifies the transaction structure is accepted by the RPC.
    SendReceiveHandler handler;
    SendReceiveMintSizeInput sizeInput{"tookokok", "yokokok", "", false, false, false, false};
    quint64 accountSize = handler.computeMintAccountSize(sizeInput);
    quint64 rentLamports = (accountSize + 128) * 6960;

    CreateTokenInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mintAddress = mintKeypair.address();
    input.name = "tookokok";
    input.symbol = "yokokok";
    input.uri = "";
    input.freezeAuthority = walletAddress;
    input.decimals = 9;
    input.rawSupply = 10000000ULL * 1000000000ULL;
    input.hasTransferFee = false;
    input.hasNonTransferable = false;
    input.hasMintClose = false;
    input.hasPermanentDelegate = false;
    input.mintAccountSize = accountSize;
    input.rentLamports = rentLamports;

    auto result = TokenOperationBuilder::buildCreateToken(input);
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 6);

    TransactionBuilder txBuilder;
    txBuilder.setVersion(TransactionBuilder::Version::Legacy);
    txBuilder.setFeePayer(walletAddress);
    txBuilder.setRecentBlockhash("11111111111111111111111111111111");

    for (const auto& ix : result.instructions) {
        txBuilder.addInstruction(ix);
    }

    QByteArray message = txBuilder.serializeMessage();
    QByteArray walletSig = walletKeypair.sign(message);
    QByteArray mintSig = mintKeypair.sign(message);
    QByteArray serializedTx = txBuilder.buildSigned({walletSig, mintSig});
    ASSERT_FALSE(serializedTx.isEmpty());

    QEventLoop loop;
    bool gotResult = false;
    QString simError;
    QStringList simLogs;

    QObject::connect(api, &SolanaApi::simulationReady, &loop, [&](const SimulationResponse& sim) {
        gotResult = true;
        if (!sim.success) {
            simError = sim.err.toVariant().toString();
            if (simError.isEmpty()) {
                QJsonDocument doc;
                doc.setObject(sim.err.toObject());
                simError = doc.toJson(QJsonDocument::Compact);
            }
        }
        simLogs = sim.logs;
        loop.quit();
    });
    QObject::connect(api, &SolanaApi::requestFailed, &loop,
                     [&](const QString& method, const QString& error) {
                         if (method == "simulateTransaction") {
                             gotResult = true;
                             simError = "RPC error: " + error;
                             loop.quit();
                         }
                     });
    QTimer::singleShot(15000, &loop, [&]() { loop.quit(); });

    api->simulateTransaction(serializedTx, false, true);
    loop.exec();

    if (gotResult) {
        qDebug() << "\n=== 6-ix Simulation Result ===";
        qDebug() << "Error:" << simError;
        for (const auto& log : simLogs) {
            qDebug().noquote() << "  " << log;
        }

        // Structural errors would mention "invalid account data" even at instruction 0.
        // Execution errors (wallet has no SOL) should mention "insufficient lamports" or
        // "AccountNotFound", which is fine — it means the transaction structure was accepted.
        if (!simError.isEmpty()) {
            EXPECT_FALSE(simError.contains("invalid account data"))
                << "Transaction structure should be valid even if wallet has no SOL";
        }
    } else {
        qDebug() << "6-ix simulation timed out (15s)";
    }
}

TEST_F(CreateTokenSimulationTest, SimulateBasicTokenCreation) {
    // Compute account size
    SendReceiveHandler handler;
    SendReceiveMintSizeInput sizeInput{"TestToken", "TST", "", false, false, false, false};
    quint64 accountSize = handler.computeMintAccountSize(sizeInput);

    // Get rent for this size
    quint64 rentLamports = (accountSize + 128) * 6960; // approximate

    // Build create token instructions
    CreateTokenInstructionBuildInput input;
    input.walletAddress = walletAddress;
    input.mintAddress = mintKeypair.address();
    input.name = "TestToken";
    input.symbol = "TST";
    input.uri = "";
    input.freezeAuthority = walletAddress; // freeze authority enabled
    input.decimals = 9;
    input.rawSupply = 0; // no initial mint
    input.hasTransferFee = false;
    input.hasNonTransferable = false;
    input.hasMintClose = false;
    input.hasPermanentDelegate = false;
    input.mintAccountSize = accountSize;
    input.rentLamports = rentLamports;

    auto result = TokenOperationBuilder::buildCreateToken(input);
    ASSERT_TRUE(result.ok) << "buildCreateToken failed: " << result.error.toStdString();

    qDebug() << "=== Create Token Simulation ===";
    qDebug() << "Wallet:" << walletAddress;
    qDebug() << "Mint:" << mintKeypair.address();
    qDebug() << "Account size:" << accountSize << "bytes";
    qDebug() << "Rent:" << rentLamports << "lamports";
    qDebug() << "Instructions:" << result.instructions.size();

    for (int i = 0; i < result.instructions.size(); ++i) {
        const auto& ix = result.instructions[i];
        qDebug() << "  Instruction" << i << "program:" << ix.programId
                 << "data_size:" << ix.data.size() << "data_hex:" << ix.data.toHex().left(40)
                 << "accounts:" << ix.accounts.size();
    }

    // Verify instruction data sizes are correct
    ASSERT_EQ(result.instructions.size(), 4) << "Basic token should have 4 instructions";
    EXPECT_EQ(result.instructions[0].data.size(), 52) << "createAccount: 4+8+8+32";
    EXPECT_EQ(result.instructions[1].data.size(), 66) << "MetadataPointer: 2+32+32";
    EXPECT_EQ(result.instructions[2].data.size(), 67)
        << "initializeMint2 with freeze auth: 1+1+32+(1+32)=67";
    EXPECT_EQ(result.instructions[3].data.size(), 32) << "TokenMetadata init: 8+4+9+4+3+4+0=32";

    // Verify initializeMint2 data format
    const QByteArray& mint2Data = result.instructions[2].data;
    EXPECT_EQ(static_cast<quint8>(mint2Data[0]), 0x14) << "Instruction index 20";
    EXPECT_EQ(static_cast<quint8>(mint2Data[1]), 9) << "Decimals = 9";
    // Bytes 2-33: mint authority (32 bytes raw pubkey)
    // Byte 34: 1-byte option tag = Some (SPL Token pack_pubkey_option format)
    EXPECT_EQ(static_cast<quint8>(mint2Data[34]), 0x01) << "Option tag = Some (1 byte)";
    // Bytes 35-66: freeze authority (32 bytes raw pubkey)
    QByteArray walletPubkeyRaw2 = Base58::decode(walletAddress);
    EXPECT_EQ(mint2Data.mid(35, 32), walletPubkeyRaw2) << "Freeze authority should be wallet";

    // Build and sign transaction properly
    TransactionBuilder txBuilder;
    txBuilder.setVersion(TransactionBuilder::Version::Legacy);
    txBuilder.setFeePayer(walletAddress);
    txBuilder.setRecentBlockhash("11111111111111111111111111111111"); // dummy, replaced by RPC

    for (const auto& ix : result.instructions) {
        txBuilder.addInstruction(ix);
    }

    // Sign with both keypairs
    QByteArray message = txBuilder.serializeMessage();
    ASSERT_FALSE(message.isEmpty()) << "Message serialization failed";

    int numSigners = txBuilder.numRequiredSignatures();
    EXPECT_EQ(numSigners, 2) << "Need wallet + mint signatures";

    // Wallet is first (fee payer), mint is second
    QByteArray walletSig = walletKeypair.sign(message);
    QByteArray mintSig = mintKeypair.sign(message);
    ASSERT_EQ(walletSig.size(), 64);
    ASSERT_EQ(mintSig.size(), 64);

    QByteArray serializedTx = txBuilder.buildSigned({walletSig, mintSig});
    ASSERT_FALSE(serializedTx.isEmpty())
        << "Transaction serialization failed: " << txBuilder.lastError().toStdString();

    qDebug() << "Serialized tx size:" << serializedTx.size() << "bytes";
    qDebug() << "Required signers:" << numSigners;

    // Output base64 for curl simulation
    QString base64Tx = serializedTx.toBase64();
    qDebug() << "\n=== Base64 (submit with sigVerify:false, replaceRecentBlockhash:true) ===";
    qDebug().noquote() << base64Tx;

    // Submit for simulation via SolanaApi
    QEventLoop loop;
    bool gotResult = false;
    QString simError;
    QStringList simLogs;

    QObject::connect(api, &SolanaApi::simulationReady, &loop, [&](const SimulationResponse& sim) {
        gotResult = true;
        if (!sim.success) {
            simError = sim.err.toVariant().toString();
            if (simError.isEmpty()) {
                // err might be a JSON object, stringify it
                QJsonDocument doc;
                doc.setObject(sim.err.toObject());
                simError = doc.toJson(QJsonDocument::Compact);
            }
        }
        simLogs = sim.logs;
        loop.quit();
    });
    QObject::connect(api, &SolanaApi::requestFailed, &loop,
                     [&](const QString& method, const QString& error) {
                         if (method == "simulateTransaction") {
                             gotResult = true;
                             simError = "RPC error: " + error;
                             loop.quit();
                         }
                     });

    QTimer::singleShot(15000, &loop, [&]() { loop.quit(); });

    api->simulateTransaction(serializedTx, false, true);
    loop.exec();

    if (gotResult) {
        qDebug() << "\n=== Simulation Result ===";
        if (simError.isEmpty()) {
            qDebug() << "SUCCESS! Transaction simulation passed.";
        } else {
            qDebug() << "Error:" << simError;
        }
        for (const auto& log : simLogs) {
            qDebug().noquote() << "  " << log;
        }

        // The wallet has no SOL, so we expect "insufficient lamports" not "invalid account data"
        if (!simError.isEmpty()) {
            EXPECT_FALSE(simError.contains("invalid account data"))
                << "Instruction encoding should be valid even if wallet has no SOL";
        }
    } else {
        qDebug() << "Simulation timed out (15s)";
    }
}
