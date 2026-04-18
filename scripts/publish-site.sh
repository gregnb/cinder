#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
WWW_DIR="$PROJECT_DIR/www"
DIST_DIR="$WWW_DIR/dist"
PAGES_PROJECT="${CINDER_PAGES_PROJECT:-cinderwallet}"
DRY_RUN=0
SKIP_BUILD=0

usage() {
    cat <<'USAGE'
Usage: ./scripts/publish-site.sh [options]

Options:
  --dry-run      build the site and print the deploy command without publishing
  --skip-build   reuse the existing www/dist output
  -h, --help     show this help

Environment:
  CINDER_PAGES_PROJECT   Cloudflare Pages project name (default: cinderwallet)
USAGE
}

require_command() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "❌ Required command not found: $cmd"
        exit 1
    fi
}

for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=1 ;;
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "❌ Unknown option: $arg"
            usage
            exit 1
            ;;
    esac
done

if [[ ! -d "$WWW_DIR" ]]; then
    echo "❌ Website directory not found: $WWW_DIR"
    exit 1
fi

require_command npm
require_command npx

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "🔨 Building website..."
    (
        cd "$WWW_DIR"
        npm run build
    )
else
    echo "⏭️  Skipping build (--skip-build)"
fi

if [[ ! -d "$DIST_DIR" ]]; then
    echo "❌ Dist directory not found: $DIST_DIR"
    exit 1
fi

echo "📦 Cloudflare Pages project: $PAGES_PROJECT"
echo "📁 Deploy directory: $DIST_DIR"

if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "🧪 Dry run: no deploy will be performed"
    echo "Deploy command:"
    echo "  cd \"$WWW_DIR\" && npx wrangler pages deploy dist --project-name \"$PAGES_PROJECT\""
    exit 0
fi

echo "🚀 Publishing website..."
(
    cd "$WWW_DIR"
    npx wrangler pages deploy dist --project-name "$PAGES_PROJECT"
)

echo ""
echo "══════════════════════════════════════════════════"
echo "  ✅ Website published"
echo "  Project: $PAGES_PROJECT"
echo "  URL: https://cinderwallet.io"
echo "══════════════════════════════════════════════════"
