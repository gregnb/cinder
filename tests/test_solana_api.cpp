#include "services/SolanaApi.h"
#include "services/model/PriorityFee.h"
#include "services/model/SignatureInfo.h"
#include "services/model/SimulationResponse.h"
#include "services/model/TransactionResponse.h"
#include "tx/NonceAccount.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <gtest/gtest.h>
#include <memory>
#include <optional>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;

    qRegisterMetaType<QList<SignatureInfo>>("QList<SignatureInfo>");
    qRegisterMetaType<TransactionResponse>("TransactionResponse");
    qRegisterMetaType<QList<PriorityFee>>("QList<PriorityFee>");
    qRegisterMetaType<NonceAccount>("NonceAccount");
    qRegisterMetaType<SimulationResponse>("SimulationResponse");

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

static bool waitForSignal(QSignalSpy& spy, int timeoutMs = 10000) {
    if (spy.count() > 0) {
        return true;
    }
    return spy.wait(timeoutMs);
}

static bool waitForEither(QSignalSpy& a, QSignalSpy& b, int timeoutMs = 10000) {
    if (a.count() > 0 || b.count() > 0) {
        return true;
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if (a.count() > 0 || b.count() > 0) {
            return true;
        }
    }
    return false;
}

static QJsonObject mockTransactionResult() {
    QJsonObject meta;
    meta["fee"] = 5000;
    meta["computeUnitsConsumed"] = 2000;
    meta["err"] = QJsonValue();
    meta["preBalances"] = QJsonArray{2000000000LL, 0};
    meta["postBalances"] = QJsonArray{1999995000LL, 5000};
    meta["preTokenBalances"] = QJsonArray{};
    meta["postTokenBalances"] = QJsonArray{};
    meta["innerInstructions"] = QJsonArray{};
    meta["logMessages"] = QJsonArray{"Program log: ok"};

    QJsonObject payerKey;
    payerKey["pubkey"] = "payer1111111111111111111111111111111111111";
    payerKey["signer"] = true;
    payerKey["writable"] = true;
    payerKey["source"] = "transaction";

    QJsonObject destKey;
    destKey["pubkey"] = "dest11111111111111111111111111111111111111";
    destKey["signer"] = false;
    destKey["writable"] = true;
    destKey["source"] = "transaction";

    QJsonObject parsedInfo;
    parsedInfo["lamports"] = 5000;
    parsedInfo["source"] = "payer1111111111111111111111111111111111111";
    parsedInfo["destination"] = "dest11111111111111111111111111111111111111";

    QJsonObject parsed;
    parsed["type"] = "transfer";
    parsed["info"] = parsedInfo;

    QJsonObject ix;
    ix["programId"] = "11111111111111111111111111111111";
    ix["program"] = "system";
    ix["parsed"] = parsed;

    QJsonObject message;
    message["recentBlockhash"] = "H3tFMV2G5x8fsY5YUUw1QyW2Nqk5o3n8N7QWf4fFz9yQ";
    message["accountKeys"] = QJsonArray{payerKey, destKey};
    message["instructions"] = QJsonArray{ix};

    QJsonObject txObj;
    txObj["signatures"] = QJsonArray{"mock-signature"};
    txObj["message"] = message;

    QJsonObject result;
    result["slot"] = 123456;
    result["blockTime"] = 1700001000;
    result["version"] = "legacy";
    result["meta"] = meta;
    result["transaction"] = txObj;

    return QJsonObject{{"result", result}};
}

static QJsonArray mockTokenAccountsFor(const QString& owner, const QString& programId) {
    if (owner == "11111111111111111111111111111112") {
        return QJsonArray{};
    }
    return QJsonArray{QJsonObject{
        {"pubkey",
         programId == "TokenzQdBNbLqP5VEhdkAS6EPFLC1PHnBqCXEpPxuEb" ? "acct-t22" : "acct-legacy"},
        {"account", QJsonObject{{"owner", programId}}}}};
}

static std::optional<QJsonObject> mockRpcResponse(const QString& method, const QJsonArray& params) {
    if (method == "getSignaturesForAddress") {
        return QJsonObject{
            {"result",
             QJsonArray{
                 QJsonObject{{"signature", "sig-1"}, {"slot", 1001}, {"blockTime", 1700000001}},
                 QJsonObject{{"signature", "sig-2"}, {"slot", 1000}, {"blockTime", 1700000000}},
                 QJsonObject{{"signature", "sig-3"}, {"slot", 999}, {"blockTime", 1699999999}}}}};
    }
    if (method == "getTransaction") {
        return mockTransactionResult();
    }
    if (method == "getTokenAccountsByOwner") {
        const QString owner = params.at(0).toString();
        const QString programId = params.at(1).toObject().value("programId").toString();
        return QJsonObject{
            {"result", QJsonObject{{"value", mockTokenAccountsFor(owner, programId)}}}};
    }
    if (method == "getRecentPrioritizationFees") {
        return QJsonObject{
            {"result", QJsonArray{QJsonObject{{"slot", 999999}, {"prioritizationFee", 0}},
                                  QJsonObject{{"slot", 1000000}, {"prioritizationFee", 2500}}}}};
    }
    if (method == "getMinimumBalanceForRentExemption") {
        return QJsonObject{{"result", 1461600}};
    }
    if (method == "getLatestBlockhash") {
        return QJsonObject{
            {"result",
             QJsonObject{{"value",
                          QJsonObject{{"blockhash", "8do8W3V6k7K7CFH1yQpJmKbrf8oJQ8p8GVi7ccKqcnQG"},
                                      {"lastValidBlockHeight", 123456789}}}}}};
    }
    if (method == "sendTransaction" || method == "simulateTransaction") {
        return QJsonObject{
            {"error", QJsonObject{{"code", -32002}, {"message", "Transaction simulation failed"}}}};
    }
    return std::nullopt;
}

class SolanaApiTest : public ::testing::Test {
  protected:
    std::unique_ptr<SolanaApi> api;
    const QString WALLET = "Efs2KuHv6UtRakD2xjyvfXCwZFTdfpXrFkUYnQSjh3ro";
    const QString KNOWN_SIG =
        "56SnGbj7NLngcgCvQ2ktEEjCa3pZ5rdXtLSbadpsmkZwP6VPrHxjt2eSdQdFiHgiibzU6XuEvzEtoWdwJzfqKdCc";

    void SetUp() override {
        api = std::make_unique<SolanaApi>("http://mock.invalid");
        api->setRpcInterceptorForTests(mockRpcResponse);
        api->setMethodLimitForTests("getTokenAccountsByOwner", 2);
    }

    void TearDown() override { api.reset(); }
};

TEST_F(SolanaApiTest, FetchSignatures) {
    QSignalSpy readySpy(api.get(), &SolanaApi::signaturesReady);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->fetchSignatures(WALLET, 3);

    ASSERT_TRUE(waitForSignal(readySpy));
    ASSERT_EQ(errorSpy.count(), 0);

    auto args = readySpy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), WALLET);

    auto sigs = args.at(1).value<QList<SignatureInfo>>();
    EXPECT_EQ(sigs.size(), 3);

    const SignatureInfo& first = sigs[0];
    EXPECT_FALSE(first.signature.isEmpty());
    EXPECT_GT(first.slot, 0);
    EXPECT_GT(first.blockTime, 0);
}

TEST_F(SolanaApiTest, FetchTransaction) {
    QSignalSpy readySpy(api.get(), &SolanaApi::transactionReady);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->fetchTransaction(KNOWN_SIG);

    ASSERT_TRUE(waitForSignal(readySpy));
    ASSERT_EQ(errorSpy.count(), 0);

    auto args = readySpy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), KNOWN_SIG);

    auto tx = args.at(1).value<TransactionResponse>();
    EXPECT_GT(tx.slot, 0);
    EXPECT_GT(tx.blockTime, 0);
    EXPECT_FALSE(tx.signatures.isEmpty());
    EXPECT_FALSE(tx.message.accountKeys.isEmpty());
    EXPECT_FALSE(tx.message.instructions.isEmpty());
}

TEST_F(SolanaApiTest, FetchTokenAccountsByOwnerMergesBothPrograms) {
    QSignalSpy readySpy(api.get(), &SolanaApi::tokenAccountsReady);

    api->fetchTokenAccountsByOwner(WALLET);

    ASSERT_TRUE(waitForSignal(readySpy));

    auto args = readySpy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), WALLET);

    QJsonArray accounts = args.at(1).toJsonArray();
    EXPECT_EQ(readySpy.count(), 0) << "tokenAccountsReady should fire exactly once (merged)";

    static const QString TOKEN = "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
    static const QString TOKEN2022 = "TokenzQdBNbLqP5VEhdkAS6EPFLC1PHnBqCXEpPxuEb";

    for (int i = 0; i < accounts.size(); ++i) {
        QJsonObject acct = accounts[i].toObject();
        EXPECT_FALSE(acct["pubkey"].toString().isEmpty()) << "account " << i << " missing pubkey";

        QString owner = acct["account"].toObject()["owner"].toString();
        EXPECT_TRUE(owner == TOKEN || owner == TOKEN2022)
            << "account " << i << " has unexpected owner: " << owner.toStdString();
    }
}

TEST_F(SolanaApiTest, FetchTokenAccountsEmitsOnceEvenForEmptyWallet) {
    QSignalSpy readySpy(api.get(), &SolanaApi::tokenAccountsReady);

    api->fetchTokenAccountsByOwner("11111111111111111111111111111112");

    ASSERT_TRUE(waitForSignal(readySpy));

    auto args = readySpy.takeFirst();
    QJsonArray accounts = args.at(1).toJsonArray();
    EXPECT_EQ(accounts.size(), 0);
    EXPECT_EQ(readySpy.count(), 0);
}

TEST_F(SolanaApiTest, FetchRecentPrioritizationFees) {
    QSignalSpy readySpy(api.get(), &SolanaApi::prioritizationFeesReady);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->fetchRecentPrioritizationFees();

    ASSERT_TRUE(waitForSignal(readySpy));
    ASSERT_EQ(errorSpy.count(), 0);

    auto fees = readySpy.takeFirst().at(0).value<QList<PriorityFee>>();
    EXPECT_GT(fees.size(), 0);

    const PriorityFee& first = fees[0];
    EXPECT_GT(first.slot, 0);
}

TEST_F(SolanaApiTest, FetchMinimumBalanceForRentExemption) {
    QSignalSpy readySpy(api.get(), &SolanaApi::minimumBalanceReady);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->fetchMinimumBalanceForRentExemption(80);

    ASSERT_TRUE(waitForSignal(readySpy));
    ASSERT_EQ(errorSpy.count(), 0);

    quint64 lamports = readySpy.takeFirst().at(0).value<quint64>();
    EXPECT_GT(lamports, 0);
    EXPECT_LT(lamports, 100'000'000);
}

TEST_F(SolanaApiTest, FetchLatestBlockhash) {
    QSignalSpy readySpy(api.get(), &SolanaApi::latestBlockhashReady);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->fetchLatestBlockhash();

    ASSERT_TRUE(waitForSignal(readySpy));
    ASSERT_EQ(errorSpy.count(), 0);

    auto args = readySpy.takeFirst();
    QString blockhash = args.at(0).toString();
    quint64 lastValidBlockHeight = args.at(1).value<quint64>();

    EXPECT_FALSE(blockhash.isEmpty());
    EXPECT_GT(blockhash.length(), 30);
    EXPECT_GT(lastValidBlockHeight, 0);
}

TEST_F(SolanaApiTest, SendInvalidTransactionFails) {
    QSignalSpy sentSpy(api.get(), &SolanaApi::transactionSent);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->sendTransaction(QByteArray(64, '\0'));

    ASSERT_TRUE(waitForEither(sentSpy, errorSpy));
    EXPECT_EQ(sentSpy.count(), 0);
    EXPECT_GT(errorSpy.count(), 0);

    auto args = errorSpy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), "sendTransaction");
    EXPECT_FALSE(args.at(1).toString().isEmpty());
}

TEST_F(SolanaApiTest, SimulateInvalidTransactionFails) {
    QSignalSpy simSpy(api.get(), &SolanaApi::simulationReady);
    QSignalSpy errorSpy(api.get(), &SolanaApi::requestFailed);

    api->simulateTransaction(QByteArray(64, '\0'));

    ASSERT_TRUE(waitForEither(simSpy, errorSpy));

    if (errorSpy.count() > 0) {
        auto args = errorSpy.takeFirst();
        EXPECT_EQ(args.at(0).toString(), "simulateTransaction");
        EXPECT_FALSE(args.at(1).toString().isEmpty());
    } else {
        auto sim = simSpy.takeFirst().at(0).value<SimulationResponse>();
        EXPECT_FALSE(sim.success);
    }
}
