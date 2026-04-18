#ifndef TERMINALCOMMANDCATALOG_H
#define TERMINALCOMMANDCATALOG_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

enum class TerminalCommandRoot {
    Help,
    Clear,
    Address,
    Balance,
    Balances,
    Send,
    History,
    Version,
    Echo,
    Wallet,
    Key,
    Mnemonic,
    Tx,
    Token,
    Account,
    Rpc,
    Contact,
    Portfolio,
    Program,
    Encode,
    Db,
    Nonce,
    Network,
    Stake,
    Validator,
    Swap,
    Price,
    Unknown,
};

enum class TerminalSubcommand {
    None,
    // Wallet
    WalletInfo,
    // Key
    KeyGenerate,
    KeyFromSecret,
    KeyFromMnemonic,
    KeyPubkey,
    KeyVerify,
    // Mnemonic
    MnemonicGenerate,
    MnemonicValidate,
    // Tx
    TxClassify,
    TxDeltas,
    TxCu,
    TxSignatures,
    TxDecode,
    TxSimulate,
    // Token
    TokenInfo,
    TokenAccounts,
    TokenAta,
    TokenSend,
    TokenBurn,
    TokenClose,
    // Account
    AccountNonce,
    AccountRent,
    // Rpc
    RpcSet,
    RpcAdd,
    RpcRemove,
    RpcBlockhash,
    RpcFees,
    // Contact
    ContactList,
    ContactAdd,
    ContactRemove,
    ContactFind,
    ContactLookup,
    // Portfolio
    PortfolioSummary,
    PortfolioHistory,
    PortfolioLots,
    // Program
    ProgramFetch,
    ProgramIsDex,
    // Encode
    EncodeBase58,
    EncodeDecode58,
    EncodeHash,
    // Db
    DbStats,
    // Nonce
    NonceInfo,
    NonceCreate,
    NonceRefresh,
    NonceAdvance,
    NonceWithdraw,
    NonceClose,
    // Stake
    StakeList,
    StakeCreate,
    StakeDeactivate,
    StakeWithdraw,
    // Validator
    ValidatorList,
    ValidatorInfo,
    // Swap
    SwapQuote,
};

struct TerminalParsedCommand {
    QString raw;
    QStringList parts;
    QString rootToken;
    QString subcommandToken;
    QStringList positionalArgs;
    TerminalCommandRoot root = TerminalCommandRoot::Unknown;
    TerminalSubcommand sub = TerminalSubcommand::None;
};

struct TerminalHelpEntry {
    QString group;
    QString usage;
    QString description;
};

enum class TerminalCommandValidity {
    Valid,
    UnknownRoot,
    UnknownSubcommand,
};

struct TerminalCommandValidation {
    TerminalCommandValidity validity = TerminalCommandValidity::Valid;
    QString message;
};

struct TerminalCommandSpec {
    TerminalCommandRoot root = TerminalCommandRoot::Unknown;
    QString name;
    QString group;
    QString description;
    QStringList subcommands;
};

TerminalParsedCommand parseTerminalCommand(const QString& command);
TerminalCommandRoot terminalCommandRootFromString(const QString& root);
QString terminalCommandName(TerminalCommandRoot root);
const QStringList& terminalRootCommands();
const QMap<QString, QStringList>& terminalSubcommandMap();
const QList<TerminalHelpEntry>& terminalHelpEntries();
bool terminalHasSubcommand(TerminalCommandRoot root, const QString& subcommand);
const QList<TerminalCommandSpec>& terminalCommandSpecs();
TerminalCommandValidation terminalValidateCommand(const TerminalParsedCommand& parsed);
QString terminalPrimaryUsage(TerminalCommandRoot root);

#endif // TERMINALCOMMANDCATALOG_H
