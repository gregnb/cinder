export default function Tokens() {
  return (
    <>
      <h1>Token-2022</h1>
      <p className="docs-subtitle">Create custom tokens with extensions, mint supply, burn tokens, and close accounts.</p>

      <h2>Create Token</h2>
      <img src="/images/docs/07-create-token.png" alt="Create Token form" className="docs-screenshot" />

      <ol>
        <li>Fill in <strong>Token Name</strong> and <strong>Symbol</strong></li>
        <li>Optionally add a description and image (can be uploaded to Arweave for permanent storage)</li>
        <li>Set <strong>Decimals</strong> (9 is SOL-like, 6 is USDC-like, 0 for NFT-like whole units)</li>
        <li>Set <strong>Initial Supply</strong> — minted directly to your wallet</li>
        <li>Toggle any extensions you want</li>
        <li>Review the cost breakdown and confirm</li>
      </ol>

      <h3>Extensions</h3>
      <table>
        <thead><tr><th>Extension</th><th>What it does</th><th>When to use it</th></tr></thead>
        <tbody>
          <tr><td><strong>Transfer Fee</strong></td><td>Charges a fee on every transfer (basis points + max cap)</td><td>Revenue-generating tokens, protocol fees</td></tr>
          <tr><td><strong>Non-Transferable</strong></td><td>Tokens can't be moved after minting</td><td>Soulbound tokens, credentials, badges</td></tr>
          <tr><td><strong>Mint Close Authority</strong></td><td>Lets you close the mint account later</td><td>Reclaiming rent after a token is no longer needed</td></tr>
          <tr><td><strong>Permanent Delegate</strong></td><td>One address can always transfer/burn from any holder</td><td>Compliance, clawback mechanisms</td></tr>
        </tbody>
      </table>

      <div className="docs-callout">
        <p>Extensions are set at creation time and <strong>cannot be added later</strong>. Choose carefully — a token without Transfer Fee can never have one added.</p>
      </div>

      <h2>Mint Tokens</h2>
      <p>Increase supply for tokens where you hold the mint authority. Select the token, enter the amount, confirm. New tokens go to your wallet.</p>

      <h2>Burn Tokens</h2>
      <p>Permanently destroy tokens from your account. This reduces total supply. Irreversible.</p>

      <h2>Close Token Accounts</h2>
      <p>
        Every token you hold has an Associated Token Account that costs ~0.002 SOL in rent. When the balance hits zero, you can close the account and reclaim that SOL. Cinder lists all empty accounts with checkboxes for batch closing.
      </p>

      <h3>Can I re-open a closed account?</h3>
      <p>Yes — receiving that token again automatically creates a new ATA. Closing just reclaims the rent; it doesn't block future transfers.</p>
    </>
  )
}
