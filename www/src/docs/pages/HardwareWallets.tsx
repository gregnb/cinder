export default function HardwareWallets() {
  return (
    <>
      <h1>Hardware Wallets</h1>
      <p className="docs-subtitle">Connect Ledger, Trezor, or Lattice1 to sign transactions on-device.</p>

      <h2>Supported Devices</h2>
      <table>
        <thead>
          <tr><th>Device</th><th>Transport</th><th>Derivation Path</th></tr>
        </thead>
        <tbody>
          <tr><td>Ledger Nano S / X / S+</td><td>HID (USB)</td><td><code>m/44'/501'/0'</code></td></tr>
          <tr><td>Trezor Model T / Safe 3</td><td>USB (libusb)</td><td><code>m/44'/501'/0'/0'</code></td></tr>
          <tr><td>Trezor One</td><td>HID</td><td><code>m/44'/501'/0'/0'</code></td></tr>
          <tr><td>Lattice1 (GridPlus)</td><td>USB</td><td><code>m/44'/501'/0'/0'</code></td></tr>
        </tbody>
      </table>

      <h2>Connecting a Device</h2>
      <ol>
        <li>Plug in your hardware wallet via USB</li>
        <li>Unlock the device and open the Solana app (Ledger) or navigate to the home screen (Trezor)</li>
        <li>In Cinder, go to <strong>Wallets</strong> and click <strong>Add Hardware Wallet</strong></li>
        <li>Select your device type and click <strong>Scan</strong></li>
        <li>Choose the account to import</li>
      </ol>

      <h2>Signing Transactions</h2>
      <p>
        When you send a transaction from a hardware wallet, Cinder builds the transaction locally and sends only the message to the device for signing. Your private keys never leave the hardware.
      </p>
      <p>
        You'll see a confirmation prompt on the device screen showing the transaction details. Approve on-device to complete the signature.
      </p>

      <h2>Ledger Notes</h2>
      <p>
        Make sure the <strong>Solana app</strong> is installed and open on your Ledger before scanning. Cinder communicates via the standard Ledger HID protocol.
      </p>

      <h2>Trezor Notes</h2>
      <p>
        Trezor Safe 3 and Model T use USB bulk transfers (not HID). Trezor One uses HID. Cinder auto-detects the transport. Solana signing requires firmware that supports message types 900–905.
      </p>

      <h2>Lattice1 Notes</h2>
      <p>
        The Lattice1 requires an initial pairing step. Once paired, Cinder stores the pairing key locally. SafeCard integration is supported for additional key storage.
      </p>
    </>
  )
}
