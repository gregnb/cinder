#include "ContactResolver.h"
#include "db/ContactDb.h"
#include "services/AvatarCache.h"
#include "widgets/AddressLink.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>

bool ContactResolver::resolveLabel(const QString& address, QLabel* label,
                                   QHBoxLayout* parentLayout) {
    auto contact = ContactDb::getByAddressRecord(address);
    if (!contact || contact->name.isEmpty()) {
        return false;
    }

    label->setText(contact->name);

    if (!contact->avatarPath.isEmpty() && parentLayout) {
        QString fullPath = ContactDb::avatarFullPath(contact->avatarPath);
        QPixmap pm(fullPath);
        if (!pm.isNull()) {
            int idx = parentLayout->indexOf(label);
            if (idx >= 0) {
                QLabel* avatar = new QLabel();
                avatar->setFixedSize(16, 16);
                avatar->setStyleSheet("background: transparent; border: none;");
                qreal dpr = qApp->devicePixelRatio();
                avatar->setPixmap(AvatarCache::circleClip(pm, 16, dpr));
                parentLayout->insertWidget(idx, avatar);
            }
        }
    }

    return true;
}

bool ContactResolver::resolveAddressLink(AddressLink* link) {
    auto contact = ContactDb::getByAddressRecord(link->address());
    if (!contact || contact->name.isEmpty()) {
        return false;
    }

    link->setContactInfo(contact->name, ContactDb::avatarFullPath(contact->avatarPath));
    return true;
}
