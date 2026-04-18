#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR/.."
cd "$PROJECT_DIR"

usage() {
    cat <<'USAGE'
Usage: ./scripts/publish-release.sh --version <version> --tag <tag> --dmg <path> --notes <path> --manifest <path> --md5 <path> --sha256 <path> [options]

Options:
  --dry-run       print planned GitHub/R2 actions only
  --skip-github   skip GitHub release creation/update
  --skip-r2       skip Cloudflare R2 upload/update
  -h, --help      show this help

R2 env vars:
  CINDER_R2_BUCKET           bucket name
  CINDER_R2_PUBLIC_BASE_URL  public base URL, e.g. https://downloads.example.com
USAGE
}

DRY_RUN=0
SKIP_GITHUB=0
SKIP_R2=0
VERSION=""
TAG=""
DMG_PATH=""
RELEASE_NOTES_PATH=""
MANIFEST_PATH=""
MD5_PATH=""
SHA256_PATH=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --tag) TAG="$2"; shift 2 ;;
        --dmg) DMG_PATH="$2"; shift 2 ;;
        --notes) RELEASE_NOTES_PATH="$2"; shift 2 ;;
        --manifest) MANIFEST_PATH="$2"; shift 2 ;;
        --md5) MD5_PATH="$2"; shift 2 ;;
        --sha256) SHA256_PATH="$2"; shift 2 ;;
        --dry-run) DRY_RUN=1; shift ;;
        --skip-github) SKIP_GITHUB=1; shift ;;
        --skip-r2) SKIP_R2=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "❌ Unknown option: $1"; usage; exit 1 ;;
    esac
done

require_command() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "❌ Required command not found: $cmd"
        exit 1
    fi
}

require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo "❌ Required file not found: $path"
        exit 1
    fi
}

for value in "$VERSION" "$TAG" "$DMG_PATH" "$RELEASE_NOTES_PATH" "$MANIFEST_PATH" "$MD5_PATH" "$SHA256_PATH"; do
    if [[ -z "$value" ]]; then
        echo "❌ Missing required arguments"
        usage
        exit 1
    fi
done

require_file "$RELEASE_NOTES_PATH"
require_file "$MANIFEST_PATH"
if [[ "$DRY_RUN" -eq 0 ]]; then
    require_file "$DMG_PATH"
    require_file "$MD5_PATH"
    require_file "$SHA256_PATH"
fi

DMG_NAME=$(basename "$DMG_PATH")
MD5_NAME=$(basename "$MD5_PATH")
SHA256_NAME=$(basename "$SHA256_PATH")
MANIFEST_NAME=$(basename "$MANIFEST_PATH")
LATEST_DMG_NAME="Cinder-latest-macOS.dmg"
LATEST_MD5_NAME="${LATEST_DMG_NAME}.md5"
LATEST_SHA256_NAME="${LATEST_DMG_NAME}.sha256"
LATEST_MANIFEST_NAME="release.json"

publish_github() {
    require_command gh
    if ! gh auth status >/dev/null 2>&1; then
        echo "❌ GitHub CLI is not authenticated"
        exit 1
    fi

    local current_branch
    current_branch=$(git branch --show-current)

    if git tag -l "$TAG" | grep -q "$TAG"; then
        echo "🏷  Tag $TAG already exists"
    else
        echo "🏷  Creating tag $TAG"
        git tag -a "$TAG" -m "Release $VERSION"
    fi

    echo "🚀 Pushing $current_branch and $TAG"
    git push origin "$current_branch"
    git push origin "$TAG"

    if gh release view "$TAG" >/dev/null 2>&1; then
        echo "📝 Updating existing GitHub release $TAG"
        gh release upload "$TAG" \
            "$DMG_PATH" "$MD5_PATH" "$SHA256_PATH" "$MANIFEST_PATH" \
            --clobber
        gh release edit "$TAG" \
            --title "Cinder $TAG" \
            --notes-file "$RELEASE_NOTES_PATH" \
            --latest
    else
        echo "📝 Creating GitHub release $TAG"
        gh release create "$TAG" \
            --title "Cinder $TAG" \
            --notes-file "$RELEASE_NOTES_PATH" \
            --latest \
            "$DMG_PATH" "$MD5_PATH" "$SHA256_PATH" "$MANIFEST_PATH"
    fi

    gh release view "$TAG" --json url -q .url
}

publish_r2() {
    require_command wrangler

    local bucket="${CINDER_R2_BUCKET:-}"
    local public_base="${CINDER_R2_PUBLIC_BASE_URL:-}"
    if [[ -z "$bucket" || -z "$public_base" ]]; then
        echo "❌ R2 publishing requires CINDER_R2_BUCKET and CINDER_R2_PUBLIC_BASE_URL"
        exit 1
    fi

    local release_prefix="releases"
    local latest_prefix="latest"
    local release_cache_control="public, max-age=31536000, immutable"
    local latest_cache_control="no-cache, no-store, must-revalidate"

    echo "☁️  Uploading release artifacts to R2 bucket $bucket"
    wrangler r2 object put "$bucket/$release_prefix/$DMG_NAME" --file "$DMG_PATH" --remote \
        --content-type "application/x-apple-diskimage" --cache-control "$release_cache_control"
    wrangler r2 object put "$bucket/$release_prefix/$MD5_NAME" --file "$MD5_PATH" --remote \
        --content-type "text/plain; charset=utf-8" --cache-control "$release_cache_control"
    wrangler r2 object put "$bucket/$release_prefix/$SHA256_NAME" --file "$SHA256_PATH" --remote \
        --content-type "text/plain; charset=utf-8" --cache-control "$release_cache_control"
    wrangler r2 object put "$bucket/$release_prefix/${VERSION}.json" --file "$MANIFEST_PATH" --remote \
        --content-type "application/json; charset=utf-8" --cache-control "$release_cache_control"

    wrangler r2 object put "$bucket/$latest_prefix/$LATEST_DMG_NAME" --file "$DMG_PATH" --remote \
        --content-type "application/x-apple-diskimage" --cache-control "$latest_cache_control"
    wrangler r2 object put "$bucket/$latest_prefix/$LATEST_MD5_NAME" --file "$MD5_PATH" --remote \
        --content-type "text/plain; charset=utf-8" --cache-control "$latest_cache_control"
    wrangler r2 object put "$bucket/$latest_prefix/$LATEST_SHA256_NAME" --file "$SHA256_PATH" --remote \
        --content-type "text/plain; charset=utf-8" --cache-control "$latest_cache_control"
    wrangler r2 object put "$bucket/$latest_prefix/$LATEST_MANIFEST_NAME" --file "$MANIFEST_PATH" --remote \
        --content-type "application/json; charset=utf-8" --cache-control "$latest_cache_control"

    echo "$public_base/$release_prefix/$DMG_NAME"
    echo "$public_base/$latest_prefix/$LATEST_DMG_NAME"
}

if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "🧪 Dry run: no git, GitHub, or R2 state will be changed"
    if [[ "$SKIP_GITHUB" -eq 0 ]]; then
        echo "GitHub release plan:"
        echo "  tag: $TAG"
        echo "  title: Cinder $TAG"
        echo "  assets:"
        echo "    - $DMG_PATH"
        echo "    - $MD5_PATH"
        echo "    - $SHA256_PATH"
        echo "    - $MANIFEST_PATH"
    fi
    if [[ "$SKIP_R2" -eq 0 ]]; then
        echo "R2 upload plan:"
        echo "  releases/$DMG_NAME"
        echo "  releases/$MD5_NAME"
        echo "  releases/$SHA256_NAME"
        echo "  releases/${VERSION}.json"
        echo "  latest/$LATEST_DMG_NAME"
        echo "  latest/$LATEST_MD5_NAME"
        echo "  latest/$LATEST_SHA256_NAME"
        echo "  latest/$LATEST_MANIFEST_NAME"
        if [[ -n "${CINDER_R2_PUBLIC_BASE_URL:-}" ]]; then
            echo "  latest URL: ${CINDER_R2_PUBLIC_BASE_URL}/latest/$LATEST_DMG_NAME"
        fi
    fi
    exit 0
fi

GITHUB_URL=""
R2_VERSION_URL=""
R2_LATEST_URL=""

if [[ "$SKIP_GITHUB" -eq 0 ]]; then
    GITHUB_URL=$(publish_github)
fi

if [[ "$SKIP_R2" -eq 0 ]]; then
    r2_output="$(publish_r2)"
    R2_VERSION_URL="$(printf '%s\n' "$r2_output" | tail -2 | head -1)"
    R2_LATEST_URL="$(printf '%s\n' "$r2_output" | tail -1)"
fi

echo ""
echo "══════════════════════════════════════════════════"
echo "  ✅ Published Cinder $TAG"
if [[ -n "$GITHUB_URL" ]]; then
    echo "  GitHub: $GITHUB_URL"
fi
if [[ -n "$R2_VERSION_URL" ]]; then
    echo "  R2 versioned: $R2_VERSION_URL"
fi
if [[ -n "$R2_LATEST_URL" ]]; then
    echo "  R2 latest:    $R2_LATEST_URL"
fi
echo "══════════════════════════════════════════════════"
