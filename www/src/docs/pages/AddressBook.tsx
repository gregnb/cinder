export default function AddressBook() {
  return (
    <>
      <h1>Address Book</h1>
      <p className="docs-subtitle">Save frequently used addresses with names and avatars.</p>

      <img src="/images/docs/24-address-book.png" alt="Address Book" className="docs-screenshot" />

      <h2>Adding a Contact</h2>
      <ol>
        <li>Go to Address Book and click <strong>+ Add Contact</strong></li>
        <li>Enter a name and Solana address</li>
        <li>Optionally upload an avatar image</li>
        <li>Click Save — the address is validated before saving</li>
      </ol>

      <h2>Using Contacts</h2>
      <p>
        When entering a recipient address in the Send form, saved contacts appear as autocomplete suggestions. Type a name or address to match.
      </p>

      <h2>Editing & Deleting</h2>
      <p>
        Click a contact to edit its name, address, or avatar. Delete with the trash button — a confirmation prompt prevents accidental removal.
      </p>

      <h2>Search</h2>
      <p>
        The search bar filters contacts in real-time by name or address.
      </p>
    </>
  )
}
