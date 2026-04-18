#include "tx/KnownTokens.h"
#include <gtest/gtest.h>

// ── WSOL mint should resolve to "WSOL", not "SOL" ──────────────

TEST(KnownTokens, WsolMintResolvesToWsol) {
    KnownToken tk = resolveKnownToken(WSOL_MINT);
    EXPECT_EQ(tk.symbol.toStdString(), "WSOL");
}

TEST(KnownTokens, WsolMintHasSolIcon) {
    KnownToken tk = resolveKnownToken(WSOL_MINT);
    EXPECT_FALSE(tk.iconPath.isEmpty());
    EXPECT_TRUE(tk.iconPath.contains("sol.png"));
}

TEST(KnownTokens, WsolMintConstant) {
    EXPECT_EQ(WSOL_MINT.toStdString(), "So11111111111111111111111111111111111111112");
}

// ── Other well-known tokens ─────────────────────────────────────

TEST(KnownTokens, UsdcResolves) {
    KnownToken tk = resolveKnownToken("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
    EXPECT_EQ(tk.symbol.toStdString(), "USDC");
    EXPECT_FALSE(tk.iconPath.isEmpty());
}

TEST(KnownTokens, UsdtResolves) {
    KnownToken tk = resolveKnownToken("Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB");
    EXPECT_EQ(tk.symbol.toStdString(), "USDT");
    EXPECT_FALSE(tk.iconPath.isEmpty());
}

TEST(KnownTokens, BonkResolves) {
    KnownToken tk = resolveKnownToken("DezXAZ8z7PnrnRJjz3wXBoRgixCa6xjnB7YaB1pPB263");
    EXPECT_EQ(tk.symbol.toStdString(), "BONK");
}

TEST(KnownTokens, JtoResolves) {
    KnownToken tk = resolveKnownToken("jtojtomepa8beP8AuQc6eXt5FriJwfFMwQx2v2f9mCL");
    EXPECT_EQ(tk.symbol.toStdString(), "JTO");
}

TEST(KnownTokens, AtlasResolves) {
    KnownToken tk = resolveKnownToken("ATLASXmbPQxBUYbxPsV97usA3fPQYEqzQBUHgiFCUsXx");
    EXPECT_EQ(tk.symbol.toStdString(), "ATLAS");
}

// ── Unknown mint returns empty ──────────────────────────────────

TEST(KnownTokens, UnknownMintReturnsEmpty) {
    KnownToken tk = resolveKnownToken("UnknownMint111111111111111111111111111111111");
    EXPECT_TRUE(tk.symbol.isEmpty());
    EXPECT_TRUE(tk.iconPath.isEmpty());
}

TEST(KnownTokens, EmptyMintReturnsEmpty) {
    KnownToken tk = resolveKnownToken("");
    EXPECT_TRUE(tk.symbol.isEmpty());
    EXPECT_TRUE(tk.iconPath.isEmpty());
}

// ── Native SOL (system program) is NOT the WSOL mint ────────────

TEST(KnownTokens, SystemProgramIsNotWsol) {
    // The system program address should NOT resolve to any token
    KnownToken tk = resolveKnownToken("11111111111111111111111111111111");
    EXPECT_TRUE(tk.symbol.isEmpty());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
