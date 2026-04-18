#ifndef MCPTOOLDEFS_H
#define MCPTOOLDEFS_H

#include <QList>
#include <QString>

enum class McpToolCategory { Read, Write };

struct McpToolDef {
    const char* name;
    const char* description;
    McpToolCategory category;
    bool defaultEnabled;
    bool fundRisk; // true if allowing this tool can move/destroy funds
};

inline QList<McpToolDef> allMcpTools() {
    return {
        // ── Read Tools ────────────────────────────────────────────────────
        {"wallet_ping", "Check if the Cinder Solana wallet MCP server is running",
         McpToolCategory::Read, true, false},
        {"wallet_address", "Get the active Solana wallet address", McpToolCategory::Read, true,
         false},
        {"wallet_info", "Get Solana wallet details and RPC endpoint", McpToolCategory::Read, true,
         false},
        {"wallet_list_wallets", "List all Solana wallets in Cinder", McpToolCategory::Read, true,
         false},
        {"wallet_get_balance", "Get native SOL balance for the active Solana wallet (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_get_balances", "Get all Solana token balances from the wallet database",
         McpToolCategory::Read, true, false},
        {"wallet_get_portfolio", "Get the latest Solana portfolio snapshot", McpToolCategory::Read,
         true, false},
        {"wallet_get_portfolio_history", "Get Solana portfolio snapshots over time",
         McpToolCategory::Read, true, false},
        {"wallet_get_transactions", "Get Solana transaction history for the wallet",
         McpToolCategory::Read, true, false},
        {"wallet_get_transaction", "Get raw JSON for a Solana transaction by signature",
         McpToolCategory::Read, true, false},
        {"wallet_classify_transaction", "Classify a Solana transaction by its activities",
         McpToolCategory::Read, true, false},
        {"wallet_get_token_info", "Get Solana SPL token metadata by mint address",
         McpToolCategory::Read, true, false},
        {"wallet_get_token_accounts", "Get Solana SPL token accounts for the wallet (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_derive_ata", "Derive a Solana associated token account address for a mint",
         McpToolCategory::Read, true, false},
        {"wallet_inspect_account", "Inspect any on-chain Solana account (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_get_rent_exempt",
         "Get Solana minimum lamport balance for rent exemption (live RPC)", McpToolCategory::Read,
         true, false},
        {"wallet_get_stake_accounts", "Get Solana stake accounts for the wallet (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_get_validators", "Get Solana validator list from cache", McpToolCategory::Read,
         true, false},
        {"wallet_get_validator_info", "Get details for a Solana validator by vote account",
         McpToolCategory::Read, true, false},
        {"wallet_get_swap_quote", "Get a Solana token swap quote from Jupiter",
         McpToolCategory::Read, true, false},
        {"wallet_get_price", "Get current USD price for a Solana token mint", McpToolCategory::Read,
         true, false},
        {"wallet_get_network_stats", "Get Solana network statistics (TPS, epoch, slot)",
         McpToolCategory::Read, true, false},
        {"wallet_get_blockhash", "Get the latest Solana blockhash (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_get_priority_fees", "Get recent Solana prioritization fees (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_get_contacts", "Get all contacts from the Solana wallet address book",
         McpToolCategory::Read, true, false},
        {"wallet_find_contact", "Search Solana wallet contacts by name or address",
         McpToolCategory::Read, true, false},
        {"wallet_simulate_transaction", "Simulate a base64-encoded Solana transaction (live RPC)",
         McpToolCategory::Read, true, false},
        {"wallet_encode_base58", "Encode bytes to Solana Base58", McpToolCategory::Read, true,
         false},
        {"wallet_decode_base58", "Decode a Solana Base58 string to hex bytes",
         McpToolCategory::Read, true, false},

        // ── Write Tools (require approval) ───────────────────────────────
        {"wallet_send_sol", "Transfer native SOL on Solana to an address", McpToolCategory::Write,
         true, true},
        {"wallet_send_token", "Transfer Solana SPL tokens to an address", McpToolCategory::Write,
         true, true},
        {"wallet_swap", "Swap Solana tokens via Jupiter DEX", McpToolCategory::Write, true, true},
        {"wallet_stake_create", "Create and delegate a Solana stake account",
         McpToolCategory::Write, true, false},
        {"wallet_stake_deactivate", "Deactivate a Solana stake account", McpToolCategory::Write,
         true, false},
        {"wallet_stake_withdraw", "Withdraw SOL from a Solana stake account",
         McpToolCategory::Write, true, true},
        {"wallet_token_burn", "Burn Solana SPL tokens", McpToolCategory::Write, true, true},
        {"wallet_token_close", "Close an empty Solana SPL token account", McpToolCategory::Write,
         true, true},
        {"wallet_nonce_create", "Create a Solana durable nonce account", McpToolCategory::Write,
         true, false},
        {"wallet_nonce_advance", "Advance a Solana durable nonce value", McpToolCategory::Write,
         true, false},
        {"wallet_nonce_withdraw", "Withdraw SOL from a Solana nonce account",
         McpToolCategory::Write, true, true},
        {"wallet_nonce_close", "Close a Solana nonce account and reclaim SOL",
         McpToolCategory::Write, true, true},
        {"wallet_add_contact", "Add a contact to the Solana wallet address book",
         McpToolCategory::Write, true, false},
        {"wallet_remove_contact", "Remove a contact from the Solana wallet address book",
         McpToolCategory::Write, true, false},
    };
}

inline QString mcpToolCategoryString(McpToolCategory cat) {
    return cat == McpToolCategory::Read ? QStringLiteral("read") : QStringLiteral("write");
}

#endif // MCPTOOLDEFS_H
