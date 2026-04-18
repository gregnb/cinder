#!/bin/bash

# Cinder - Incremental Build and Run Script
# Usage:
#   ./build-and-run.sh                  # incremental app rebuild + run
#   ./build-and-run.sh --clean          # full clean app rebuild + run
#   ./build-and-run.sh --all-targets    # rebuild full target graph + run
#   ./build-and-run.sh --build-only     # rebuild only; do not launch

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

set -euo pipefail

CLEAN_BUILD=0
BUILD_ALL_TARGETS=0
BUILD_ONLY=0

for arg in "$@"; do
    case "$arg" in
        --clean|-c)
            CLEAN_BUILD=1
            ;;
        --all-targets|-a)
            BUILD_ALL_TARGETS=1
            ;;
        --build-only)
            BUILD_ONLY=1
            ;;
        --help|-h)
            echo "Usage: $0 [--clean|-c] [--all-targets|-a] [--build-only]"
            echo "  default: incremental app rebuild + run"
            echo "  --clean: remove build dir first (full app rebuild)"
            echo "  --all-targets: build the full target graph instead of just Cinder"
            echo "  --build-only: build but do not stop or launch the app"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [--clean|-c] [--all-targets|-a] [--build-only]"
            exit 1
            ;;
    esac
done

if [[ "$CLEAN_BUILD" -eq 1 ]]; then
    echo "🧹 Cleaning build directory..."
    rm -rf build
fi

if [[ ! -d build ]]; then
    echo "📁 Creating build directory..."
    mkdir -p build
fi

echo "⚙️  Configuring with CMake..."
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" -DCMAKE_BUILD_TYPE=RelWithDebInfo

BUILD_TARGET_DESC="application target only"
BUILD_ARGS=(--target Cinder)

if [[ "$BUILD_ALL_TARGETS" -eq 1 ]]; then
    BUILD_TARGET_DESC="full target graph"
    BUILD_ARGS=()
fi

if [[ "$CLEAN_BUILD" -eq 1 ]]; then
    echo "🔨 Building ${BUILD_TARGET_DESC} (clean rebuild)..."
else
    echo "🔨 Building ${BUILD_TARGET_DESC} (incremental)..."
fi
cmake --build build "${BUILD_ARGS[@]}"

if [[ "$BUILD_ONLY" -eq 1 ]]; then
    echo "✅ Build complete (launch skipped)."
    exit 0
fi

echo "🛑 Stopping existing Cinder instances..."

# First ask the app to quit gracefully if it's running.
osascript -e 'tell application "Cinder" to quit' >/dev/null 2>&1 || true

# Ensure no stale binaries are left running.
pkill -x Cinder >/dev/null 2>&1 || true
pkill -f "Cinder.app/Contents/MacOS/Cinder" >/dev/null 2>&1 || true

# Wait briefly for processes to exit.
for _ in {1..30}; do
    if ! pgrep -x Cinder >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

echo "🚀 Launching Cinder..."
APP_EXECUTABLE="build/Cinder.app/Contents/MacOS/Cinder"
if [[ ! -x "$APP_EXECUTABLE" ]]; then
    echo "❌ App executable not found: $APP_EXECUTABLE"
    exit 1
fi

LAUNCH_LOG="/tmp/cinder-build-and-run.log"
rm -f "$LAUNCH_LOG"

open build/Cinder.app >/dev/null 2>"$LAUNCH_LOG"

echo "✅ Done!"
