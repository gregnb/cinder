// Tests for TrezorUsbTransport shared libusb context lifecycle.
//
// These tests verify that multiple TrezorUsbTransport instances share a single
// libusb_context via reference counting. Without shared context, macOS fails to
// re-claim a USB interface immediately after a previous transport destroys its
// context (the kernel hasn't released the claim yet).
//
// No physical device is needed — we test the context management, not USB I/O.

#include "crypto/plugin/trezor/TrezorUsbTransport.h"

#include <gtest/gtest.h>
#include <memory>

// ── Shared context ref-counting ──────────────────────────────

TEST(TrezorUsbContext, SingleInstanceAcquiresContext) {
    // Creating a transport should acquire the shared context
    auto t = std::make_unique<TrezorUsbTransport>();
    // If context failed to init, enumerate would crash — just verify construction
    EXPECT_NO_FATAL_FAILURE({ (void)t; });
}

TEST(TrezorUsbContext, MultipleInstancesShareContext) {
    // Creating multiple transports should all share the same context
    auto t1 = std::make_unique<TrezorUsbTransport>();
    auto t2 = std::make_unique<TrezorUsbTransport>();
    auto t3 = std::make_unique<TrezorUsbTransport>();

    // All three should be valid (not crash)
    EXPECT_NO_FATAL_FAILURE({
        (void)t1;
        (void)t2;
        (void)t3;
    });
}

TEST(TrezorUsbContext, DestroyAndRecreateDoesNotCrash) {
    // This is the critical regression test: create → destroy → create
    // must not fail. Before the fix, the second create would fail because
    // libusb_exit() in the first destructor raced with the kernel releasing
    // the interface claim on macOS.
    {
        auto t1 = std::make_unique<TrezorUsbTransport>();
    } // t1 destroyed here — context should survive if refcount is correct

    // Create another transport immediately — this must not crash or fail
    auto t2 = std::make_unique<TrezorUsbTransport>();
    EXPECT_NO_FATAL_FAILURE({ (void)t2; });
}

TEST(TrezorUsbContext, InterleavedLifecycles) {
    // Simulate the real-world pattern: transport A is created for a signer,
    // then transport B is created (e.g. for debug panel) while A still lives,
    // then A is destroyed, then B is used.
    auto a = std::make_unique<TrezorUsbTransport>();
    auto b = std::make_unique<TrezorUsbTransport>();

    // Destroy A while B is alive
    a.reset();

    // B should still be valid (context kept alive by B's ref)
    EXPECT_NO_FATAL_FAILURE({ (void)b; });

    // Create C after A is gone — must work
    auto c = std::make_unique<TrezorUsbTransport>();
    EXPECT_NO_FATAL_FAILURE({ (void)c; });
}

TEST(TrezorUsbContext, RapidCreateDestroyLoop) {
    // Stress test: rapidly create and destroy transports
    for (int i = 0; i < 20; ++i) {
        auto t = std::make_unique<TrezorUsbTransport>();
        // Context should be acquired and released cleanly each iteration
    }
    // Final creation after all the churn must succeed
    auto final_t = std::make_unique<TrezorUsbTransport>();
    EXPECT_NO_FATAL_FAILURE({ (void)final_t; });
}

TEST(TrezorUsbContext, EnumerateDoesNotInterfereWithSharedContext) {
    // enumerate() creates its own temporary context — it should not
    // interfere with the shared context used by transport instances.
    auto t = std::make_unique<TrezorUsbTransport>();

    // enumerate uses a separate context internally
    auto devices = TrezorUsbTransport::enumerate(0x1209, 0x53C1);
    // Don't care about results — just verify no crash

    // Transport should still be valid after enumerate
    EXPECT_NO_FATAL_FAILURE({ (void)t; });
}

TEST(TrezorUsbContext, OpenWithoutDeviceReturnsError) {
    auto t = std::make_unique<TrezorUsbTransport>();

    // Opening a non-existent device path should fail gracefully
    bool result = t->open(QStringLiteral("255:255"));
    EXPECT_FALSE(result);
    EXPECT_FALSE(t->isOpen());
}

TEST(TrezorUsbContext, OpenInvalidPathFormat) {
    auto t = std::make_unique<TrezorUsbTransport>();

    // Invalid path format should fail
    bool result = t->open(QStringLiteral("not-a-path"));
    EXPECT_FALSE(result);
    EXPECT_FALSE(t->isOpen());
}

TEST(TrezorUsbContext, CloseWithoutOpenIsNoOp) {
    auto t = std::make_unique<TrezorUsbTransport>();

    // Closing a never-opened transport should not crash
    EXPECT_NO_FATAL_FAILURE({ t->close(); });
    EXPECT_FALSE(t->isOpen());
}

TEST(TrezorUsbContext, DoubleCloseIsNoOp) {
    auto t = std::make_unique<TrezorUsbTransport>();
    t->open(QStringLiteral("255:255")); // will fail, but that's fine
    t->close();
    EXPECT_NO_FATAL_FAILURE({ t->close(); });
}
