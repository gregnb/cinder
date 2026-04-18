export default function CreateWallet() {
  return (
    <>
      <h1>Create a Wallet</h1>
      <p className="docs-subtitle">Set up a new wallet or import an existing one.</p>

      <h2>New Mnemonic Wallet</h2>
      <p>
        Generate a 12 or 24-word recovery phrase. Cinder uses BIP-39 mnemonics with BIP-44 HD derivation at path <code>m/44'/501'/0'/0'</code>.
      </p>
      <div className="docs-callout">
        <p><strong>Write down your recovery phrase.</strong> It cannot be recovered if lost. Anyone with access to it controls your funds.</p>
      </div>
      <ol>
        <li>Click <strong>Create Wallet</strong> on the welcome screen</li>
        <li>Choose 12 or 24 words</li>
        <li>Write down the phrase and confirm it</li>
        <li>Set a password to encrypt the wallet</li>
        <li>Give the wallet a name</li>
      </ol>

      <h2>Import from Private Key</h2>
      <p>
        Paste a Base58-encoded secret key to import a wallet directly. This is useful for migrating from other wallets or CLI-generated keypairs.
      </p>

      <h2>Import from Recovery Phrase</h2>
      <p>
        Enter an existing 12 or 24-word BIP-39 mnemonic. Cinder derives the same address as Phantom, Solflare, and other Solana wallets using the standard derivation path.
      </p>

      <h2>Derived Accounts</h2>
      <p>
        From any mnemonic wallet, you can create additional derived accounts by incrementing the account index. Each derived account has a unique address but shares the same recovery phrase.
      </p>

      <h2>Wallet Avatar & Name</h2>
      <p>
        Each wallet can have a custom name and avatar image to help you distinguish between multiple wallets at a glance.
      </p>
    </>
  )
}
