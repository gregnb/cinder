#ifndef TOKENACCOUNTDB_H
#define TOKENACCOUNTDB_H

#include <QList>
#include <QString>
#include <optional>

class QSqlDatabase;

struct TokenInfoRecord {
    QString address;
    QString symbol;
    QString name;
    int decimals = 0;
    QString tokenProgram;
    QString logoUrl;
    bool metadataFetched = false;
    qint64 createdAt = 0;
    qint64 updatedAt = 0;
};

struct TokenAccountRecord {
    QString accountAddress;
    QString tokenAddress;
    QString ownerAddress;
    QString balance;
    double usdPrice = 0.0;
    QString state;
    qint64 createdAt = 0;
    qint64 updatedAt = 0;
    QString symbol;
    QString name;
    int decimals = 0;
    QString tokenProgram;
    QString logoUrl;
};

class TokenAccountDb {
  public:
    // ── Token registry ─────────────────────────────────────
    static bool upsertToken(const QString& address, const QString& symbol, const QString& name,
                            int decimals, const QString& tokenProgram);

    static std::optional<TokenInfoRecord> getTokenRecord(const QString& address);

    // Mint authority for a token (nullable — NULL means not yet fetched)
    static bool setMintAuthority(const QString& mintAddress, const QString& authority);
    static QStringList getMintsWithoutAuthority(const QString& ownerAddress);
    static QList<TokenAccountRecord> getMintableAccounts(const QString& ownerAddress);

    // CoinGecko ID cache — NULL = never looked up, "" = not found
    static QString getCoingeckoId(const QString& mintAddress);
    static bool setCoingeckoId(const QString& mintAddress, const QString& coingeckoId);

    // ── Token accounts (balances) ──────────────────────────
    static bool upsertAccount(const QString& accountAddress, const QString& tokenAddress,
                              const QString& ownerAddress, const QString& balance, double usdPrice,
                              const QString& state = "initialized");

    static bool updateBalance(const QString& accountAddress, const QString& balance,
                              double usdPrice);

    static bool deleteAccount(const QString& accountAddress);

    // Returns joined rows: token_accounts + tokens (symbol, name, decimals)
    static QList<TokenAccountRecord> getAccountsByOwnerRecords(const QString& ownerAddress);

    static std::optional<TokenAccountRecord> getAccountRecord(const QString& ownerAddress,
                                                              const QString& tokenAddress);

    // Count token accounts for an owner.
    static int countByOwner(const QString& ownerAddress);

    // ── Token metadata ──────────────────────────────────────
    // Returns mint addresses where metadata_fetched = 0 (pending resolution).
    static QStringList getUnfetchedMints();

    // Insert stub token rows for mints found in transactions but not in tokens table.
    static int insertMissingTransactionMints();

    // Update resolved on-chain metadata for a token.
    static bool updateTokenMetadata(const QString& address, const QString& name,
                                    const QString& symbol, const QString& logoUrl);

  private:
    static QSqlDatabase db();
};

#endif // TOKENACCOUNTDB_H
