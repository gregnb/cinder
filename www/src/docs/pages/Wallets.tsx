export default function Wallets() {
  return (
    <>
      <h1>Wallet Management</h1>
      <p className="docs-subtitle">Create, import, and manage multiple wallets.</p>

      <img src="/images/docs/23-wallets.png" alt="Wallets page" className="docs-screenshot" />

      <h2>Wallet Types</h2>
      <table>
        <thead><tr><th>Type</th><th>Source</th><th>Derivation</th></tr></thead>
        <tbody>
          <tr><td>Mnemonic</td><td>12 or 24-word phrase</td><td><code>m/44'/501'/n'/0'</code></td></tr>
          <tr><td>Private Key</td><td>Base58 secret key</td><td>Direct import</td></tr>
          <tr><td>Hardware</td><td>Ledger / Trezor / Lattice1</td><td>Device-specific</td></tr>
        </tbody>
      </table>

      <h2>Adding Wallets</h2>
      <p>Click <strong>+ Add Wallet</strong> to create a new mnemonic, import a private key, or connect a hardware wallet.</p>

      <h2>Derived Accounts</h2>
      <p>
        Click <strong>+ Add Account</strong> under a mnemonic wallet to derive additional addresses from the same recovery phrase. Each account uses the next index in the HD path. Useful for separating funds (e.g., trading vs. savings) while backing up with one phrase.
      </p>

      <h2>Switching Wallets</h2>
      <p>Click the wallet dropdown in the sidebar, or go to Wallets and click any row. The active wallet is used for all transactions, balance queries, and MCP tools.</p>

      <h2>Revealing Keys</h2>
      <p>
        On the wallet detail page, click <strong>Reveal Private Key</strong> or <strong>Reveal Recovery Phrase</strong>. The sensitive data shows for 30 seconds then auto-hides. A copy button is available but the data is never logged.
      </p>

      <h2>Removing a Wallet</h2>
      <div className="docs-callout">
        <p>Make sure you have your recovery phrase or private key backed up before removing. Deletion is permanent — Cinder cannot recover a removed wallet.</p>
      </div>
    </>
  )
}
