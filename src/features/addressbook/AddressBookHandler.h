#ifndef ADDRESSBOOKHANDLER_H
#define ADDRESSBOOKHANDLER_H

#include "models/AddressBook.h"
#include <QList>
#include <QString>

class AddressBookHandler {
  public:
    QList<AddressBookContactView> listContacts(const QString& search = {}) const;

    bool canSaveContact(const QString& name, const QString& address) const;
    bool saveContact(int editContactId, const QString& name, const QString& address) const;
    bool deleteContact(int contactId) const;

    bool saveAvatar(int contactId, const QString& address, const QString& sourcePath,
                    QString& relativePathOut) const;
};

#endif // ADDRESSBOOKHANDLER_H
