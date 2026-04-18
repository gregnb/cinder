import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import './index.css'
import App from './App.tsx'
import DocsLayout from './docs/DocsLayout.tsx'
import DocsIndex from './docs/pages/DocsIndex.tsx'
import Installation from './docs/pages/Installation.tsx'
import CreateWallet from './docs/pages/CreateWallet.tsx'
import HardwareWallets from './docs/pages/HardwareWallets.tsx'
import Security from './docs/pages/Security.tsx'
import Overview from './docs/pages/Overview.tsx'
import SendReceive from './docs/pages/SendReceive.tsx'
import Swap from './docs/pages/Swap.tsx'
import Staking from './docs/pages/Staking.tsx'
import Tokens from './docs/pages/Tokens.tsx'
import Explorer from './docs/pages/Explorer.tsx'
import Terminal from './docs/pages/Terminal.tsx'
import MCP from './docs/pages/MCP.tsx'
import Wallets from './docs/pages/Wallets.tsx'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<App />} />
        <Route path="/docs" element={<DocsLayout />}>
          <Route index element={<DocsIndex />} />
          <Route path="installation" element={<Installation />} />
          <Route path="create-wallet" element={<CreateWallet />} />
          <Route path="hardware-wallets" element={<HardwareWallets />} />
          <Route path="security" element={<Security />} />
          <Route path="overview" element={<Overview />} />
          <Route path="send-receive" element={<SendReceive />} />
          <Route path="swap" element={<Swap />} />
          <Route path="staking" element={<Staking />} />
          <Route path="tokens" element={<Tokens />} />
          <Route path="explorer" element={<Explorer />} />
          <Route path="terminal" element={<Terminal />} />
          <Route path="mcp" element={<MCP />} />
          <Route path="wallets" element={<Wallets />} />
        </Route>
      </Routes>
    </BrowserRouter>
  </StrictMode>,
)
