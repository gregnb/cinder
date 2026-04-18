export default function SendReceive() {
  return (
    <>
      <h1>Send & Receive</h1>
      <p className="docs-subtitle">Transfer SOL and SPL tokens with full control over fees, recipients, and transaction options.</p>

      <img src="/images/docs/03-send-card-grid.png" alt="New Transaction action cards" className="docs-screenshot" />

      <h2>Send Form</h2>
      <img src="/images/docs/04-send-form-filled.png" alt="Send form with recipient and amount" className="docs-screenshot" />

      <ol>
        <li>Choose <strong>Send SOL</strong> or <strong>Send Token</strong> from the action grid</li>
        <li>Select a token from the dropdown — your balance shows on the right</li>
        <li>Enter the recipient address (or start typing to autocomplete from your Address Book)</li>
        <li>Enter the amount — click <strong>Max</strong> to send your full balance minus fees</li>
        <li>Click <strong>Review Transaction</strong></li>
      </ol>

      <h3>Multi-Recipient</h3>
      <p>
        Click <strong>+ Add recipient</strong> to batch multiple transfers into one transaction. Each recipient gets their own address and amount fields. This saves on base fees compared to sending individually.
      </p>

      <h3>Priority Fee</h3>
      <p>
        <strong>Auto</strong> uses the median recent fee — good for most transactions. Switch to <strong>Custom</strong> to set your own micro-lamports per compute unit. Higher fees get prioritized during network congestion.
      </p>

      <h3>Durable Nonce</h3>
      <p>
        Normal transactions expire after ~60 seconds if not confirmed. Enable <strong>Durable Nonce</strong> to make the transaction valid indefinitely. Useful for:
      </p>
      <ul>
        <li>Offline signing workflows (sign now, broadcast later)</li>
        <li>Multi-party approval (pass a transaction around for co-signing)</li>
        <li>Scheduled transactions</li>
      </ul>
      <p>If you don't have a nonce account, Cinder creates one automatically.</p>

      <h2>Review & Simulation</h2>
      <img src="/images/docs/05-send-review.png" alt="Transaction review" className="docs-screenshot" />

      <p>
        The review page simulates the transaction on-chain before you sign. If the simulation fails (insufficient balance, invalid account, etc.), the error is shown here instead of wasting fees on a failed transaction.
      </p>
      <p>
        If the token has a <strong>transfer fee</strong> (Token-2022 extension), a warning banner shows the fee percentage and maximum. Recipients receive the amount minus this fee.
      </p>
      <p>Click <strong>Export</strong> to save the review as PDF or CSV for record-keeping.</p>

      <h2>Confirmation</h2>
      <img src="/images/docs/06-send-success.png" alt="Transaction success" className="docs-screenshot" />

      <p>
        After signing, Cinder polls the network until the transaction is confirmed. The success page shows the signature, block, and a <strong>View on Solscan</strong> link.
      </p>

      <h2>Common Questions</h2>

      <h3>What if I send to the wrong address?</h3>
      <p>Solana transactions are irreversible. Always double-check the recipient address on the review page before confirming.</p>

      <h3>Why does sending a token cost more than sending SOL?</h3>
      <p>If the recipient doesn't have an Associated Token Account (ATA) for that token, Cinder creates one automatically. This costs ~0.002 SOL in rent.</p>

      <h3>What does "Max" actually send?</h3>
      <p>For SOL, Max deducts the network fee and rent-exempt minimum so your account stays valid. For tokens, Max sends your entire token balance.</p>
    </>
  )
}
