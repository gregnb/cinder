#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CLANG_TIDY_BIN="${CLANG_TIDY_BIN:-/opt/homebrew/opt/llvm/bin/clang-tidy}"
CLANG_FORMAT_BIN="${CLANG_FORMAT_BIN:-clang-format}"

if ! command -v "$CLANG_FORMAT_BIN" >/dev/null 2>&1; then
    echo "Error: clang-format not found. Install it with: brew install clang-format"
    exit 1
fi

if [[ ! -x "$CLANG_TIDY_BIN" ]]; then
    echo "Error: clang-tidy not found at '$CLANG_TIDY_BIN'."
    echo "Set CLANG_TIDY_BIN or install llvm: brew install llvm"
    exit 1
fi

if [[ ! -f build/compile_commands.json ]]; then
    echo "No build/compile_commands.json found. Configuring project first..."
    cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

FORMAT_FILES=()
while IFS= read -r file; do
    FORMAT_FILES+=("$file")
done < <(find src tests -type f \( -name "*.cpp" -o -name "*.h" \) | sort)

TIDY_FILES=()
while IFS= read -r file; do
    TIDY_FILES+=("$file")
done < <(find src tests -type f -name "*.cpp" | sort)

if [[ ${#FORMAT_FILES[@]} -eq 0 ]]; then
    echo "No C++ files found under src/ or tests/."
    exit 0
fi

echo "Formatting ${#FORMAT_FILES[@]} files with clang-format..."
"$CLANG_FORMAT_BIN" -i "${FORMAT_FILES[@]}"

echo "Applying clang-tidy fixes (including braces-around-statements)..."
for file in "${TIDY_FILES[@]}"; do
    "$CLANG_TIDY_BIN" -p build -fix -format-style=file "$file" --quiet || true
done

echo "Style auto-fix pass complete."
