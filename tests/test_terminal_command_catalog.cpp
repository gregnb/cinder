#include "features/terminal/TerminalCommandCatalog.h"

#include <gtest/gtest.h>

TEST(TerminalCommandCatalogTest, ParsesRootSubcommandAndArgs) {
    const TerminalParsedCommand parsed = parseTerminalCommand("tx signatures 9f6abc... 25");

    EXPECT_EQ(parsed.root, TerminalCommandRoot::Tx);
    EXPECT_EQ(parsed.rootToken, "tx");
    EXPECT_EQ(parsed.subcommandToken, "signatures");
    ASSERT_EQ(parsed.positionalArgs.size(), 2);
    EXPECT_EQ(parsed.positionalArgs[0], "9f6abc...");
    EXPECT_EQ(parsed.positionalArgs[1], "25");
}

TEST(TerminalCommandCatalogTest, HandlesUnknownRoot) {
    const TerminalParsedCommand parsed = parseTerminalCommand("bogus foo bar");

    EXPECT_EQ(parsed.root, TerminalCommandRoot::Unknown);
    EXPECT_EQ(parsed.rootToken, "bogus");
    EXPECT_EQ(parsed.subcommandToken, "foo");
    ASSERT_EQ(parsed.positionalArgs.size(), 1);
    EXPECT_EQ(parsed.positionalArgs[0], "bar");
}

TEST(TerminalCommandCatalogTest, EmptyInputIsUnknown) {
    const TerminalParsedCommand parsed = parseTerminalCommand("   ");

    EXPECT_EQ(parsed.root, TerminalCommandRoot::Unknown);
    EXPECT_TRUE(parsed.rootToken.isEmpty());
    EXPECT_TRUE(parsed.subcommandToken.isEmpty());
    EXPECT_TRUE(parsed.positionalArgs.isEmpty());
}

TEST(TerminalCommandCatalogTest, ValidatesKnownSubcommands) {
    EXPECT_TRUE(terminalHasSubcommand(TerminalCommandRoot::Tx, "decode"));
    EXPECT_TRUE(terminalHasSubcommand(TerminalCommandRoot::Rpc, "set"));
    EXPECT_FALSE(terminalHasSubcommand(TerminalCommandRoot::Price, "set"));
    EXPECT_FALSE(terminalHasSubcommand(TerminalCommandRoot::Unknown, "set"));
}

TEST(TerminalCommandCatalogTest, ValidatesUnknownSubcommandWithMessage) {
    const TerminalParsedCommand parsed = parseTerminalCommand("rpc banana");
    const TerminalCommandValidation validation = terminalValidateCommand(parsed);

    EXPECT_EQ(validation.validity, TerminalCommandValidity::UnknownSubcommand);
    EXPECT_EQ(validation.message, "Unknown rpc subcommand: banana");
}

TEST(TerminalCommandCatalogTest, PrimaryUsageComesFromCatalog) {
    EXPECT_EQ(terminalPrimaryUsage(TerminalCommandRoot::Send), "send <to_address> <sol_amount>");
    EXPECT_EQ(terminalPrimaryUsage(TerminalCommandRoot::Rpc),
              "rpc <set|add|remove|blockhash|fees>");
}
