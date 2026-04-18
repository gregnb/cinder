#!/bin/bash

# Cinder - Build, sign, notarize, and package a release DMG
# Usage:
#   ./scripts/build-dmg.sh                 # build + sign + package DMG
#   ./scripts/build-dmg.sh --skip-build    # package existing build only
#   ./scripts/build-dmg.sh --skip-notarize # skip notarization/stapling

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR/.."
cd "$PROJECT_DIR"

APP_NAME="Cinder"
BUILD_DIR="build-release"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
CODESIGN_IDENTITY="${CINDER_CODESIGN_IDENTITY:-${CINDER_SIGNING_IDENTITY:-}}"
QT_PREFIX="${CINDER_QT_PREFIX:-$(brew --prefix qt@6)}"
MACDEPLOYQT_BIN="${CINDER_MACDEPLOYQT:-$QT_PREFIX/bin/macdeployqt}"

# Extract version from CMakeLists.txt
VERSION=$(grep 'project(Cinder VERSION' CMakeLists.txt | sed 's/.*VERSION \([^ ]*\).*/\1/')
DMG_NAME="${APP_NAME}-${VERSION}-macOS"
DMG_DIR="$BUILD_DIR/dmg"
DMG_PATH="$BUILD_DIR/$DMG_NAME.dmg"

SKIP_BUILD=0
SKIP_NOTARIZE=0
NOTARIZATION_PERFORMED=0
for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        --skip-notarize) SKIP_NOTARIZE=1 ;;
        --help|-h)
            echo "Usage: $0 [--skip-build] [--skip-notarize]"
            echo "  default:         clean Release build + sign + package DMG + notarize if configured"
            echo "  --skip-build:    skip build, just re-package existing .app"
            echo "  --skip-notarize: skip notarization and stapling"
            echo ""
            echo "Environment:"
            echo "  CINDER_CODESIGN_IDENTITY   override signing identity"
            echo "  CINDER_SIGNING_IDENTITY    fallback signing identity alias"
            echo "  CINDER_QT_PREFIX           override Qt prefix used for macdeployqt"
            echo "  CINDER_MACDEPLOYQT         override macdeployqt binary path"
            echo "  CINDER_NOTARY_PROFILE      notarytool keychain profile name"
            echo "  CINDER_NOTARY_APPLE_ID     Apple ID for notarytool"
            echo "  CINDER_NOTARY_TEAM_ID      Team ID for notarytool"
            echo "  CINDER_NOTARY_PASSWORD     app-specific password for notarytool"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

require_command() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "❌ Required command not found: $cmd"
        exit 1
    fi
}

notarization_configured() {
    if [[ -n "${CINDER_NOTARY_PROFILE:-}" ]]; then
        return 0
    fi
    if [[ -n "${CINDER_NOTARY_APPLE_ID:-}" && -n "${CINDER_NOTARY_TEAM_ID:-}" && -n "${CINDER_NOTARY_PASSWORD:-}" ]]; then
        return 0
    fi
    return 1
}

run_notarytool_submit() {
    if [[ -n "${CINDER_NOTARY_PROFILE:-}" ]]; then
        xcrun notarytool submit "$DMG_PATH" --keychain-profile "$CINDER_NOTARY_PROFILE" --wait
        return
    fi

    xcrun notarytool submit "$DMG_PATH" \
        --apple-id "$CINDER_NOTARY_APPLE_ID" \
        --team-id "$CINDER_NOTARY_TEAM_ID" \
        --password "$CINDER_NOTARY_PASSWORD" \
        --wait
}

verify_app_signature() {
    echo "🔍 Verifying app signature..."
    codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"
    codesign -dv --verbose=2 "$APP_BUNDLE" >/dev/null 2>&1
    echo "✅ App signature is valid"

    local spctl_output
    if spctl_output=$(spctl --assess --type execute --verbose=4 "$APP_BUNDLE" 2>&1); then
        echo "✅ Gatekeeper accepts app bundle"
        echo "$spctl_output" | head -3
    else
        echo "ℹ️  Gatekeeper app assessment:"
        echo "$spctl_output" | head -3
    fi
}

verify_dmg_signature() {
    echo "🔍 Verifying DMG signature..."
    codesign --verify --strict --verbose=4 "$DMG_PATH"
    codesign -dv --verbose=2 "$DMG_PATH" >/dev/null 2>&1
    echo "✅ DMG signature is valid"
}

verify_notarized_dmg() {
    echo "🔍 Verifying notarization..."
    xcrun stapler validate "$DMG_PATH"
    spctl --assess --type open --context context:primary-signature --verbose=4 "$DMG_PATH"
    echo "✅ DMG notarization and Gatekeeper assessment passed"
}

require_command cmake
require_command codesign
require_command ditto
require_command hdiutil
require_command xcrun

if [[ ! -x "$MACDEPLOYQT_BIN" ]]; then
    echo "❌ macdeployqt not found at $MACDEPLOYQT_BIN"
    echo "   Set CINDER_MACDEPLOYQT or CINDER_QT_PREFIX."
    exit 1
fi

# ── Build ──────────────────────────────────────────────
if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "🧹 Cleaning release build directory..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    echo "⚙️  Configuring Release build..."
    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
        -DCINDER_CODESIGN_IDENTITY="$CODESIGN_IDENTITY"

    echo "🔨 Building..."
    cmake --build "$BUILD_DIR" --config Release --target Cinder --parallel "$(sysctl -n hw.ncpu)"
fi

if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "❌ $APP_BUNDLE not found. Run without --skip-build first."
    exit 1
fi

# ── Bundle Qt frameworks ───────────────────────────────
echo "📦 Bundling Qt frameworks with macdeployqt..."
"$MACDEPLOYQT_BIN" "$APP_BUNDLE" -verbose=1 2>&1 | tail -5

# ── Code sign ──────────────────────────────────────────
if [[ -z "$CODESIGN_IDENTITY" ]]; then
    echo "❌ No signing identity configured."
    echo "   Set CINDER_CODESIGN_IDENTITY or CINDER_SIGNING_IDENTITY."
    exit 1
fi

echo "🔏 Signing app bundle with: $CODESIGN_IDENTITY"

ENTITLEMENTS_PATH="$BUILD_DIR/generated/Cinder.entitlements"
if [[ -f "$ENTITLEMENTS_PATH" ]]; then
    codesign --force --sign "$CODESIGN_IDENTITY" --deep --options runtime \
        --entitlements "$ENTITLEMENTS_PATH" "$APP_BUNDLE"
else
    codesign --force --sign "$CODESIGN_IDENTITY" --deep --options runtime "$APP_BUNDLE"
fi
verify_app_signature

# ── Create DMG ─────────────────────────────────────────
echo "💿 Creating DMG..."

rm -rf "$DMG_DIR" "$DMG_PATH"
mkdir -p "$DMG_DIR"
ditto "$APP_BUNDLE" "$DMG_DIR/$APP_NAME.app"
ln -s /Applications "$DMG_DIR/Applications"

hdiutil create -volname "$APP_NAME" \
    -srcfolder "$DMG_DIR" \
    -ov -format UDZO \
    "$DMG_PATH"

codesign --force --sign "$CODESIGN_IDENTITY" "$DMG_PATH"
rm -rf "$DMG_DIR"
verify_dmg_signature

# ── Notarize + staple ──────────────────────────────────
if [[ "$SKIP_NOTARIZE" -eq 1 ]]; then
    echo "⏭️  Skipping notarization (--skip-notarize)"
elif notarization_configured; then
    echo "📝 Submitting DMG for notarization..."
    run_notarytool_submit

    echo "📌 Stapling notarization ticket..."
    xcrun stapler staple "$DMG_PATH"
    verify_notarized_dmg
    NOTARIZATION_PERFORMED=1
else
    echo "⚠️  Notarization skipped: no notarytool credentials configured."
    echo "   Set CINDER_NOTARY_PROFILE or CINDER_NOTARY_APPLE_ID/CINDER_NOTARY_TEAM_ID/CINDER_NOTARY_PASSWORD."
fi

DMG_SIZE=$(du -h "$DMG_PATH" | cut -f1)
echo ""
echo "══════════════════════════════════════════════════"
echo "  ✅ $DMG_NAME.dmg ($DMG_SIZE)"
echo "  📍 $DMG_PATH"
if [[ "$NOTARIZATION_PERFORMED" -eq 1 ]]; then
    echo "  🔏 Signed and notarized"
else
    echo "  🔏 Signed only (not notarized)"
fi
echo "══════════════════════════════════════════════════"
