export default function Staking() {
  return (
    <>
      <h1>Staking</h1>
      <p className="docs-subtitle">Delegate SOL to validators and earn rewards.</p>

      <h2>Choosing a Validator</h2>
      <img src="/images/docs/09-staking-validators.png" alt="Validator browser" className="docs-screenshot" />

      <p>The Validators tab shows every active validator with sortable columns. Things to consider:</p>
      <ul>
        <li><strong>APY</strong> — Higher is better, but varies by epoch. Look for consistently high APY.</li>
        <li><strong>Commission</strong> — The percentage the validator keeps from your rewards. 0% means you keep everything. 10% is common.</li>
        <li><strong>Stake</strong> — Very large validators are "superminority" (marked in the subtitle). Spreading stake to smaller validators improves network decentralization.</li>
        <li><strong>Version</strong> — Validators running outdated software may be less reliable.</li>
      </ul>

      <h2>How to Stake</h2>
      <ol>
        <li>Click a validator row to open the stake form</li>
        <li>Enter the amount of SOL to stake</li>
        <li>Confirm the transaction</li>
      </ol>
      <p>Cinder creates a stake account, transfers SOL, and delegates — all in one transaction.</p>

      <div className="docs-callout">
        <p><strong>Activation takes one full epoch</strong> (2–3 days). You won't earn rewards until the epoch after you delegate. Similarly, deactivation takes one epoch before you can withdraw.</p>
      </div>

      <h2>Managing Stakes</h2>
      <img src="/images/docs/10-staking-mystakes.png" alt="My Stakes" className="docs-screenshot" />

      <p>The My Stakes tab shows all your stake accounts. Each has one of four states:</p>
      <table>
        <thead><tr><th>State</th><th>Meaning</th><th>Action</th></tr></thead>
        <tbody>
          <tr><td><strong>Active</strong></td><td>Earning rewards</td><td>Click to deactivate (unstake)</td></tr>
          <tr><td><strong>Activating</strong></td><td>Waiting for next epoch</td><td>Wait — activates automatically</td></tr>
          <tr><td><strong>Deactivating</strong></td><td>Cooling down for one epoch</td><td>Wait — then withdraw</td></tr>
          <tr><td><strong>Inactive</strong></td><td>Ready to withdraw</td><td>Click to reclaim SOL</td></tr>
        </tbody>
      </table>

      <h2>Common Questions</h2>

      <h3>What's the minimum stake?</h3>
      <p>The Solana network requires a minimum of ~0.01 SOL for rent exemption on the stake account, but practically you'll want at least 1 SOL for rewards to be meaningful.</p>

      <h3>Can I stake to multiple validators?</h3>
      <p>Yes. Each delegation creates a separate stake account. You can have as many as you want.</p>

      <h3>When do rewards arrive?</h3>
      <p>Rewards are distributed automatically at the start of each epoch. They're added directly to your stake account balance — no action needed.</p>

      <h3>Can I add more SOL to an existing stake?</h3>
      <p>Not directly. Create a new stake delegation to the same validator. You'll have two stake accounts with that validator, both earning rewards.</p>
    </>
  )
}
