export default function Settings() {
  return (
    <>
      <h1>Settings</h1>
      <p className="docs-subtitle">Configure language, security, and network endpoints.</p>

      <img src="/images/docs/25-settings-general.png" alt="Settings - General" className="docs-screenshot" />

      <h2>General</h2>
      <ul>
        <li><strong>Language</strong> — Select your preferred locale for the interface</li>
        <li><strong>Biometric Unlock</strong> — Enable Touch ID or Face ID on macOS to unlock without typing your password</li>
      </ul>

      <h2>RPC Endpoints</h2>
      <p>
        Manage your Solana RPC connections. Cinder ships with a default endpoint, but you can add custom ones for better performance or to use a private node.
      </p>
      <ul>
        <li><strong>Add</strong> — Enter a new RPC URL</li>
        <li><strong>Remove</strong> — Delete a saved endpoint</li>
        <li><strong>Switch</strong> — Click to make an endpoint active</li>
      </ul>
      <div className="docs-callout">
        <p>Some RPC methods have tight rate limits on free endpoints. If you see 429 errors, consider a dedicated provider like Helius, Triton, or QuickNode.</p>
      </div>

      <h2>About</h2>
      <img src="/images/docs/26-settings-about.png" alt="Settings - About" className="docs-screenshot" />
      <p>Shows the app version, Qt version, and build information.</p>
    </>
  )
}
