#include "TerminalCommandCatalog.h"

namespace {

    struct TerminalSubcommandSpec {
        TerminalSubcommand sub;
        QString name;
        QString usageSuffix;
        QString description;
    };

    struct TerminalCommandDefinition {
        TerminalCommandRoot root;
        QString name;
        QString group;
        QString usageSuffix;
        QString description;
        QList<TerminalSubcommandSpec> subcommands;
    };

    const QList<TerminalCommandDefinition> kDefinitions = {
        {TerminalCommandRoot::Address, "address", "Core", "", "Show active wallet address", {}},
        {TerminalCommandRoot::Balance, "balance", "Core", "", "Fetch SOL balance from RPC", {}},
        {TerminalCommandRoot::Balances,
         "balances",
         "Core",
         "",
         "Show token balances cached in DB",
         {}},
        {TerminalCommandRoot::Clear, "clear", "Core", "", "Clear terminal output", {}},
        {TerminalCommandRoot::Db,
         "db",
         "Core",
         "<stats>",
         "Database diagnostics",
         {{TerminalSubcommand::DbStats, "stats", "stats", "Show database statistics"}}},
        {TerminalCommandRoot::Echo, "echo", "Core", "<text>", "Print text to the terminal", {}},
        {TerminalCommandRoot::Help, "help", "Core", "", "Show help", {}},
        {TerminalCommandRoot::History,
         "history",
         "Core",
         "[n]",
         "Show latest n transactions from DB",
         {}},
        {TerminalCommandRoot::Send,
         "send",
         "Core",
         "<to_address> <sol_amount>",
         "Send SOL to an address",
         {}},
        {TerminalCommandRoot::Version, "version", "Core", "", "Show app + Qt version", {}},
        // ── Wallet / Keys ─────────────────────────────────────────
        {TerminalCommandRoot::Key,
         "key",
         "Wallet / Keys",
         "<generate|from-secret|from-mnemonic|pubkey|verify>",
         "Key utilities",
         {{TerminalSubcommand::KeyGenerate, "generate", "generate", "Generate ephemeral keypair"},
          {TerminalSubcommand::KeyFromSecret, "from-secret", "from-secret <base58>",
           "Load keypair from secret key"},
          {TerminalSubcommand::KeyFromMnemonic, "from-mnemonic",
           "from-mnemonic <words...> [--path p]", "Derive keypair from mnemonic"},
          {TerminalSubcommand::KeyPubkey, "pubkey", "pubkey <secret_base58>",
           "Print pubkey from secret"},
          {TerminalSubcommand::KeyVerify, "verify", "verify <pub> <msg> <sig>",
           "Verify Ed25519 signature"}}},
        {TerminalCommandRoot::Mnemonic,
         "mnemonic",
         "Wallet / Keys",
         "<generate|validate>",
         "Mnemonic utilities",
         {{TerminalSubcommand::MnemonicGenerate, "generate", "generate [12|24]",
           "Generate mnemonic"},
          {TerminalSubcommand::MnemonicValidate, "validate", "validate <words...>",
           "Validate mnemonic"}}},
        {TerminalCommandRoot::Wallet,
         "wallet",
         "Wallet / Keys",
         "<info>",
         "Show wallet metadata",
         {{TerminalSubcommand::WalletInfo, "info", "info", "Show current wallet metadata"}}},
        // ── Transactions ─────────────────────────────────────────
        {TerminalCommandRoot::Tx,
         "tx",
         "Transactions",
         "<signature|classify|deltas|cu|signatures|decode|simulate>",
         "Transaction analysis",
         {{TerminalSubcommand::TxClassify, "classify", "classify <sig>",
           "Classify transaction intent"},
          {TerminalSubcommand::TxDeltas, "deltas", "deltas <sig>", "Token balance deltas"},
          {TerminalSubcommand::TxCu, "cu", "cu <sig>", "Compute-unit breakdown"},
          {TerminalSubcommand::TxSignatures, "signatures", "signatures <address> [limit]",
           "Fetch recent signatures"},
          {TerminalSubcommand::TxDecode, "decode", "decode <base58_data>",
           "Decode instruction payload"},
          {TerminalSubcommand::TxSimulate, "simulate", "simulate <base64_encoded_tx>",
           "Simulate transaction"}}},
        // ── Token / Accounts ─────────────────────────────────────
        {TerminalCommandRoot::Account,
         "account",
         "Token / Accounts",
         "<address|nonce|rent>",
         "Account inspection",
         {{TerminalSubcommand::AccountNonce, "nonce", "nonce <address>", "Parse nonce account"},
          {TerminalSubcommand::AccountRent, "rent", "rent <bytes>", "Rent exemption quote"}}},
        {TerminalCommandRoot::Token,
         "token",
         "Token / Accounts",
         "<info|send|burn|close|accounts|ata>",
         "Token helpers",
         {{TerminalSubcommand::TokenInfo, "info", "info <mint>", "Show token metadata"},
          {TerminalSubcommand::TokenAccounts, "accounts", "accounts <address>",
           "Fetch token accounts from RPC"},
          {TerminalSubcommand::TokenAta, "ata", "ata <owner> <mint>", "Derive ATA"},
          {TerminalSubcommand::TokenSend, "send", "send <mint> <to_address> <amount>",
           "Send tokens to an address"},
          {TerminalSubcommand::TokenBurn, "burn", "burn <mint> <amount>",
           "Burn tokens from wallet"},
          {TerminalSubcommand::TokenClose, "close", "close <mint>", "Close empty token account"}}},
        // ── RPC / Network ────────────────────────────────────────
        {TerminalCommandRoot::Network, "network", "RPC / Network", "", "Show network snapshot", {}},
        {TerminalCommandRoot::Nonce,
         "nonce",
         "RPC / Network",
         "<info|create|refresh|advance|withdraw|close>",
         "Durable nonce operations",
         {{TerminalSubcommand::NonceInfo, "info", "info", "Show nonce account details"},
          {TerminalSubcommand::NonceCreate, "create", "create", "Create nonce account"},
          {TerminalSubcommand::NonceRefresh, "refresh", "refresh", "Refresh nonce value"},
          {TerminalSubcommand::NonceAdvance, "advance", "advance", "Advance nonce"},
          {TerminalSubcommand::NonceWithdraw, "withdraw", "withdraw <sol_amount>",
           "Withdraw nonce lamports"},
          {TerminalSubcommand::NonceClose, "close", "close", "Close nonce account"}}},
        {TerminalCommandRoot::Rpc,
         "rpc",
         "RPC / Network",
         "<set|add|remove|blockhash|fees>",
         "RPC endpoint and fee tools",
         {{TerminalSubcommand::RpcSet, "set", "set <url>", "Change RPC endpoint"},
          {TerminalSubcommand::RpcAdd, "add", "add <url>", "Add RPC endpoint"},
          {TerminalSubcommand::RpcRemove, "remove", "remove <url>", "Remove RPC endpoint"},
          {TerminalSubcommand::RpcBlockhash, "blockhash", "blockhash", "Fetch latest blockhash"},
          {TerminalSubcommand::RpcFees, "fees", "fees", "Fetch recent prioritization fees"}}},
        // ── Contacts / Portfolio ─────────────────────────────────
        {TerminalCommandRoot::Contact,
         "contact",
         "Contacts / Portfolio",
         "<list|add|remove|find|lookup>",
         "Address-book commands",
         {{TerminalSubcommand::ContactList, "list", "list", "List contacts"},
          {TerminalSubcommand::ContactAdd, "add", "add <name> <address>", "Add contact"},
          {TerminalSubcommand::ContactRemove, "remove", "remove <id>", "Remove contact"},
          {TerminalSubcommand::ContactFind, "find", "find <query>", "Find contacts"},
          {TerminalSubcommand::ContactLookup, "lookup", "lookup <address>", "Lookup by address"}}},
        {TerminalCommandRoot::Portfolio,
         "portfolio",
         "Contacts / Portfolio",
         "<summary|history|lots>",
         "Portfolio snapshots",
         {{TerminalSubcommand::PortfolioSummary, "summary", "summary",
           "Show latest portfolio snapshot"},
          {TerminalSubcommand::PortfolioHistory, "history", "history [days]",
           "Show historical daily values"},
          {TerminalSubcommand::PortfolioLots, "lots", "lots <mint>", "Show open tax lots"}}},
        // ── Programs / Utilities ─────────────────────────────────
        {TerminalCommandRoot::Encode,
         "encode",
         "Programs / Utilities",
         "<base58|decode58|hash>",
         "Encoding utilities",
         {{TerminalSubcommand::EncodeBase58, "base58", "base58 <hex>", "Hex -> Base58"},
          {TerminalSubcommand::EncodeDecode58, "decode58", "decode58 <b58>", "Base58 -> Hex"},
          {TerminalSubcommand::EncodeHash, "hash", "hash <text>", "SHA-256"}}},
        {TerminalCommandRoot::Program,
         "program",
         "Programs / Utilities",
         "<fetch|is-dex>",
         "Program/IDL utilities",
         {{TerminalSubcommand::ProgramFetch, "fetch", "fetch <program_id>", "Resolve/fetch IDL"},
          {TerminalSubcommand::ProgramIsDex, "is-dex", "is-dex <program_id>", "Check known DEX"}}},
        // ── Staking / Validators / Swap / Price ──────────────────
        {TerminalCommandRoot::Price,
         "price",
         "Staking / Validators / Swap / Price",
         "<mint>",
         "Price lookup",
         {}},
        {TerminalCommandRoot::Stake,
         "stake",
         "Staking / Validators / Swap / Price",
         "<list|create|deactivate|withdraw>",
         "Stake operations",
         {{TerminalSubcommand::StakeList, "list", "list [address]", "List stake accounts"},
          {TerminalSubcommand::StakeCreate, "create", "create <vote_account> <sol_amount>",
           "Create + delegate stake"},
          {TerminalSubcommand::StakeDeactivate, "deactivate", "deactivate <stake_account>",
           "Deactivate stake"},
          {TerminalSubcommand::StakeWithdraw, "withdraw", "withdraw <stake_account> <sol_amount>",
           "Withdraw stake"}}},
        {TerminalCommandRoot::Swap,
         "swap",
         "Staking / Validators / Swap / Price",
         "<quote>",
         "Jupiter quote",
         {{TerminalSubcommand::SwapQuote, "quote", "quote <inMint> <outMint> <amount>",
           "Fetch swap quote"}}},
        {TerminalCommandRoot::Validator,
         "validator",
         "Staking / Validators / Swap / Price",
         "<list|info>",
         "Validator info",
         {{TerminalSubcommand::ValidatorList, "list", "list [--top n]", "List validators"},
          {TerminalSubcommand::ValidatorInfo, "info", "info <vote_account>", "Validator details"}}},
    };

    const QMap<QString, TerminalCommandRoot> kByRoot = []() {
        QMap<QString, TerminalCommandRoot> map;
        for (const auto& def : kDefinitions) {
            map.insert(def.name, def.root);
        }
        return map;
    }();

    const QStringList kRootCommands = []() {
        QStringList roots;
        for (const auto& def : kDefinitions) {
            roots << def.name;
        }
        return roots;
    }();

    const QMap<QString, QStringList> kSubcommands = []() {
        QMap<QString, QStringList> map;
        for (const auto& def : kDefinitions) {
            QStringList subs;
            for (const auto& sub : def.subcommands) {
                subs << sub.name;
            }
            if (!subs.isEmpty()) {
                map.insert(def.name, subs);
            }
        }
        return map;
    }();

    const QList<TerminalHelpEntry> kHelpEntries = []() {
        QList<TerminalHelpEntry> entries;
        for (const auto& def : kDefinitions) {
            const QString rootUsage =
                def.usageSuffix.isEmpty() ? def.name : def.name + " " + def.usageSuffix;
            entries.append({def.group, rootUsage, def.description});
            for (const auto& sub : def.subcommands) {
                entries.append({def.group, def.name + " " + sub.usageSuffix, sub.description});
            }
        }
        return entries;
    }();

    const QList<TerminalCommandSpec> kSpecs = []() {
        QList<TerminalCommandSpec> specs;
        for (const auto& def : kDefinitions) {
            TerminalCommandSpec spec;
            spec.root = def.root;
            spec.name = def.name;
            spec.group = def.group;
            spec.description = def.description;
            for (const auto& sub : def.subcommands) {
                spec.subcommands << sub.name;
            }
            specs << spec;
        }
        return specs;
    }();

    using SubKey = QPair<TerminalCommandRoot, QString>;
    const QMap<SubKey, TerminalSubcommand> kSubcommandEnum = []() {
        QMap<SubKey, TerminalSubcommand> map;
        for (const auto& def : kDefinitions) {
            for (const auto& s : def.subcommands) {
                map.insert({def.root, s.name}, s.sub);
            }
        }
        return map;
    }();

} // namespace

TerminalParsedCommand parseTerminalCommand(const QString& command) {
    TerminalParsedCommand parsed;
    parsed.raw = command.trimmed();
    parsed.parts = parsed.raw.split(' ', Qt::SkipEmptyParts);
    if (parsed.parts.isEmpty()) {
        parsed.root = TerminalCommandRoot::Unknown;
        return parsed;
    }
    parsed.rootToken = parsed.parts[0].toLower();
    parsed.root = terminalCommandRootFromString(parsed.rootToken);
    if (parsed.parts.size() > 1) {
        parsed.subcommandToken = parsed.parts[1].toLower();
        parsed.positionalArgs = parsed.parts.mid(2);
        auto it = kSubcommandEnum.constFind({parsed.root, parsed.subcommandToken});
        if (it != kSubcommandEnum.constEnd()) {
            parsed.sub = it.value();
        }
    }
    return parsed;
}

TerminalCommandRoot terminalCommandRootFromString(const QString& root) {
    return kByRoot.value(root.toLower(), TerminalCommandRoot::Unknown);
}

QString terminalCommandName(TerminalCommandRoot root) {
    for (const auto& def : kDefinitions) {
        if (def.root == root) {
            return def.name;
        }
    }
    return {};
}

const QStringList& terminalRootCommands() { return kRootCommands; }

const QMap<QString, QStringList>& terminalSubcommandMap() { return kSubcommands; }

const QList<TerminalHelpEntry>& terminalHelpEntries() { return kHelpEntries; }

bool terminalHasSubcommand(TerminalCommandRoot root, const QString& subcommand) {
    const QString rootName = terminalCommandName(root);
    if (rootName.isEmpty()) {
        return false;
    }
    const auto it = kSubcommands.constFind(rootName);
    if (it == kSubcommands.constEnd()) {
        return false;
    }
    return it.value().contains(subcommand.toLower());
}

const QList<TerminalCommandSpec>& terminalCommandSpecs() { return kSpecs; }

TerminalCommandValidation terminalValidateCommand(const TerminalParsedCommand& parsed) {
    if (parsed.root == TerminalCommandRoot::Unknown) {
        return {TerminalCommandValidity::UnknownRoot,
                QString("Command not found: %1").arg(parsed.rootToken)};
    }

    if (parsed.subcommandToken.isEmpty()) {
        return {TerminalCommandValidity::Valid, {}};
    }

    const QString rootName = terminalCommandName(parsed.root);
    const auto it = kSubcommands.constFind(rootName);
    if (it == kSubcommands.constEnd()) {
        return {TerminalCommandValidity::Valid, {}};
    }

    if (!it.value().contains(parsed.subcommandToken)) {
        return {TerminalCommandValidity::UnknownSubcommand,
                QString("Unknown %1 subcommand: %2").arg(rootName, parsed.subcommandToken)};
    }

    return {TerminalCommandValidity::Valid, {}};
}

QString terminalPrimaryUsage(TerminalCommandRoot root) {
    for (const auto& def : kDefinitions) {
        if (def.root != root) {
            continue;
        }
        return def.usageSuffix.isEmpty() ? def.name : def.name + " " + def.usageSuffix;
    }
    return {};
}
