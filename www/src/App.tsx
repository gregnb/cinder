import { useCallback, useEffect, useRef, useState } from 'react'
import * as NavigationMenu from '@radix-ui/react-navigation-menu'
import './App.css'
import { LATEST_DMG_URL } from './config/downloads'

function App() {
  const [activeSection, setActiveSection] = useState('hero')

  useEffect(() => {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            setActiveSection(entry.target.id)
          }
        })
      },
      { threshold: 0.15 }
    )

    document.querySelectorAll('section[id]').forEach((section) => {
      observer.observe(section)
    })

    return () => observer.disconnect()
  }, [])

  return (
    <div className="snap-container">
      <Nav activeSection={activeSection} />
      <Hero />
      <Gallery />
      {features.map((f, i) => (
        <FeatureSection key={i} feature={f} index={i} />
      ))}
    </div>
  )
}

function Nav({ activeSection }: { activeSection: string }) {
  const isFeature = activeSection.startsWith('feature-')

  return (
    <NavigationMenu.Root className="nav">
      <div className="nav-brand">
        <img src="/images/cinder-logo.png" alt="Cinder" />
        <span>Cinder</span>
      </div>
      <NavigationMenu.List className="nav-links">
        <NavigationMenu.Item>
          <NavigationMenu.Link
            className={`nav-link ${activeSection === 'gallery' ? 'nav-link--active' : ''}`}
            href="#gallery"
          >
            Gallery
          </NavigationMenu.Link>
        </NavigationMenu.Item>
        <NavigationMenu.Item>
          <NavigationMenu.Link
            className={`nav-link ${isFeature ? 'nav-link--active' : ''}`}
            href="#feature-0"
          >
            Features
          </NavigationMenu.Link>
        </NavigationMenu.Item>
        <NavigationMenu.Item>
          <NavigationMenu.Link className="nav-link" href="/docs">Docs</NavigationMenu.Link>
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
  )
}

function Hero() {
  return (
    <section id="hero" className="snap-section hero">
      <div className="hero-content">
        <h1>Desktop Solana wallet</h1>
        <p>
          Send, swap, stake, and manage SPL tokens. MCP and terminal. Hardware wallet support.
        </p>
        <a className="download-btn" href={LATEST_DMG_URL}>
          <AppleIcon />
          Download for Mac
          <svg className="download-chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <polyline points="6 9 12 15 18 9" />
          </svg>
        </a>
      </div>
      <div className="hero-image">
        <img src="/images/hero-flame.png" alt="Cinder flame" />
      </div>
    </section>
  )
}

const features = [
  {
    id: 'feature-0',
    title: 'Send & receive',
    subtitle: 'SOL and SPL tokens',
    description: 'Transfer SOL or any SPL token to one or multiple recipients in a single transaction. Review everything before signing — amounts, fees, priority — with full simulation.',
    image: '/images/feature-send.png',
    color: '#9945FF',
  },
  {
    id: 'feature-1',
    title: 'Swap tokens',
    subtitle: 'Powered by Jupiter',
    description: 'Exchange any token pair through Jupiter DEX aggregation. Live quotes, price impact, slippage controls, and auto-refreshing rates — all without leaving the app.',
    image: '/images/feature-swap.png',
    color: '#00D4FF',
  },
  {
    id: 'feature-2',
    title: 'Agent-ready',
    subtitle: 'Model Context Protocol',
    description: 'Cinder runs an MCP server so AI agents can query balances, build transactions, and request approvals — all governed by access policies you control. Your wallet becomes a tool any agent can use.',
    image: '/images/feature-agents.png',
    color: '#E040FB',
  },
  {
    id: 'feature-3',
    title: 'Built-in terminal',
    subtitle: 'Power-user CLI',
    description: 'Drop into a command line without leaving the wallet. Query validators, inspect transactions, check swap rates, analyze token accounts, and run network diagnostics — all from one prompt.',
    image: '/images/feature-terminal.png',
    color: '#22D3EE',
  },
  {
    id: 'feature-4',
    title: 'Stake SOL',
    subtitle: 'Browse validators, earn rewards',
    description: 'Explore validators sorted by APY, commission, and total stake. Delegate, deactivate, or withdraw in a few clicks. Track your staking positions from the dashboard.',
    image: '/images/feature-stake.png',
    color: '#14F195',
  },
  {
    id: 'feature-5',
    title: 'Hardware wallets',
    subtitle: 'Ledger, Trezor & Lattice1',
    description: 'Connect your hardware wallet over USB. Sign transactions on-device with full Solana support. Your keys never leave the hardware.',
    image: '/images/feature-hardware.png',
    color: '#8B5CF6',
  },
  {
    id: 'feature-6',
    title: 'Token-2022',
    subtitle: 'Create, mint & burn',
    description: 'Full Token-2022 support. Create custom tokens with extensions — transfer hooks, metadata pointers, mint close authority. Mint supply or burn tokens you own.',
    image: '/images/feature-token.png',
    color: '#F59E0B',
  },
  {
    id: 'feature-7',
    title: 'Transaction explorer',
    subtitle: 'Decode any transaction',
    description: 'Paste any signature to inspect a transaction top to bottom. See instructions decoded with IDL data, balance changes, compute units, program logs, and more.',
    image: '/images/feature-explorer.png',
    color: '#3B82F6',
  },
]

function FeatureSection({ feature, index }: { feature: typeof features[0]; index: number }) {
  const ref = useRef<HTMLElement>(null)
  const [visible, setVisible] = useState(false)
  const reversed = index % 2 === 1

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) setVisible(true)
      },
      { threshold: 0.2 }
    )
    if (ref.current) observer.observe(ref.current)
    return () => observer.disconnect()
  }, [])

  return (
    <section
      id={feature.id}
      ref={ref}
      className={`snap-section feature-section ${reversed ? 'feature-section--reversed' : ''} ${visible ? 'feature-section--visible' : ''}`}
    >
      <div className="feature-text">
        <span className="feature-subtitle" style={{ color: feature.color }}>{feature.subtitle}</span>
        <h2>{feature.title}</h2>
        <p>{feature.description}</p>
      </div>
      <div className="feature-visual">
        <div className="feature-image-wrap">
          <div className="feature-glow" style={{ background: feature.color }} />
          <img src={feature.image} alt={feature.title} />
        </div>
      </div>
    </section>
  )
}

const galleryImages = [
  { src: '/images/gallery/dashboard.png', alt: 'Dashboard' },
  { src: '/images/gallery/assets.png', alt: 'Assets' },
  { src: '/images/gallery/send-receive.png', alt: 'Send & Receive' },
  { src: '/images/gallery/staking-1.png', alt: 'Staking — Validators' },
  { src: '/images/gallery/staking-2.png', alt: 'Staking — My Stakes' },
  { src: '/images/gallery/terminal.png', alt: 'Terminal' },
  { src: '/images/gallery/agents-1.png', alt: 'MCP Agent Tools' },
  { src: '/images/gallery/agents-2.png', alt: 'MCP Agent Permissions' },
  { src: '/images/gallery/tx-details-1.png', alt: 'Transaction Details' },
  { src: '/images/gallery/tx-details-2.png', alt: 'Transaction Instructions' },
  { src: '/images/gallery/tx-details-3.png', alt: 'Transaction Logs' },
  { src: '/images/gallery/wallets.png', alt: 'Wallet Management' },
]

function Gallery() {
  const ref = useRef<HTMLElement>(null)
  const [visible, setVisible] = useState(false)
  const [lightboxIndex, setLightboxIndex] = useState<number | null>(null)

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => { if (entry.isIntersecting) setVisible(true) },
      { threshold: 0.1 }
    )
    if (ref.current) observer.observe(ref.current)
    return () => observer.disconnect()
  }, [])

  return (
    <>
      <section
        id="gallery"
        ref={ref}
        className={`snap-section gallery-section ${visible ? 'gallery-section--visible' : ''}`}
      >
        <h2 className="gallery-title">Gallery</h2>
        <div className="gallery-grid">
          {galleryImages.map((img, i) => (
            <button
              key={i}
              className="gallery-thumb"
              onClick={() => setLightboxIndex(i)}
              style={{ animationDelay: `${i * 0.04}s` }}
            >
              <img src={img.src} alt={img.alt} loading="lazy" />
            </button>
          ))}
        </div>
      </section>
      {lightboxIndex !== null && (
        <Lightbox
          images={galleryImages}
          index={lightboxIndex}
          onClose={() => setLightboxIndex(null)}
          onNavigate={setLightboxIndex}
        />
      )}
    </>
  )
}

function Lightbox({
  images,
  index,
  onClose,
  onNavigate,
}: {
  images: typeof galleryImages
  index: number
  onClose: () => void
  onNavigate: (i: number) => void
}) {
  const prev = useCallback(() => onNavigate((index - 1 + images.length) % images.length), [index, images.length, onNavigate])
  const next = useCallback(() => onNavigate((index + 1) % images.length), [index, images.length, onNavigate])

  useEffect(() => {
    const handleKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose()
      if (e.key === 'ArrowLeft') prev()
      if (e.key === 'ArrowRight') next()
    }
    window.addEventListener('keydown', handleKey)
    document.body.style.overflow = 'hidden'
    return () => {
      window.removeEventListener('keydown', handleKey)
      document.body.style.overflow = ''
    }
  }, [onClose, prev, next])

  return (
    <div className="lightbox-overlay" onClick={onClose}>
      <div className="lightbox-content" onClick={(e) => e.stopPropagation()}>
        <button className="lightbox-close" onClick={onClose}>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <line x1="18" y1="6" x2="6" y2="18" /><line x1="6" y1="6" x2="18" y2="18" />
          </svg>
        </button>
        <button className="lightbox-nav lightbox-nav--prev" onClick={prev}>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <polyline points="15 18 9 12 15 6" />
          </svg>
        </button>
        <img src={images[index].src} alt={images[index].alt} className="lightbox-img" />
        <button className="lightbox-nav lightbox-nav--next" onClick={next}>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <polyline points="9 6 15 12 9 18" />
          </svg>
        </button>
        <div className="lightbox-caption">
          {images[index].alt}
          <span className="lightbox-counter">{index + 1} / {images.length}</span>
        </div>
      </div>
    </div>
  )
}

function AppleIcon() {
  return (
    <svg className="apple-icon" viewBox="0 0 384 512" fill="currentColor">
      <path d="M318.7 268.7c-.2-36.7 16.4-64.4 50-84.8-18.8-26.9-47.2-41.7-84.7-44.6-35.5-2.8-74.3 20.7-88.5 20.7-15 0-49.4-19.7-76.4-19.7C63.3 141.2 4 184.8 4 273.5q0 39.3 14.4 81.2c12.8 36.7 59 126.7 107.2 125.2 25.2-.6 43-17.9 75.8-17.9 31.8 0 48.3 17.9 76.4 17.9 48.6-.7 90.4-82.5 102.6-119.3-65.2-30.7-61.7-90-61.7-91.9zm-56.6-164.2c27.3-32.4 24.8-61.9 24-72.5-24.1 1.4-52 16.4-67.9 34.9-17.5 19.8-27.8 44.3-25.6 71.9 26.1 2 49.9-11.4 69.5-34.3z" />
    </svg>
  )
}

export default App
