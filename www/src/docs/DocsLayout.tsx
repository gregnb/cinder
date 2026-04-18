import { useEffect } from 'react'
import { NavLink, Outlet, useLocation } from 'react-router-dom'
import * as NavigationMenu from '@radix-ui/react-navigation-menu'
import './Docs.css'
import { LATEST_DMG_URL } from '../config/downloads'

const sections = [
  {
    title: 'Getting Started',
    items: [
      { to: '/docs/installation', label: 'Installation' },
      { to: '/docs/create-wallet', label: 'Create a Wallet' },
      { to: '/docs/hardware-wallets', label: 'Hardware Wallets' },
      { to: '/docs/security', label: 'Lock & Security' },
    ],
  },
  {
    title: 'Features',
    items: [
      { to: '/docs/overview', label: 'Interface Overview' },
      { to: '/docs/send-receive', label: 'Send & Receive' },
      { to: '/docs/swap', label: 'Swap' },
      { to: '/docs/staking', label: 'Staking' },
      { to: '/docs/tokens', label: 'Token-2022' },
      { to: '/docs/explorer', label: 'TX Explorer' },
      { to: '/docs/terminal', label: 'Terminal' },
      { to: '/docs/mcp', label: 'MCP (Agents)' },
      { to: '/docs/wallets', label: 'Wallet Management' },
    ],
  },
]

export default function DocsLayout() {
  const { pathname } = useLocation()

  useEffect(() => {
    window.scrollTo(0, 0)
  }, [pathname])

  return (
    <>
      <NavigationMenu.Root className="nav">
        <a href="/" className="nav-brand">
          <img src="/images/cinder-logo.png" alt="Cinder" />
          <span>Cinder</span>
        </a>
        <NavigationMenu.List className="nav-links">
          <NavigationMenu.Item>
            <NavigationMenu.Link className="nav-link" href="/#gallery">Gallery</NavigationMenu.Link>
          </NavigationMenu.Item>
          <NavigationMenu.Item>
            <NavigationMenu.Link className="nav-link" href="/#feature-0">Features</NavigationMenu.Link>
          </NavigationMenu.Item>
          <NavigationMenu.Item>
            <NavigationMenu.Link className="nav-link nav-link--active" href="/docs">Docs</NavigationMenu.Link>
          </NavigationMenu.Item>
          <NavigationMenu.Item>
            <NavigationMenu.Link className="nav-download-btn" href={LATEST_DMG_URL}>
              Download
            </NavigationMenu.Link>
          </NavigationMenu.Item>
          <NavigationMenu.Item>
            <NavigationMenu.Link className="nav-github" href="https://github.com/gregnb/cinder" target="_blank" rel="noopener noreferrer">
              <svg viewBox="0 0 98 96" fill="currentColor">
                <path fillRule="evenodd" clipRule="evenodd" d="M48.854 0C21.839 0 0 22 0 49.217c0 21.756 13.993 40.172 33.405 46.69 2.427.49 3.316-1.059 3.316-2.362 0-1.141-.08-5.052-.08-9.127-13.59 2.934-16.42-5.867-16.42-5.867-2.184-5.704-5.42-7.17-5.42-7.17-4.448-3.015.324-3.015.324-3.015 4.934.326 7.523 5.052 7.523 5.052 4.367 7.496 11.404 5.378 14.235 4.074.404-3.178 1.699-5.378 3.074-6.6-10.839-1.141-22.243-5.378-22.243-24.283 0-5.378 1.94-9.778 5.014-13.2-.485-1.222-2.184-6.275.486-13.038 0 0 4.125-1.304 13.426 5.052a46.97 46.97 0 0 1 12.214-1.63c4.125 0 8.33.571 12.213 1.63 9.302-6.356 13.427-5.052 13.427-5.052 2.67 6.763.97 11.816.485 13.038 3.155 3.422 5.015 7.822 5.015 13.2 0 18.905-11.404 23.06-22.324 24.283 1.78 1.548 3.316 4.481 3.316 9.126 0 6.6-.08 11.897-.08 13.526 0 1.304.89 2.853 3.316 2.364 19.412-6.52 33.405-24.935 33.405-46.691C97.707 22 75.788 0 48.854 0z" />
              </svg>
            </NavigationMenu.Link>
          </NavigationMenu.Item>
        </NavigationMenu.List>
      </NavigationMenu.Root>

      <div className="docs-layout">
        <nav className="docs-sidebar">
          {sections.map((section) => (
            <div key={section.title} className="docs-nav-section">
              <h3>{section.title}</h3>
              <ul>
                {section.items.map((item) => (
                  <li key={item.to}>
                    <NavLink
                      to={item.to}
                      className={({ isActive }) => isActive ? 'docs-nav-link docs-nav-link--active' : 'docs-nav-link'}
                    >
                      {item.label}
                    </NavLink>
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </nav>
        <main className="docs-content">
          <Outlet />
        </main>
      </div>
    </>
  )
}
