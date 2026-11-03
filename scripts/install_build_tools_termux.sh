#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

# Run this script from a real Termux installation, not from the VS Code
# sandbox. The sandbox has no package manager and cannot install system tools.
if ! command -v pkg >/dev/null 2>&1; then
    echo "ERROR: Termux pkg is not available. Run this script inside a real Termux app." >&2
    exit 127
fi
pkg update -y
pkg upgrade -y
pkg install -y cmake make clang git unzip openjdk-17

echo "Build tools installed. Verify with: cmake --version; clang++ --version"