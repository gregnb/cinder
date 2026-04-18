#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CLANG_TIDY_BIN="${CLANG_TIDY_BIN:-/opt/homebrew/opt/llvm/bin/clang-tidy}"
CLANG_FORMAT_BIN="${CLANG_FORMAT_BIN:-clang-format}"

if ! command -v "$CLANG_FORMAT_BIN" >/dev/null 2>&1; then
    echo "pre-commit: clang-format not found. Install: brew install clang-format"
    exit 1
fi

if [[ ! -x "$CLANG_TIDY_BIN" ]]; then
    echo "pre-commit: clang-tidy not found at '$CLANG_TIDY_BIN'. Install: brew install llvm"
    exit 1
fi

if [[ ! -f build/compile_commands.json ]]; then
    echo "pre-commit: generating compile_commands.json..."
    cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

STAGED_FILES=()
while IFS= read -r file; do
    STAGED_FILES+=("$file")
done < <(
    git diff --cached --name-only --diff-filter=ACMR \
    | grep -E '^(src|tests)/.*\.(cpp|h)$' || true
)

if [[ ${#STAGED_FILES[@]} -eq 0 ]]; then
    exit 0
fi

echo "pre-commit: running clang-format on staged C++ files..."
"$CLANG_FORMAT_BIN" -i "${STAGED_FILES[@]}"

echo "pre-commit: running clang-tidy --fix on staged .cpp files..."
for file in "${STAGED_FILES[@]}"; do
    [[ "$file" == *.cpp ]] || continue
    "$CLANG_TIDY_BIN" -p build -fix -format-style=file "$file" --quiet || true
done

git add "${STAGED_FILES[@]}"
echo "pre-commit: style fixes applied and re-staged."
