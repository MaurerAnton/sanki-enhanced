#!/bin/bash
# Build sanki-tui (Terminal User Interface version)
set -e
cd "$(dirname "$0")"
mkdir -p build-tui
cmake -S tui -B build-tui
make -j$(nproc) -C build-tui
echo ""
echo "Build complete! Binary: build-tui/sanki-tui"
echo "Run: ./build-tui/sanki-tui"
