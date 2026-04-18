#include "features/terminal/TerminalCompletion.h"

#include <gtest/gtest.h>

TEST(TerminalCompletionTest, CompletesSingleRootCommand) {
    const TerminalCompletionResult result = completeTerminalInput("wallet i");
    ASSERT_TRUE(result.handled);
    EXPECT_EQ(result.updatedInput, "wallet info ");
    EXPECT_TRUE(result.candidatesToShow.isEmpty());
}

TEST(TerminalCompletionTest, CompletesSingleSubcommand) {
    const TerminalCompletionResult result = completeTerminalInput("nonce a");
    ASSERT_TRUE(result.handled);
    EXPECT_EQ(result.updatedInput, "nonce advance ");
    EXPECT_TRUE(result.candidatesToShow.isEmpty());
}

TEST(TerminalCompletionTest, ReturnsCandidatesForMultiMatch) {
    const TerminalCompletionResult result = completeTerminalInput("bal");
    ASSERT_TRUE(result.handled);
    EXPECT_EQ(result.updatedInput, "balance");
    EXPECT_FALSE(result.candidatesToShow.isEmpty());
    EXPECT_TRUE(result.candidatesToShow.contains("balance"));
    EXPECT_TRUE(result.candidatesToShow.contains("balances"));
}

TEST(TerminalCompletionTest, IgnoresInputsBeyondSupportedDepth) {
    const TerminalCompletionResult result = completeTerminalInput("wallet list x");
    EXPECT_FALSE(result.handled);
}
