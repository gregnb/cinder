#include "AddressBookHandler.h"
#include "db/ContactDb.h"
#include "tx/Base58.h"
#include <QDateTime>
#include <QPixmap>

namespace {
    constexpr auto kContactCreatedDateFormat = "MMM dd, yyyy";
}

QList<AddressBookContactView> AddressBookHandler::listContacts(const QString& search) const {
    const QList<ContactRecord> records =
        search.isEmpty() ? ContactDb::getAllRecords() : ContactDb::getAllRecords(search);

    QList<AddressBookContactView> contacts;
    contacts.reserve(records.size());

    for (const auto& record : records) {
        AddressBookContactView contact;
        contact.id = record.id;
        contact.name = record.name;
        contact.address = record.address;
        contact.avatarPath = record.avatarPath;
        contact.createdDate =
            QDateTime::fromSecsSinceEpoch(record.createdAt).toString(kContactCreatedDateFormat);
        contacts.append(contact);
    }

    return contacts;
}

bool AddressBookHandler::canSaveContact(const QString& name, const QString& address) const {
    if (name.trimmed().isEmpty() || address.trimmed().isEmpty()) {
        return false;
    }
    return Base58::decode(address.trimmed()).size() == 32;
}

bool AddressBookHandler::saveContact(int editContactId, const QString& name,
                                     const QString& address) const {
    const QString trimmedName = name.trimmed();
    const QString trimmedAddress = address.trimmed();

    if (!canSaveContact(trimmedName, trimmedAddress)) {
        return false;
    }

    if (editContactId >= 0) {
        return ContactDb::updateContact(editContactId, trimmedName, trimmedAddress);
    }

    return ContactDb::insertContact(trimmedName, trimmedAddress);
}

bool AddressBookHandler::deleteContact(int contactId) const {
    if (contactId < 0) {
        return false;
    }

    return ContactDb::deleteContact(contactId);
}

bool AddressBookHandler::saveAvatar(int contactId, const QString& address,
                                    const QString& sourcePath, QString& relativePathOut) const {
    if (contactId < 0 || address.isEmpty()) {
        return false;
    }

    QPixmap pm(sourcePath);
    if (pm.isNull()) {
        return false;
    }
    if (pm.width() > 256 || pm.height() > 256) {
        pm = pm.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const QString relName = address + ".png";
    const QString destPath = ContactDb::avatarDir() + "/" + relName;
    if (!pm.save(destPath, "PNG")) {
        return false;
    }
    if (!ContactDb::setAvatarPath(contactId, relName)) {
        return false;
    }

    relativePathOut = relName;
    return true;
}
