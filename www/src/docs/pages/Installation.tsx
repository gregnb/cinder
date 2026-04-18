import { LATEST_DMG_URL } from '../../config/downloads'

export default function Installation() {
  return (
    <>
      <h1>Installation</h1>
      <p className="docs-subtitle">Download and install Cinder on macOS.</p>

      <h2>System Requirements</h2>
      <ul>
        <li>macOS 12 (Monterey) or later</li>
        <li>Apple Silicon (M1/M2/M3/M4) or Intel</li>
        <li>200 MB disk space</li>
      </ul>

      <h2>Download</h2>
      <p>
        Download the latest <a href={LATEST_DMG_URL}><code>.dmg</code></a>. Open the disk
        image and drag Cinder into your Applications folder.
      </p>

      <h2>First Launch</h2>
      <p>
        On first launch macOS may show a Gatekeeper warning. Right-click the app and choose <strong>Open</strong> to bypass it. You'll be prompted to either create a new wallet or import an existing one.
      </p>

      <h2>Building from Source</h2>
      <p>Cinder is built with Qt 6 and CMake.</p>
      <pre><code>{`# Clone the repo
git clone https://github.com/gregnb/cinder.git
cd cinder

# Build and run
bash build-and-run.sh`}</code></pre>
      <p>
        The build script compiles the app and launches it. Requires Qt 6, CMake, and a C++17 compiler.
      </p>
    </>
  )
}
