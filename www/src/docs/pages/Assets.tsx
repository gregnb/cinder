export default function Assets() {
  return (
    <>
      <h1>Assets</h1>
      <p className="docs-subtitle">View and manage your token portfolio.</p>

      <img src="/images/docs/02-assets.png" alt="Assets page" className="docs-screenshot" />

      <h2>Portfolio Overview</h2>
      <p>
        The top card shows your total portfolio value in USD with a historical chart. The asset count is shown in the page title (e.g., "Assets (15)").
      </p>

      <h2>Token Grid</h2>
      <p>
        All tokens with a non-zero balance are displayed in a scrollable card grid. Each card shows:
      </p>
      <ul>
        <li>Token icon (bundled for SOL, USDC, USDT, BONK, JTO; fetched from on-chain metadata for others)</li>
        <li>Token name and symbol</li>
        <li>Total value in USD</li>
        <li>Total holding and current price per token</li>
        <li><strong>Send</strong> and <strong>Receive</strong> buttons</li>
      </ul>

      <h2>Sorting & Search</h2>
      <p>Use the dropdown in the top-right to sort by:</p>
      <ul>
        <li>Value: High to Low / Low to High</li>
        <li>Name: A to Z / Z to A</li>
        <li>Holding: High to Low / Low to High</li>
        <li>Price: High to Low / Low to High</li>
      </ul>
      <p>The search bar filters tokens by name or symbol in real time.</p>

      <h2>Actions</h2>
      <p>
        Click <strong>Send</strong> on any card to open the send form pre-filled with that token. Click <strong>Receive</strong> to display your wallet address as a QR code.
      </p>
    </>
  )
}
