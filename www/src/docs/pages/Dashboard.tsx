export default function Dashboard() {
  return (
    <>
      <h1>Dashboard</h1>
      <p className="docs-subtitle">Your wallet at a glance — balance, chart, network stats, and recent activity.</p>

      <img src="/images/docs/01-dashboard.png" alt="Dashboard" className="docs-screenshot" />

      <h2>Balance Card</h2>
      <p>
        The top-left card shows your total portfolio value in SOL and USD. A "Last updated" timestamp shows when data was last synced from the network.
      </p>

      <h2>Staking Summary</h2>
      <p>
        The top-right card shows your active stake accounts with the validator name, icon, status, and staked amount. The count in the title (e.g., "Staking (1)") reflects how many active stake accounts you have.
      </p>

      <h2>Portfolio Chart</h2>
      <p>
        A spline chart below the balance tracks your portfolio value over time. Data is collected from periodic snapshots and smoothed into up to 200 points.
      </p>

      <h2>Network Stats</h2>
      <p>The network card displays live Solana cluster data:</p>
      <ul>
        <li><strong>TPS</strong> — Current transactions per second</li>
        <li><strong>Slot</strong> — Current slot number</li>
        <li><strong>Epoch</strong> and <strong>Progress</strong> — Current epoch and completion percentage</li>
        <li><strong>Validators</strong> and <strong>Delinquent</strong> — Active count and delinquent percentage</li>
        <li><strong>Supply</strong> and <strong>Circulating</strong> — Total supply and circulating percentage</li>
        <li><strong>Inflation</strong> — Current inflation rate</li>
        <li><strong>Version</strong> — Solana cluster version</li>
      </ul>
      <p>A TPS bar chart at the bottom visualizes vote (green) vs. non-vote (purple) transactions over time.</p>

      <h2>Recent Activity</h2>
      <p>
        The last 10 transactions with type icons, descriptions, relative timestamps, and amounts. Green amounts are incoming, red are outgoing. Click any row to inspect it in the TX Explorer.
      </p>
    </>
  )
}
