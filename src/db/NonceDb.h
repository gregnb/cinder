#ifndef NONCEDB_H
#define NONCEDB_H

#include <QString>
#include <optional>

struct NonceAccountRecord {
    int id = 0;
    QString address;
    QString authority;
    QString nonceValue;
    qint64 createdAt = 0;
};

class QSqlDatabase;

class NonceDb {
  public:
    // Insert a new nonce account. Returns false if address already exists.
    static bool insertNonceAccount(const QString& address, const QString& authority,
                                   const QString& nonceValue);

    // Get the nonce account for a given wallet authority.
    static std::optional<NonceAccountRecord> getByAuthorityRecord(const QString& authority);

    // Update the stored nonce value (after use or refresh).
    static bool updateNonceValue(const QString& address, const QString& newNonceValue);

    // Delete a nonce account record.
    static bool deleteNonceAccount(const QString& address);

    // Check if an authority has a nonce account.
    static bool hasNonceAccount(const QString& authority);

  private:
    static QSqlDatabase db();
};

#endif // NONCEDB_H
