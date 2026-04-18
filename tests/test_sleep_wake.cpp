#include "services/PriceService.h"
#include "services/SolanaApi.h"
#include <QCoreApplication>
#include <gtest/gtest.h>

static QCoreApplication* app = nullptr;

int main(int argc, char** argv) {
    QCoreApplication a(argc, argv);
    app = &a;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ── SolanaApi queue flush ────────────────────────────────────

class SolanaApiFlushTest : public ::testing::Test {
  protected:
    void SetUp() override { api = new SolanaApi(); }
    void TearDown() override { delete api; }
    SolanaApi* api = nullptr;
};

TEST_F(SolanaApiFlushTest, InitialQueueIsEmpty) { EXPECT_EQ(api->totalQueueSize(), 0); }

TEST_F(SolanaApiFlushTest, FlushOnEmptyQueueIsNoOp) {
    api->flushQueues();
    EXPECT_EQ(api->totalQueueSize(), 0);
}

TEST_F(SolanaApiFlushTest, EnqueueThenFlush) {
    // Enqueue several requests (they won't actually send — no event loop drain)
    api->fetchBalance("11111111111111111111111111111111");
    api->fetchBlockHeight();
    api->fetchVersion();
    // Process events briefly to let enqueue run
    QCoreApplication::processEvents();

    // Queue should have items (some may have already been dispatched, but
    // totalQueueSize only counts pending, not in-flight)
    int before = api->totalQueueSize();
    api->flushQueues();
    EXPECT_EQ(api->totalQueueSize(), 0);
    // Flush should have cleared at least something (or was already 0 if all dispatched)
    EXPECT_LE(api->totalQueueSize(), before);
}

TEST_F(SolanaApiFlushTest, FlushThenEnqueueStillWorks) {
    api->flushQueues();
    api->fetchBalance("11111111111111111111111111111111");
    QCoreApplication::processEvents();
    // Should not crash — api remains functional after flush
}

// ── PriceService pause/resume/flush ──────────────────────────

class PriceServicePauseTest : public ::testing::Test {
  protected:
    void SetUp() override { price = new PriceService(); }
    void TearDown() override { delete price; }
    PriceService* price = nullptr;
};

TEST_F(PriceServicePauseTest, InitiallyNotPaused) { EXPECT_FALSE(price->isPaused()); }

TEST_F(PriceServicePauseTest, PauseSetsFlag) {
    price->pause();
    EXPECT_TRUE(price->isPaused());
}

TEST_F(PriceServicePauseTest, ResumeUnsetsFlag) {
    price->pause();
    EXPECT_TRUE(price->isPaused());
    price->resume();
    EXPECT_FALSE(price->isPaused());
}

TEST_F(PriceServicePauseTest, DoublePauseIsIdempotent) {
    price->pause();
    price->pause();
    EXPECT_TRUE(price->isPaused());
    price->resume();
    EXPECT_FALSE(price->isPaused());
}

TEST_F(PriceServicePauseTest, DoubleResumeIsIdempotent) {
    price->pause();
    price->resume();
    price->resume();
    EXPECT_FALSE(price->isPaused());
}

TEST_F(PriceServicePauseTest, FlushQueueClearsAll) {
    // Enqueue some requests then flush
    price->fetchPrices({"solana", "bitcoin", "ethereum"});
    QCoreApplication::processEvents();
    price->flushQueue();
    EXPECT_EQ(price->queueSize(), 0);
}

TEST_F(PriceServicePauseTest, FlushEmptyQueueIsNoOp) {
    EXPECT_EQ(price->queueSize(), 0);
    price->flushQueue();
    EXPECT_EQ(price->queueSize(), 0);
}

TEST_F(PriceServicePauseTest, PauseFlushResumeCycle) {
    // Simulate full sleep/wake cycle
    price->fetchPrices({"solana"});
    QCoreApplication::processEvents();

    price->pause();
    EXPECT_TRUE(price->isPaused());

    price->flushQueue();
    EXPECT_EQ(price->queueSize(), 0);

    price->resume();
    EXPECT_FALSE(price->isPaused());
    EXPECT_EQ(price->queueSize(), 0);
}
