export default function Terminal() {
  return (
    <>
      <h1>Terminal</h1>
      <p className="docs-subtitle">60+ commands for querying the network, managing tokens, and debugging transactions.</p>

      <img src="/images/docs/18-terminal-commands.png" alt="Terminal" className="docs-screenshot" />

      <h2>Basics</h2>
      <p>
        Type a command and press Enter. Up/down arrows navigate history. Tab completes commands and subcommands. Type <code>help</code> to list everything.
      </p>

      <h2>Wallet</h2>
      <pre><code>{`$ address
BDBy4xsVFkghEPTMnxXCNHY73VHkcWiT4qHop4t573VH

$ balance
Fetching balance...
18.6942 SOL

$ network
NETWORK
  Epoch:           944
  Epoch Progress:  37.8%
  Slot:            407,921,219
  Block Height:    100,047,127
  TPS:             3,010
SUPPLY
  Total:           622.80M SOL
  Circulating:     573.80M SOL
VALIDATORS
  Active:          777
  Delinquent:      0.0%`}</code></pre>

      <h2>Transactions</h2>
      <pre><code>{`send <address> <amount>          # Send SOL
history [n]                      # Last n transactions
tx classify <sig>                # What kind of transaction?
tx deltas <sig>                  # Who gained/lost what?
tx cu <sig>                      # Compute unit breakdown
tx decode <base58>               # Decode instruction data
tx simulate <base64_tx>          # Dry-run a transaction`}</code></pre>

      <h2>Tokens</h2>
      <pre><code>{`token info <mint>                # Metadata, supply, authorities
token accounts <address>         # All token accounts (live RPC)
token ata <owner> <mint>         # Derive ATA address
token send <mint> <to> <amount>  # Send SPL tokens
token burn <mint> <amount>       # Burn tokens
token close <mint>               # Close empty account, reclaim rent`}</code></pre>

      <h2>Staking</h2>
      <pre><code>{`stake list [address]              # Your stake accounts
stake create <vote> <amount>     # Delegate to validator
stake deactivate <stake_acct>    # Begin unstaking
stake withdraw <stake_acct> <n>  # Withdraw inactive stake
validator list [--top n]         # Network validators
validator info <vote_account>    # Validator details`}</code></pre>

      <h2>Swap & Price</h2>
      <pre><code>{`swap quote <in> <out> <amount>   # Jupiter swap quote
price <mint>                     # USD price lookup`}</code></pre>

      <h2>RPC & Nonce</h2>
      <pre><code>{`rpc set <url>                    # Switch RPC endpoint
rpc blockhash                    # Latest blockhash
rpc fees                         # Recent priority fees
nonce create                     # Create durable nonce account
nonce info                       # Show nonce value
nonce advance                    # Advance to new value`}</code></pre>

      <h2>Contacts & Portfolio</h2>
      <pre><code>{`contact list                     # All saved contacts
contact add <name> <address>     # Add contact
portfolio summary                # Latest snapshot
portfolio history [days]         # Daily values over time
portfolio lots <mint>            # Tax lot tracking`}</code></pre>

      <h2>Utilities</h2>
      <pre><code>{`encode base58 <hex>              # Hex → Base58
encode decode58 <b58>            # Base58 → Hex
encode hash <text>               # SHA-256
account rent <bytes>             # Rent exemption for N bytes
program fetch <id>               # Download/resolve IDL
db stats                         # Database table counts`}</code></pre>
    </>
  )
}
