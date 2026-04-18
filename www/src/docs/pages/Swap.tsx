export default function Swap() {
  return (
    <>
      <h1>Swap</h1>
      <p className="docs-subtitle">Exchange any token pair through Jupiter DEX aggregation.</p>

      <img src="/images/docs/08-swap.png" alt="Swap page" className="docs-screenshot" />

      <h2>How to Swap</h2>
      <ol>
        <li>Select your <strong>From</strong> token and enter the amount (or click <strong>Max</strong>)</li>
        <li>Select the <strong>To</strong> token — Jupiter finds the best route automatically</li>
        <li>Review the quote: exchange rate, route, price impact, minimum received</li>
        <li>Click <strong>Swap</strong></li>
      </ol>
      <p>Use the flip button (arrows) to reverse the swap direction.</p>

      <h2>Understanding the Quote</h2>

      <h3>Price Impact</h3>
      <p>
        The percentage your trade moves the market price. Under 1% is normal. Above 5% (shown in red) means you're trading a large amount relative to the pool's liquidity — consider splitting into smaller swaps.
      </p>

      <h3>Minimum Received</h3>
      <p>
        The worst-case output after slippage. If the price moves beyond your slippage tolerance between quote and execution, the transaction reverts and you keep your tokens.
      </p>

      <h3>Slippage</h3>
      <p>
        Default is 0.5%. Increase to 1-2% for volatile tokens or during high congestion. Setting it too high risks getting a worse price; setting it too low risks failed transactions.
      </p>

      <h2>Common Questions</h2>

      <h3>Why did my swap fail?</h3>
      <p>Usually slippage — the price moved between quote and execution. Try increasing slippage tolerance or refreshing the quote. Swaps can also fail due to insufficient SOL for fees.</p>

      <h3>Where do quotes come from?</h3>
      <p>
        Jupiter aggregates across all major Solana DEXes (Raydium, Orca, Meteora, etc.) and routes through whichever path gives the best price. Cinder uses the <code>lite-api.jup.ag</code> endpoint — no API key required.
      </p>

      <h3>Is there a fee?</h3>
      <p>Cinder adds no fee on top of the swap. You pay only the Solana network fee and any DEX protocol fees built into the route.</p>
    </>
  )
}
