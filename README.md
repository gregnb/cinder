<p align="center">
  <img width="320" alt="Cinder" src="https://github.com/user-attachments/assets/339e224b-8888-4ba6-8364-431a726b3e64" />
</p>

<h1 align="center">Cinder</h1>

<p align="center">
  A non-custodial desktop Solana wallet built with Qt and C++.
  <br />
  Send, swap, stake, and manage SPL tokens — with hardware wallet support, a built-in terminal, and an MCP server for AI agents.
</p>

<p align="center">
  <a href="https://cinderwallet.io">Website</a> · <a href="https://cinderwallet.io/docs">Documentation</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey" alt="Platform" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus" alt="C++20" />
  <img src="https://img.shields.io/badge/Qt-6-green?logo=qt" alt="Qt 6" />
  <img src="https://img.shields.io/badge/Solana-Mainnet-blueviolet?logo=solana" alt="Solana" />
</p>

---

<table align="center">
  <tr>
    <td><img alt="Dashboard" src="www/public/images/gallery/dashboard.png" /></td>
    <td><img alt="Send / Receive" src="www/public/images/gallery/send-receive.png" /></td>
    <td><img alt="Assets" src="www/public/images/gallery/assets.png" /></td>
  </tr>
  <tr>
    <td><img alt="Wallets" src="www/public/images/gallery/wallets.png" /></td>
    <td><img alt="Staking" src="www/public/images/gallery/staking-1.png" /></td>
    <td><img alt="Transaction Details" src="www/public/images/gallery/tx-details-1.png" /></td>
  </tr>
  <tr>
    <td><img alt="Agents" src="www/public/images/gallery/agents-1.png" /></td>
    <td><img alt="Terminal" src="www/public/images/gallery/terminal.png" /></td>
    <td><img alt="Staking Validators" src="www/public/images/gallery/staking-2.png" /></td>
  </tr>
  <tr>
    <td><img alt="Agents Activity" src="www/public/images/gallery/agents-2.png" /></td>
    <td><img alt="Transaction Details Balances" src="www/public/images/gallery/tx-details-2.png" /></td>
    <td><img alt="Transaction Details Instructions" src="www/public/images/gallery/tx-details-3.png" /></td>
  </tr>
</table>

## Features



**Wallet**

- Send/receive SOL and SPL tokens (including Token-2022)
- Swap via Jupiter DEX aggregation with slippage controls
- Stake SOL — browse validators, delegate, withdraw
- Create, mint, and burn Token-2022 tokens with extensions
- Durable nonce support
- Biometric unlock (Touch ID) 

**AI Agents (MCP)**

- Model Context Protocol server with 60+ tools
- Read-only tools (balances, history, quotes) run without approval
- Fund-moving tools require explicit UI approval
- Policy-based access control per wallet
- Works with Claude, Claude Code, and Codex

**Hardware Wallets**

- Ledger (Nano S / X / S+)
- Trezor (Model T / Safe 3 / One)
- GridPlus Lattice1

**Misc**

- Address book
- Transaction explorer — decode instructions, balances, compute units, logs
- Built-in terminal with 40+ commands

## Tech Stack

| Layer    | Technology                                       |
| -------- | ------------------------------------------------ |
| UI       | Qt 6 Widgets, Qt Charts                          |
| Language | C++20                                            |
| Build    | CMake 3.16+                                      |
| Crypto   | libsodium (Ed25519, Argon2id, XSalsa20-Poly1305) |
| Database | SQLite (Qt Sql) with versioned migrations        |
| RPC      | Qt Network — Solana JSON-RPC, Jupiter API        |
| Hardware | HIDAPI, libusb-1.0                               |
| QR Codes | libqrencode                                      |
| macOS    | Cocoa, LocalAuthentication, Security frameworks  |
| Website  | React, TypeScript, Vite, Cloudflare Pages        |

## Building

### Prerequisites

- CMake 3.16+
- C++20 compiler
- Qt 6.x (Widgets, Charts, Network, Sql, Concurrent, Svg)
- libsodium
- libqrencode

### macOS

```bash
brew install qt@6 libsodium qrencode hidapi libusb

mkdir build && cd build
cmake ..
cmake --build .

# Run
open Cinder.app
```

### Linux (Ubuntu/Debian)

```bash
sudo apt install qt6-base-dev qt6-charts-dev libsodium-dev libqrencode-dev \
                 libhidapi-dev libusb-1.0-0-dev cmake build-essential

mkdir build && cd build
cmake ..
cmake --build .

./Cinder
```

### Windows

1. Install [Qt 6](https://www.qt.io/download), [CMake](https://cmake.org/download/), and Visual Studio 2019+
2. Install vcpkg dependencies: `libsodium`, `qrencode`, `hidapi`, `libusb`

```bash
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="C:/Qt/6.x/msvc2019_64" ..
cmake --build . --config Release
```

### Development rebuild

```bash
# Kills any running instance, rebuilds the app target only, and launches
bash build-and-run.sh

# If you intentionally want to rebuild tests and other targets too
bash build-and-run.sh --all-targets
```

## Security

- **Non-custodial** — private keys never leave your machine
- **Encrypted storage** — Argon2id key derivation, XSalsa20-Poly1305 authenticated encryption
- **Hardware wallet signing** — transactions signed on-device
- **Biometric unlock** — macOS Touch ID with Keychain integration

## License

[MIT](LICENSE)
