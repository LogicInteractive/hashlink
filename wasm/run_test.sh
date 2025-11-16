#!/bin/bash
set -e

# HashLink WASM Test Runner
# Compiles Haxe test to C, then to WASM, and serves via HTTP

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-wasm"

echo "================================="
echo "HashLink WASM Test Runner"
echo "================================="
echo ""

# Check for required tools
if ! command -v haxe &> /dev/null; then
    echo "ERROR: haxe not found!"
    echo "Please install Haxe from https://haxe.org/download/"
    exit 1
fi

if ! command -v emcc &> /dev/null; then
    echo "ERROR: emcc not found!"
    echo "Please install and activate Emscripten (see wasm/README.md)"
    exit 1
fi

echo "✓ Found Haxe: $(haxe --version 2>&1)"
echo "✓ Found Emscripten: $(emcc --version | head -1)"
echo ""

# Check for libhl.a
if [ ! -f "$BUILD_DIR/bin/libhl.a" ]; then
    echo "ERROR: libhl.a not found!"
    echo "Please run: ./wasm/build_wasm.sh"
    exit 1
fi

echo "✓ Found libhl.a: $(du -h "$BUILD_DIR/bin/libhl.a" | cut -f1)"
echo ""

# Step 1: Compile Haxe to C
echo "Step 1: Compiling Haxe to C..."
cd "$SCRIPT_DIR"

haxe -hl test.c -main Test

if [ ! -f "test.c" ]; then
    echo "ERROR: Haxe compilation failed!"
    exit 1
fi

echo "✓ Generated test.c ($(wc -l < test.c) lines)"
echo ""

# Step 2: Compile C to WASM
echo "Step 2: Compiling C to WASM..."
echo "  Optimization: -Oz (size)"
echo "  Memory: growth allowed"
echo "  Template: minimal_template.html"
echo ""

emcc test.c -o test.html \
    --shell-file minimal_template.html \
    -I. \
    -I"$PROJECT_ROOT/src" \
    -L"$BUILD_DIR/bin" \
    -lhl \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s EXPORTED_FUNCTIONS='["_main"]' \
    -Oz

if [ ! -f "test.wasm" ]; then
    echo "ERROR: WASM compilation failed!"
    exit 1
fi

echo ""
echo "✓ Generated test.html"
echo "✓ Generated test.js ($(du -h test.js | cut -f1))"
echo "✓ Generated test.wasm ($(du -h test.wasm | cut -f1))"
echo ""

# Step 3: Serve via HTTP
echo "================================="
echo "Build Successful!"
echo "================================="
echo ""
echo "To test in browser:"
echo ""
echo "  1. Start HTTP server:"
echo "     cd $SCRIPT_DIR"
echo "     python3 -m http.server 8080"
echo ""
echo "  2. Open in browser:"
echo "     http://localhost:8080/test.html"
echo ""
echo "  3. Check browser console for output"
echo ""

# Optionally start server automatically
read -p "Start HTTP server now? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Starting server on http://localhost:8080"
    echo "Press Ctrl+C to stop"
    echo ""
    python3 -m http.server 8080
fi
