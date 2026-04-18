#include "tx/TransactionExecutor.h"
#include "tx/TransactionInstruction.h"

#include <QCoreApplication>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── Helper ──────────────────────────────────────────────────

static TransactionInstruction makeDummyInstruction() {
    TransactionInstruction ix;
    ix.programId = "11111111111111111111111111111111";
    ix.data = QByteArray(4, '\0');
    return ix;
}

static TransactionExecutionBuildInput makeValidInput() {
    TransactionExecutionBuildInput input;
    input.feePayer = "So11111111111111111111111111111111111111112";
    input.blockhash = "GHtXQBtBRnh1JKboZQePfV7seWmDp2vqBo5eczXoTiF4";
    input.instructions = {makeDummyInstruction()};
    // api, context, signer are left null — buildExecutionRequest validates them
    return input;
}

// ── classifySigningFailure ──────────────────────────────────

TEST(ClassifySigningFailure, BuildMessage) {
    TransactionSigningFailure f{TransactionSigningFailureKind::BuildMessage, ""};
    EXPECT_EQ(classifySigningFailure(f), TransactionSigningErrorCategory::Build);
}

TEST(ClassifySigningFailure, BuildSigned) {
    TransactionSigningFailure f{TransactionSigningFailureKind::BuildSigned, ""};
    EXPECT_EQ(classifySigningFailure(f), TransactionSigningErrorCategory::Build);
}

TEST(ClassifySigningFailure, ExtraSignatures) {
    TransactionSigningFailure f{TransactionSigningFailureKind::ExtraSignatures, ""};
    EXPECT_EQ(classifySigningFailure(f), TransactionSigningErrorCategory::Build);
}

TEST(ClassifySigningFailure, MissingSigner) {
    TransactionSigningFailure f{TransactionSigningFailureKind::MissingSigner, ""};
    EXPECT_EQ(classifySigningFailure(f), TransactionSigningErrorCategory::MissingSigner);
}

TEST(ClassifySigningFailure, PrimarySignature) {
    TransactionSigningFailure f{TransactionSigningFailureKind::PrimarySignature, ""};
    EXPECT_EQ(classifySigningFailure(f), TransactionSigningErrorCategory::Signing);
}

// ── signingFailureMessage ───────────────────────────────────

TEST(SigningFailureMessage, BuildWithoutDetail) {
    TransactionSigningFailure f{TransactionSigningFailureKind::BuildMessage, ""};
    EXPECT_EQ(signingFailureMessage(f), "Error building transaction.");
}

TEST(SigningFailureMessage, BuildWithDetail) {
    TransactionSigningFailure f{TransactionSigningFailureKind::BuildSigned, "bad data"};
    EXPECT_EQ(signingFailureMessage(f), "Error building transaction: bad data");
}

TEST(SigningFailureMessage, MissingSignerIgnoresDetail) {
    TransactionSigningFailure f{TransactionSigningFailureKind::MissingSigner, "irrelevant"};
    EXPECT_EQ(signingFailureMessage(f), "Wallet not available for signing.");
}

TEST(SigningFailureMessage, SigningWithoutDetail) {
    TransactionSigningFailure f{TransactionSigningFailureKind::PrimarySignature, ""};
    EXPECT_EQ(signingFailureMessage(f), "Signing failed.");
}

TEST(SigningFailureMessage, SigningWithDetail) {
    TransactionSigningFailure f{TransactionSigningFailureKind::PrimarySignature, "User rejected"};
    EXPECT_EQ(signingFailureMessage(f), "User rejected");
}

// ── buildExecutionRequest — error paths ─────────────────────

TEST(BuildExecutionRequest, NullOutputPointers) {
    auto input = makeValidInput();
    EXPECT_FALSE(buildExecutionRequest(input, nullptr, nullptr));
}

TEST(BuildExecutionRequest, NullApi) {
    auto input = makeValidInput();
    // signer and context also null — hits the first validation
    TransactionExecutionRequest req;
    QString error;
    EXPECT_FALSE(buildExecutionRequest(input, &req, &error));
    EXPECT_EQ(error, "invalid_execution_context");
}

TEST(BuildExecutionRequest, EmptyFeePayer) {
    auto input = makeValidInput();
    input.feePayer.clear();
    TransactionExecutionRequest req;
    QString error;
    EXPECT_FALSE(buildExecutionRequest(input, &req, &error));
    EXPECT_EQ(error, "invalid_execution_context");
}

TEST(BuildExecutionRequest, NoInstructions) {
    auto input = makeValidInput();
    input.instructions.clear();
    // Need valid api/context/signer pointers to pass the first check
    QObject ctx;
    input.api = reinterpret_cast<SolanaApi*>(1); // non-null sentinel
    input.context = &ctx;
    input.signer = reinterpret_cast<Signer*>(1);
    TransactionExecutionRequest req;
    QString error;
    EXPECT_FALSE(buildExecutionRequest(input, &req, &error));
    EXPECT_EQ(error, "no_instructions");
}

TEST(BuildExecutionRequest, MissingBlockhashNonNonce) {
    auto input = makeValidInput();
    input.blockhash.clear();
    QObject ctx;
    input.api = reinterpret_cast<SolanaApi*>(1);
    input.context = &ctx;
    input.signer = reinterpret_cast<Signer*>(1);
    TransactionExecutionRequest req;
    QString error;
    EXPECT_FALSE(buildExecutionRequest(input, &req, &error));
    EXPECT_EQ(error, "missing_blockhash");
}

TEST(BuildExecutionRequest, InvalidNonceContext) {
    auto input = makeValidInput();
    input.useNonce = true;
    input.nonceAddress = "addr";
    // nonceAuthority and nonceValue empty — triggers invalid_nonce_context
    QObject ctx;
    input.api = reinterpret_cast<SolanaApi*>(1);
    input.context = &ctx;
    input.signer = reinterpret_cast<Signer*>(1);
    TransactionExecutionRequest req;
    QString error;
    EXPECT_FALSE(buildExecutionRequest(input, &req, &error));
    EXPECT_EQ(error, "invalid_nonce_context");
}

// ── buildExecutionRequest — happy paths ─────────────────────

TEST(BuildExecutionRequest, SuccessWithBlockhash) {
    auto input = makeValidInput();
    QObject ctx;
    input.api = reinterpret_cast<SolanaApi*>(1);
    input.context = &ctx;
    input.signer = reinterpret_cast<Signer*>(1);
    TransactionExecutionRequest req;
    QString error;
    EXPECT_TRUE(buildExecutionRequest(input, &req, &error));
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(req.api, input.api);
    EXPECT_EQ(req.context, input.context);
}

TEST(BuildExecutionRequest, SuccessWithNonce) {
    auto input = makeValidInput();
    input.useNonce = true;
    input.nonceAddress = "NonceAddr1111111111111111111111111111111111";
    input.nonceAuthority = "AuthAddr11111111111111111111111111111111111";
    input.nonceValue = "NonceVal11111111111111111111111111111111111";
    input.blockhash.clear(); // not needed in nonce mode
    QObject ctx;
    input.api = reinterpret_cast<SolanaApi*>(1);
    input.context = &ctx;
    input.signer = reinterpret_cast<Signer*>(1);
    TransactionExecutionRequest req;
    QString error;
    EXPECT_TRUE(buildExecutionRequest(input, &req, &error));
    EXPECT_TRUE(error.isEmpty());
}

TEST(BuildExecutionRequest, DefaultTimeoutApplied) {
    auto input = makeValidInput();
    QObject ctx;
    input.api = reinterpret_cast<SolanaApi*>(1);
    input.context = &ctx;
    input.signer = reinterpret_cast<Signer*>(1);
    input.submitTimeoutMs = 0; // should get default
    TransactionExecutionRequest req;
    QString error;
    ASSERT_TRUE(buildExecutionRequest(input, &req, &error));
    EXPECT_GT(req.submitTimeoutMs, 0);
}
