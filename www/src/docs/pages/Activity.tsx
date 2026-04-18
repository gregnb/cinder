export default function Activity() {
  return (
    <>
      <h1>Activity</h1>
      <p className="docs-subtitle">Full transaction history with advanced filtering, sorting, and pagination.</p>

      <img src="/images/docs/11-activity.png" alt="Activity page" className="docs-screenshot" />

      <h2>Transaction List</h2>
      <p>
        A virtual-scrolling list of all your transactions, optimized for large histories. Each row shows:
      </p>
      <ul>
        <li>Type icon and activity badge (Send, Receive, Swap, Stake, etc.)</li>
        <li>From and To addresses (clickable)</li>
        <li>Amount with color coding (green for incoming, red for outgoing)</li>
        <li>Token symbol</li>
        <li>Relative timestamp (auto-refreshes every 60 seconds)</li>
      </ul>
      <p>Click any row to open it in the TX Explorer.</p>

      <h2>Filters</h2>
      <p>Filter transactions by:</p>
      <ul>
        <li>Signature (text search)</li>
        <li>Time range (from/to date picker)</li>
        <li>Action type (Send, Receive, Swap, Stake, etc.)</li>
        <li>From or To address</li>
        <li>Amount range (min/max)</li>
        <li>Token</li>
      </ul>
      <p>Active filters appear as chips with clear buttons. Click "Clear all" to reset.</p>

      <h2>Sorting</h2>
      <p>Click column headers to sort ascending or descending. Sort icons show the current direction.</p>

      <h2>Pagination</h2>
      <p>Navigate through pages with First/Previous/Next/Last buttons. A counter shows "Showing X of Y" records. Page size is configurable from the dropdown.</p>

      <h2>Sync Status</h2>
      <p>A badge in the header shows when a background transaction backfill is running, with a spinner animation during sync.</p>
    </>
  )
}
