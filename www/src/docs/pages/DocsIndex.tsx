import { Link } from 'react-router-dom'

export default function DocsIndex() {
  return (
    <>
      <h1>Cinder Documentation</h1>
      <p className="docs-subtitle">
        Cinder is a desktop Solana wallet built with Qt. Send, swap, stake, and manage SPL tokens — with hardware wallet support, a built-in terminal, and an MCP server for AI agents.
      </p>

      <h2>Getting Started</h2>
      <ul>
        <li><Link to="/docs/installation">Installation</Link> — Download and set up Cinder</li>
        <li><Link to="/docs/create-wallet">Create a Wallet</Link> — Mnemonic, private key, or import</li>
        <li><Link to="/docs/hardware-wallets">Hardware Wallets</Link> — Ledger, Trezor, Lattice1</li>
        <li><Link to="/docs/security">Lock & Security</Link> — Password, biometrics, key storage</li>
      </ul>

      <h2>Features</h2>
      <ul>
        <li><Link to="/docs/overview">Interface Overview</Link> — Quick tour of every screen</li>
        <li><Link to="/docs/send-receive">Send & Receive</Link> — Transfers, multi-recipient, priority fees, durable nonce</li>
        <li><Link to="/docs/swap">Swap</Link> — Jupiter DEX aggregation, slippage, price impact</li>
        <li><Link to="/docs/staking">Staking</Link> — Validator browser, delegate, withdraw, epoch timing</li>
        <li><Link to="/docs/tokens">Token-2022</Link> — Create with extensions, mint, burn, close accounts</li>
        <li><Link to="/docs/explorer">TX Explorer</Link> — Decode instructions, balances, compute units, logs</li>
        <li><Link to="/docs/terminal">Terminal</Link> — 60+ commands with examples</li>
        <li><Link to="/docs/mcp">MCP (Agents)</Link> — AI tools, access policies, approval flow</li>
        <li><Link to="/docs/wallets">Wallet Management</Link> — Multi-wallet, HD derivation, hardware</li>
      </ul>
    </>
  )
}
