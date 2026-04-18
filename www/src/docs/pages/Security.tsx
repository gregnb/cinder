export default function Security() {
  return (
    <>
      <h1>Lock & Security</h1>
      <p className="docs-subtitle">Password protection, biometrics, and wallet encryption.</p>

      <h2>Password</h2>
      <p>
        On first setup you create a password that encrypts your wallet keys at rest. Every time you open Cinder, you'll enter this password to unlock.
      </p>

      <h2>Biometric Unlock</h2>
      <p>
        On macOS, you can enable <strong>Touch ID</strong> or <strong>Face ID</strong> in Settings to unlock without typing your password. The encryption key is stored in the Secure Enclave.
      </p>

      <h2>Auto-Lock</h2>
      <p>
        Cinder locks automatically when the app is closed. Your keys are only decrypted in memory while the app is unlocked.
      </p>

      <h2>Key Storage</h2>
      <p>
        Private keys and mnemonics are encrypted with your password using industry-standard encryption and stored in a local SQLite database. They are never transmitted over the network.
      </p>

      <h2>Revealing Keys</h2>
      <p>
        You can view your private key or recovery phrase from the Wallets page. The reveal has a <strong>30-second auto-hide</strong> countdown for safety. A copy button is provided but the data is never logged.
      </p>

      <div className="docs-callout">
        <p><strong>Never share your private key or recovery phrase.</strong> Cinder will never ask for it outside the app. Anyone with access to these can steal your funds.</p>
      </div>
    </>
  )
}
