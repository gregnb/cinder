#ifndef ADDRESSBOOK_H
#define ADDRESSBOOK_H

#include <QString>

struct AddressBookContactView {
    int id = -1;
    QString name;
    QString address;
    QString avatarPath;
    QString createdDate;
};

#endif // ADDRESSBOOK_H
