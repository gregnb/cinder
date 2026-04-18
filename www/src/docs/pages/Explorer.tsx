export default function Explorer() {
  return (
    <>
      <h1>Transaction Explorer</h1>
      <p className="docs-subtitle">Decode any Solana transaction by its signature.</p>

      <img src="/images/docs/12-tx-lookup-input.png" alt="TX Lookup" className="docs-screenshot" />

      <h2>How to Use</h2>
      <ol>
        <li>Paste a transaction signature into the input field</li>
        <li>Click <strong>Analyze</strong> (or press Enter)</li>
        <li>Browse the tabs: Overview, Balances, Instructions, Logs</li>
      </ol>
      <p>You can also reach the explorer by clicking any transaction in the Activity page or Dashboard.</p>

      <h2>Tabs</h2>

      <h3>Overview</h3>
      <p>Result (success/failed), block, timestamp, signer, fees, compute units consumed, transaction version, and blockhash. If a durable nonce was used, the nonce account is shown.</p>

      <h3>Balances</h3>
      <p>Before and after balances for every account in the transaction. Deltas are highlighted — green for gains, red for losses. Useful for verifying exactly what moved.</p>

      <h3>Instructions</h3>
      <p>
        Each instruction decoded with program name and action. Anchor programs get full IDL decoding — field names, types, and values shown in a readable tree. Unknown programs show raw hex.
      </p>
      <p>Cinder auto-fetches IDLs from on-chain Anchor metadata. You can also manually fetch with <code>program fetch &lt;program_id&gt;</code> in the Terminal.</p>

      <h3>Logs</h3>
      <p>Raw program logs — invoke depth, log messages, errors. Color-coded for readability.</p>

      <h2>Address Highlighting</h2>
      <p>Hover any address to highlight every other reference to it across all tabs. Helpful for tracing how a specific account was involved.</p>
    </>
  )
}
