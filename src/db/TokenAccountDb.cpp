#include "TokenAccountDb.h"
#include "DbUtil.h"
#include "db/Database.h"
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {

    TokenInfoRecord tokenInfoFromQuery(const QSqlQuery& q) {
        TokenInfoRecord r;
        r.address = q.value("address").toString();
        r.symbol = q.value("symbol").toString();
        r.name = q.value("name").toString();
        r.decimals = q.value("decimals").toInt();
        r.tokenProgram = q.value("token_program").toString();
        r.logoUrl = q.value("logo_url").toString();
        r.metadataFetched = q.value("metadata_fetched").toInt() != 0;
        r.createdAt = q.value("created_at").toLongLong();
        r.updatedAt = q.value("updated_at").toLongLong();
        return r;
    }

    TokenAccountRecord tokenAccountFromQuery(const QSqlQuery& q) {
        TokenAccountRecord r;
        r.accountAddress = q.value("account_address").toString();
        r.tokenAddress = q.value("token_address").toString();
        r.ownerAddress = q.value("owner_address").toString();
        r.balance = q.value("balance").toString();
        r.usdPrice = q.value("usd_price").toDouble();
        r.state = q.value("state").toString();
        r.createdAt = q.value("created_at").toLongLong();
        r.updatedAt = q.value("updated_at").toLongLong();
        r.symbol = q.value("symbol").toString();
        r.name = q.value("name").toString();
        r.decimals = q.value("decimals").toInt();
        r.tokenProgram = q.value("token_program").toString();
        r.logoUrl = q.value("logo_url").toString();
        return r;
    }

} // namespace

QSqlDatabase TokenAccountDb::db() { return Database::connection(); }

// ── Token registry ─────────────────────────────────────────────

bool TokenAccountDb::upsertToken(const QString& address, const QString& symbol, const QString& name,
                                 int decimals, const QString& tokenProgram) {
    static const QString kSql = R"(
        INSERT INTO tokens (address, symbol, name, decimals, token_program)
        VALUES (:addr, :sym, :name, :dec, :prog)
        ON CONFLICT(address) DO UPDATE SET
            symbol = CASE WHEN tokens.metadata_fetched = 1 THEN tokens.symbol ELSE excluded.symbol END,
            name   = CASE WHEN tokens.metadata_fetched = 1 THEN tokens.name   ELSE excluded.name   END,
            decimals = excluded.decimals,
            token_program = excluded.token_program,
            updated_at = strftime('%s', 'now')
    )";

    return DbUtil::exec(db(), kSql,
                        {{":addr", address},
                         {":sym", symbol},
                         {":name", name},
                         {":dec", decimals},
                         {":prog", tokenProgram}});
}

std::optional<TokenInfoRecord> TokenAccountDb::getTokenRecord(const QString& address) {
    return DbUtil::one<TokenInfoRecord>(db(), "SELECT * FROM tokens WHERE address = :addr LIMIT 1",
                                        {{":addr", address}}, tokenInfoFromQuery);
}

// ── CoinGecko ID cache ────────────────────────────────────────

QString TokenAccountDb::getCoingeckoId(const QString& mintAddress) {
    return DbUtil::scalarString(db(),
                                "SELECT coingecko_id FROM tokens WHERE address = :addr LIMIT 1",
                                {{":addr", mintAddress}})
        .value_or(QString{});
}

bool TokenAccountDb::setCoingeckoId(const QString& mintAddress, const QString& coingeckoId) {
    return DbUtil::exec(
        db(),
        "UPDATE tokens SET coingecko_id = :cgid, updated_at = strftime('%s', 'now') "
        "WHERE address = :addr",
        {{":cgid", coingeckoId}, {":addr", mintAddress}}, DbUtil::RequireRows::Yes);
}

// ── Token accounts ─────────────────────────────────────────────

bool TokenAccountDb::upsertAccount(const QString& accountAddress, const QString& tokenAddress,
                                   const QString& ownerAddress, const QString& balance,
                                   double usdPrice, const QString& state) {
    static const QString kSql = R"(
        INSERT INTO token_accounts
            (account_address, token_address, owner_address, balance, usd_price, state)
        VALUES (:acct, :token, :owner, :bal, :price, :state)
        ON CONFLICT(account_address) DO UPDATE SET
            balance = excluded.balance,
            usd_price = excluded.usd_price,
            state = excluded.state,
            updated_at = strftime('%s', 'now')
    )";

    return DbUtil::exec(db(), kSql,
                        {{":acct", accountAddress},
                         {":token", tokenAddress},
                         {":owner", ownerAddress},
                         {":bal", balance},
                         {":price", usdPrice},
                         {":state", state}});
}

bool TokenAccountDb::updateBalance(const QString& accountAddress, const QString& balance,
                                   double usdPrice) {
    return DbUtil::exec(
        db(),
        "UPDATE token_accounts "
        "SET balance = :bal, usd_price = :price, updated_at = strftime('%s', 'now') "
        "WHERE account_address = :acct",
        {{":bal", balance}, {":price", usdPrice}, {":acct", accountAddress}},
        DbUtil::RequireRows::Yes);
}

bool TokenAccountDb::deleteAccount(const QString& accountAddress) {
    return DbUtil::exec(db(), "DELETE FROM token_accounts WHERE account_address = :acct",
                        {{":acct", accountAddress}}, DbUtil::RequireRows::Yes);
}

QList<TokenAccountRecord> TokenAccountDb::getAccountsByOwnerRecords(const QString& ownerAddress) {
    static const QString kSql = R"(
        SELECT
            ta.account_address,
            ta.token_address,
            ta.owner_address,
            ta.balance,
            ta.usd_price,
            ta.state,
            ta.created_at,
            ta.updated_at,
            t.symbol,
            t.name,
            t.decimals,
            t.token_program,
            t.logo_url
        FROM token_accounts ta
        JOIN tokens t ON ta.token_address = t.address
        WHERE ta.owner_address = :owner
        ORDER BY ta.usd_price * CAST(ta.balance AS REAL) DESC
    )";

    return DbUtil::many<TokenAccountRecord>(db(), kSql, {{":owner", ownerAddress}},
                                            tokenAccountFromQuery);
}

int TokenAccountDb::countByOwner(const QString& ownerAddress) {
    return DbUtil::scalarInt(db(),
                             "SELECT COUNT(*) FROM token_accounts WHERE owner_address = :owner",
                             {{":owner", ownerAddress}})
        .value_or(0);
}

int TokenAccountDb::insertMissingTransactionMints() {
    auto mints =
        DbUtil::many<QString>(db(),
                              "SELECT DISTINCT t.token FROM transactions t "
                              "WHERE t.token != 'SOL' "
                              "AND t.token NOT IN (SELECT address FROM tokens)",
                              {}, [](const QSqlQuery& q) { return q.value(0).toString(); });

    int count = 0;
    for (const QString& mint : mints) {
        if (mint.isEmpty()) {
            continue;
        }
        upsertToken(mint, mint.left(6), mint.left(6), 0, "unknown");
        ++count;
    }
    return count;
}

QStringList TokenAccountDb::getUnfetchedMints() {
    QList<QString> mints =
        DbUtil::many<QString>(db(), "SELECT address FROM tokens WHERE metadata_fetched = 0", {},
                              [](const QSqlQuery& q) { return q.value(0).toString(); });
    return QStringList(mints.begin(), mints.end());
}

bool TokenAccountDb::updateTokenMetadata(const QString& address, const QString& name,
                                         const QString& symbol, const QString& logoUrl) {
    // Native SOL uses the WSOL mint address but on-chain Metaplex metadata
    // returns "Wrapped SOL" — always force the canonical display name.
    static const QString kWsolMint = QStringLiteral("So11111111111111111111111111111111111111112");
    const QString resolvedName = (address == kWsolMint) ? QStringLiteral("Solana") : name;
    const QString resolvedSymbol = (address == kWsolMint) ? QStringLiteral("SOL") : symbol;

    return DbUtil::exec(
        db(),
        "UPDATE tokens SET name = :name, symbol = :sym, logo_url = :logo, "
        "metadata_fetched = 1, updated_at = strftime('%s', 'now') "
        "WHERE address = :addr",
        {{":name", resolvedName}, {":sym", resolvedSymbol}, {":logo", logoUrl}, {":addr", address}},
        DbUtil::RequireRows::Yes);
}

std::optional<TokenAccountRecord> TokenAccountDb::getAccountRecord(const QString& ownerAddress,
                                                                   const QString& tokenAddress) {
    static const QString kSql = R"(
        SELECT ta.*, t.symbol, t.name, t.decimals, t.token_program, t.logo_url
        FROM token_accounts ta
        JOIN tokens t ON ta.token_address = t.address
        WHERE ta.owner_address = :owner AND ta.token_address = :token
        LIMIT 1
    )";

    return DbUtil::one<TokenAccountRecord>(
        db(), kSql, {{":owner", ownerAddress}, {":token", tokenAddress}}, tokenAccountFromQuery);
}

// ── Mint authority ────────────────────────────────────────────

bool TokenAccountDb::setMintAuthority(const QString& mintAddress, const QString& authority) {
    return DbUtil::exec(
        db(),
        "UPDATE tokens SET mint_authority = :auth, updated_at = strftime('%s', 'now') "
        "WHERE address = :addr",
        {{":auth", authority}, {":addr", mintAddress}}, DbUtil::RequireRows::Yes);
}

QStringList TokenAccountDb::getMintsWithoutAuthority(const QString& ownerAddress) {
    static const QString kSql = R"(
        SELECT DISTINCT ta.token_address
        FROM token_accounts ta
        JOIN tokens t ON ta.token_address = t.address
        WHERE ta.owner_address = :owner AND t.mint_authority IS NULL
    )";
    QList<QString> mints =
        DbUtil::many<QString>(db(), kSql, {{":owner", ownerAddress}},
                              [](const QSqlQuery& q) { return q.value(0).toString(); });
    return QStringList(mints.begin(), mints.end());
}

QList<TokenAccountRecord> TokenAccountDb::getMintableAccounts(const QString& ownerAddress) {
    static const QString kSql = R"(
        SELECT ta.*, t.symbol, t.name, t.decimals, t.token_program, t.logo_url
        FROM token_accounts ta
        JOIN tokens t ON ta.token_address = t.address
        WHERE ta.owner_address = :owner AND t.mint_authority = :owner
        ORDER BY t.symbol
    )";
    return DbUtil::many<TokenAccountRecord>(db(), kSql, {{":owner", ownerAddress}},
                                            tokenAccountFromQuery);
}
