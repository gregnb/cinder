#ifndef CONTACTRESOLVER_H
#define CONTACTRESOLVER_H

class QLabel;
class QHBoxLayout;
class QString;
class AddressLink;

namespace ContactResolver {

    // Resolve a contact for a QLabel showing an address.
    // If a match is found in the address book:
    //   - Changes label text to the contact name
    //   - If parentLayout is provided and the contact has an avatar,
    //     inserts a 16px circle-clipped avatar before the label
    // Returns true if a contact was found.
    bool resolveLabel(const QString& address, QLabel* label, QHBoxLayout* parentLayout = nullptr);

    // Resolve a contact for an AddressLink widget.
    // If a match is found, calls setContactInfo() with the contact's
    // name and avatar path. The raw address is preserved for copy/tooltip.
    // Returns true if a contact was found.
    bool resolveAddressLink(AddressLink* link);

} // namespace ContactResolver

#endif // CONTACTRESOLVER_H
