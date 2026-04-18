#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR/.."
cd "$PROJECT_DIR"

CMAKE_FILE="CMakeLists.txt"
BUILD_DIR="build-release"
PUBLISH_SCRIPT="scripts/publish-release.sh"

usage() {
    cat <<'USAGE'
Usage: ./scripts/release.sh [major|minor|patch] [options]

Options:
  --dry-run         generate release artifacts and print planned publish actions
  --skip-build      reuse existing build-release/Cinder.app or DMG
  --skip-notarize   package/sign only; do not notarize
  --skip-github     do not create or update GitHub release
  --skip-r2         do not upload release artifacts to Cloudflare R2
  -h, --help        show this help
USAGE
}

BUMP=""
DRY_RUN=0
SKIP_BUILD=0
SKIP_NOTARIZE=0
SKIP_GITHUB=0
SKIP_R2=0

for arg in "$@"; do
    case "$arg" in
        major|minor|patch)
            if [[ -n "$BUMP" ]]; then
                echo "❌ Only one bump type may be specified"
                exit 1
            fi
            BUMP="$arg"
            ;;
        --dry-run) DRY_RUN=1 ;;
        --skip-build) SKIP_BUILD=1 ;;
        --skip-notarize) SKIP_NOTARIZE=1 ;;
        --skip-github) SKIP_GITHUB=1 ;;
        --skip-r2) SKIP_R2=1 ;;
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

if [[ ! -f "$CMAKE_FILE" ]]; then
    echo "❌ $CMAKE_FILE not found"
    exit 1
fi

CURRENT_VERSION=$(grep 'project(Cinder VERSION' "$CMAKE_FILE" | sed 's/.*VERSION \([^ )]*\).*/\1/')
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

case "$BUMP" in
    major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
    minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
    patch) PATCH=$((PATCH + 1)) ;;
    "") ;;
esac

NEW_VERSION="$MAJOR.$MINOR.$PATCH"
TAG="v$NEW_VERSION"
DMG_NAME="Cinder-${NEW_VERSION}-macOS.dmg"
DMG_PATH="$BUILD_DIR/$DMG_NAME"
RELEASE_NOTES_PATH="$BUILD_DIR/release-notes-${TAG}.md"
MANIFEST_PATH="$BUILD_DIR/release-manifest-${TAG}.json"
MD5_PATH="${DMG_PATH}.md5"
SHA256_PATH="${DMG_PATH}.sha256"

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

if [[ "$DRY_RUN" -eq 0 && "$SKIP_GITHUB" -eq 0 ]]; then
    require_command git
    require_command gh
    if ! gh auth status >/dev/null 2>&1; then
        echo "❌ Not authenticated. Run: gh auth login"
        exit 1
    fi
fi

if [[ "$DRY_RUN" -eq 0 && "$SKIP_GITHUB" -eq 0 ]]; then
    if ! git diff --quiet || ! git diff --cached --quiet; then
        echo "❌ Working tree has uncommitted changes. Commit or stash first."
        exit 1
    fi
else
    if ! git diff --quiet || ! git diff --cached --quiet; then
        echo "ℹ️  Dry run with dirty working tree; no git state will be modified."
    fi
fi

if [[ "$DRY_RUN" -eq 0 && "$SKIP_GITHUB" -eq 0 ]] && git tag -l "$TAG" | grep -q "$TAG"; then
    echo "❌ Tag $TAG already exists."
    exit 1
fi

if [[ ! -x "$PUBLISH_SCRIPT" ]]; then
    echo "❌ Publish helper missing: $PUBLISH_SCRIPT"
    exit 1
fi

PREV_TAG=$(git tag -l 'v*' --sort=-v:refname | head -1 || true)
if [[ -z "$PREV_TAG" ]]; then
    RANGE="HEAD"
else
    RANGE="${PREV_TAG}..HEAD"
fi

FEATURES=""
FIXES=""
OTHER=""
while IFS= read -r line; do
    msg="${line#* }"
    lower=$(echo "$msg" | tr '[:upper:]' '[:lower:]')
    case "$lower" in
        add*|feat*|implement*|new*|create*|enable*|introduce*) FEATURES+="- $msg"$'\n' ;;
        fix*|bugfix*|resolve*|correct*|repair*) FIXES+="- $msg"$'\n' ;;
        bump\ version*) ;;
        *) OTHER+="- $msg"$'\n' ;;
    esac
done < <(git log "$RANGE" --pretty=format:"%h %s" --no-merges)

CHANGELOG=""
if [[ -n "$FEATURES" ]]; then
    CHANGELOG+="### Added"$'\n'"$FEATURES"$'\n'
fi
if [[ -n "$FIXES" ]]; then
    CHANGELOG+="### Fixed"$'\n'"$FIXES"$'\n'
fi
if [[ -n "$OTHER" ]]; then
    CHANGELOG+="### Other"$'\n'"$OTHER"$'\n'
fi
if [[ -z "$CHANGELOG" ]]; then
    CHANGELOG="Initial release."
fi

echo "Current version: $CURRENT_VERSION"
echo "Release version: $NEW_VERSION ($TAG)"
echo "Release notes: $RELEASE_NOTES_PATH"
echo ""
echo "═══ Release Notes Preview ═══"
printf '%s\n' "$CHANGELOG"
echo "═════════════════════════════"
echo ""

if [[ "$DRY_RUN" -eq 0 ]]; then
    read -rp "Proceed with release $TAG? [y/N] " confirm
    if [[ "$confirm" != [yY] ]]; then
        echo "Aborted."
        exit 1
    fi
fi

if [[ "$NEW_VERSION" != "$CURRENT_VERSION" ]]; then
    echo "📝 Planned version bump: $CURRENT_VERSION → $NEW_VERSION"
    if [[ "$DRY_RUN" -eq 0 ]]; then
        sed -i '' "s/project(Cinder VERSION $CURRENT_VERSION/project(Cinder VERSION $NEW_VERSION/" "$CMAKE_FILE"
        git add "$CMAKE_FILE"
        git commit -m "Bump version to $NEW_VERSION"
    fi
fi

require_command md5
require_command shasum

if [[ "$DRY_RUN" -eq 0 ]]; then
    echo "🔨 Building release artifact..."
    if [[ "$SKIP_BUILD" -eq 1 && "$SKIP_NOTARIZE" -eq 1 ]]; then
        bash scripts/build-dmg.sh --skip-build --skip-notarize
    elif [[ "$SKIP_BUILD" -eq 1 ]]; then
        bash scripts/build-dmg.sh --skip-build
    elif [[ "$SKIP_NOTARIZE" -eq 1 ]]; then
        bash scripts/build-dmg.sh --skip-notarize
    else
        bash scripts/build-dmg.sh
    fi
elif [[ -f "$DMG_PATH" ]]; then
    echo "🧪 Dry run using existing artifact: $DMG_PATH"
else
    echo "🧪 Dry run without existing artifact; packaging step will be skipped"
fi

mkdir -p "$BUILD_DIR"
printf '%s\n' "$CHANGELOG" > "$RELEASE_NOTES_PATH"

MD5_VALUE=""
SHA256_VALUE=""
FILE_SIZE_BYTES=0
if [[ -f "$DMG_PATH" ]]; then
    MD5_VALUE=$(md5 -q "$DMG_PATH")
    SHA256_VALUE=$(shasum -a 256 "$DMG_PATH" | awk '{print $1}')
    FILE_SIZE_BYTES=$(stat -f%z "$DMG_PATH")
fi

BUILD_DATE_UTC=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
NOTARIZED=false
if [[ "$SKIP_NOTARIZE" -eq 0 ]] && notarization_configured; then
    NOTARIZED=true
fi

if [[ -f "$DMG_PATH" ]]; then
    printf '%s  %s\n' "$MD5_VALUE" "$(basename "$DMG_PATH")" > "$MD5_PATH"
    printf '%s  %s\n' "$SHA256_VALUE" "$(basename "$DMG_PATH")" > "$SHA256_PATH"
fi

cat > "$MANIFEST_PATH" <<MANIFEST
{
  "version": "$NEW_VERSION",
  "tag": "$TAG",
  "artifact_name": "$(basename "$DMG_PATH")",
  "artifact_path": "$DMG_PATH",
  "artifact_size_bytes": $FILE_SIZE_BYTES,
  "md5": "$MD5_VALUE",
  "sha256": "$SHA256_VALUE",
  "build_date_utc": "$BUILD_DATE_UTC",
  "notarized": $NOTARIZED,
  "release_notes_path": "$RELEASE_NOTES_PATH"
}
MANIFEST

echo "🧾 Release artifacts prepared:"
echo "  DMG:      $DMG_PATH"
if [[ -f "$DMG_PATH" ]]; then
    echo "  MD5:      $MD5_PATH"
    echo "  SHA256:   $SHA256_PATH"
else
    echo "  MD5:      skipped (artifact not present)"
    echo "  SHA256:   skipped (artifact not present)"
fi
echo "  Manifest: $MANIFEST_PATH"

echo "📦 Publishing plan:"
if [[ "$SKIP_GITHUB" -eq 1 ]]; then
    echo "  GitHub: skipped"
else
    echo "  GitHub: $TAG will be published as latest"
fi
if [[ "$SKIP_R2" -eq 1 ]]; then
    echo "  R2:     skipped"
else
    echo "  R2:     releases/ and latest/ objects will be uploaded"
fi

PUBLISH_ARGS=(
    --version "$NEW_VERSION"
    --tag "$TAG"
    --dmg "$DMG_PATH"
    --notes "$RELEASE_NOTES_PATH"
    --manifest "$MANIFEST_PATH"
    --md5 "$MD5_PATH"
    --sha256 "$SHA256_PATH"
)

if [[ "$DRY_RUN" -eq 1 ]]; then
    PUBLISH_ARGS+=(--dry-run)
fi
if [[ "$SKIP_GITHUB" -eq 1 ]]; then
    PUBLISH_ARGS+=(--skip-github)
fi
if [[ "$SKIP_R2" -eq 1 ]]; then
    PUBLISH_ARGS+=(--skip-r2)
fi

bash "$PUBLISH_SCRIPT" "${PUBLISH_ARGS[@]}"
