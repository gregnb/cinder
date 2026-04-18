export default function MCP() {
  return (
    <>
      <h1>MCP (Agents)</h1>
      <p className="docs-subtitle">Let AI agents interact with your wallet through access-controlled tools.</p>

      <img src="/images/docs/19-agents-policies.png" alt="Access Policies" className="docs-screenshot" />

      <h2>What is MCP?</h2>
      <p>
        The <strong>Model Context Protocol</strong> lets AI assistants call external tools. Cinder exposes 60 wallet tools — agents can check balances, decode transactions, get swap quotes, and execute transactions with your approval.
      </p>

      <h2>Setting Up</h2>
      <ol>
        <li>Go to <strong>Agents → Access Policies</strong> and click <strong>Create Policy</strong></li>
        <li>Name the policy (e.g., "Trading Bot")</li>
        <li>Configure which tools the policy can use and which wallets it can access</li>
        <li>Go to the <strong>Configuration</strong> tab, select your policy, and copy the config snippet for your AI provider</li>
      </ol>

      <img src="/images/docs/21-agents-config.png" alt="Provider configuration" className="docs-screenshot" />

      <p>Supported providers:</p>
      <ul>
        <li><strong>Claude Desktop</strong> — Add the MCP server to your Claude config</li>
        <li><strong>Claude Code</strong> — Add to <code>.mcp.json</code> in your project</li>
        <li><strong>Codex (OpenAI)</strong> — Configure as an external tool</li>
      </ul>

      <h2>Approval Queue</h2>
      <img src="/images/docs/20-agents-approvals.png" alt="Approval queue" className="docs-screenshot" />
      <p>
        Operations that move funds (send, swap, burn, withdraw) always require your approval. When an agent tries one, it appears in the Approvals tab. You see exactly what's being requested and approve or reject with one click.
      </p>

      <h2>Activity Log</h2>
      <img src="/images/docs/22-agents-activity.png" alt="Activity log" className="docs-screenshot" />
      <p>Every MCP call is logged with timestamp, policy, tool, status, and result. Click any entry to see the full JSON.</p>

      <h2>Available Tools</h2>

      <h3>Read-only (no approval needed)</h3>
      <p>29 tools for querying state — balances, transactions, token info, prices, validators, stake accounts, contacts, network stats, blockhash, priority fees, ATA derivation, account inspection, transaction simulation.</p>

      <h3>Write (approval required)</h3>
      <table>
        <thead><tr><th>Tool</th><th>Action</th></tr></thead>
        <tbody>
          <tr><td><code>wallet_send_sol</code></td><td>Transfer SOL</td></tr>
          <tr><td><code>wallet_send_token</code></td><td>Transfer SPL tokens</td></tr>
          <tr><td><code>wallet_swap</code></td><td>Execute Jupiter swap</td></tr>
          <tr><td><code>wallet_stake_create</code></td><td>Delegate to validator</td></tr>
          <tr><td><code>wallet_stake_deactivate</code></td><td>Begin unstaking</td></tr>
          <tr><td><code>wallet_stake_withdraw</code></td><td>Withdraw inactive stake</td></tr>
          <tr><td><code>wallet_token_burn</code></td><td>Burn tokens</td></tr>
          <tr><td><code>wallet_token_close</code></td><td>Close empty account</td></tr>
          <tr><td><code>wallet_nonce_create</code></td><td>Create nonce account</td></tr>
          <tr><td><code>wallet_nonce_close</code></td><td>Close nonce account</td></tr>
          <tr><td><code>wallet_add_contact</code></td><td>Add to address book</td></tr>
          <tr><td><code>wallet_remove_contact</code></td><td>Remove contact</td></tr>
        </tbody>
      </table>

      <h2>Common Questions</h2>

      <h3>Can an agent drain my wallet without approval?</h3>
      <p>No. All fund-moving operations require manual approval in the Approvals tab. Read-only tools (checking balances, prices, etc.) don't need approval.</p>

      <h3>Can I restrict a policy to specific wallets?</h3>
      <p>Yes. Each policy can be scoped to a subset of your wallets.</p>

      <h3>Does the MCP server need to be running?</h3>
      <p>The server starts automatically when Cinder launches. The status is shown at the top of the Agents page with a Test Connection button.</p>
    </>
  )
}
