export default function Overview() {
  return (
    <>
      <h1>Interface Overview</h1>
      <p className="docs-subtitle">A quick tour of the main screens in Cinder.</p>

      <h2>Dashboard</h2>
      <img src="/images/docs/01-dashboard.png" alt="Dashboard" className="docs-screenshot" />
      <p>Portfolio balance, staking summary, network stats, and recent transactions — all in one view.</p>

      <h2>Assets</h2>
      <img src="/images/docs/02-assets.png" alt="Assets" className="docs-screenshot" />
      <p>Token grid with balances, prices, and quick Send/Receive buttons. Sort by value, name, holding, or price.</p>

      <h2>Activity</h2>
      <img src="/images/docs/11-activity.png" alt="Activity" className="docs-screenshot" />
      <p>Full transaction history with filters (type, date, address, amount, token), sortable columns, and pagination.</p>

      <h2>Address Book</h2>
      <img src="/images/docs/24-address-book.png" alt="Address Book" className="docs-screenshot" />
      <p>Save contacts with names and avatars. They appear as autocomplete suggestions in the Send form.</p>

      <h2>Settings</h2>
      <img src="/images/docs/25-settings-general.png" alt="Settings" className="docs-screenshot" />
      <p>Language, biometric unlock (Touch ID / Face ID), and RPC endpoint management.</p>
    </>
  )
}
