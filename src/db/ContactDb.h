#ifndef CONTACTDB_H
#define CONTACTDB_H

#include <QList>
#include <QString>
#include <optional>

struct ContactRecord {
    int id = 0;
    QString name;
    QString address;
    QString avatarPath;
    qint64 createdAt = 0;
};

class QSqlDatabase;

class ContactDb {
  public:
    // Insert a new contact. Returns false if address already exists.
    static bool insertContact(const QString& name, const QString& address);

    // Update an existing contact's name and address by id.
    static bool updateContact(int id, const QString& newName, const QString& newAddress);

    // Delete a contact by id.
    static bool deleteContact(int id);

    // Get all contacts, ordered by name (case-insensitive).
    // Optional search filter matches name or address via LIKE.
    static QList<ContactRecord> getAllRecords(const QString& search = {});

    // Get a single contact by id.
    static std::optional<ContactRecord> getByIdRecord(int id);

    // Check if an address already exists in the book.
    static bool hasAddress(const QString& address);

    // Count total contacts.
    static int countAll();

    // Look up a contact name by address. Returns empty QString if not found.
    static QString getNameByAddress(const QString& address);

    // Look up full contact record by address.
    static std::optional<ContactRecord> getByAddressRecord(const QString& address);

    // Avatar management.
    static bool setAvatarPath(int contactId, const QString& path);
    static QString getAvatarPath(int contactId);
    static QString avatarDir();
    static QString avatarFullPath(const QString& relativePath);

  private:
    static QSqlDatabase db();
};

#endif // CONTACTDB_H
