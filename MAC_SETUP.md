# Mac Setup

This project uses Homebrew-managed dependencies on macOS.

## 1. Install Xcode Command Line Tools

```bash
xcode-select --install
```

## 2. Install Homebrew (if needed)

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

## 3. Install Required Dependencies

```bash
brew install git cmake ninja pkg-config qt@6 libsodium libqrencode zlib clang-format llvm
```

What each dependency is used for:
- `git`: clone/fork workflow.
- `cmake`: project configure/generate/build.
- `ninja`: faster CMake backend (recommended).
- `pkg-config`: discovery of `libsodium` and `libqrencode`.
- `qt@6`: Qt runtime/build modules used by the app (`Core`, `Widgets`, `Charts`, `Sql`, `Network`, `Concurrent`, `Test`, `LinguistTools`, `Svg`).
- `libsodium`: wallet crypto and secure memory handling.
- `libqrencode`: QR rendering support.
- `zlib`: compression dependency used by the build.
- `clang-format`: C++ formatter.
- `llvm`: provides `clang-tidy` (expected at `/opt/homebrew/opt/llvm/bin/clang-tidy`).

## 4. Optional JavaScript Tooling (for helper scripts)

```bash
brew install node
npm install
```

Use this only if you run helper scripts such as:
- `scripts/fetch_idls.mjs`
- `tests/generate_reference_payloads.mjs`
- `tests/generate_token2022_payloads.mjs`

## 5. Build and Run

```bash
bash ./build-and-run.sh

# Rebuild the full target graph only when you need test binaries refreshed too
bash ./build-and-run.sh --all-targets
```

## 6. Auto-fix C++ Style/Lint

```bash
bash ./scripts/fix-style.sh
```

The style/lint script:
- Formats with `clang-format`
- Applies fixable `clang-tidy` checks (including enforced braces on `if`/control statements)

## 7. Enable Git Pre-commit Hook

```bash
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit scripts/fix-style.sh scripts/fix-style-staged.sh
```

This hook runs on each commit and:
- formats staged C++ files with `clang-format`
- runs `clang-tidy --fix` on staged `.cpp` files
- re-stages any auto-fixes

## 8. Quick Verification

```bash
cmake --version
pkg-config --modversion libsodium
pkg-config --modversion libqrencode
clang-format --version
/opt/homebrew/opt/llvm/bin/clang-tidy --version
```
