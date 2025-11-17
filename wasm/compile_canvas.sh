#!/bin/bash
set -e

# Helper script to compile canvas tests with correct flags
# Usage: ./compile_canvas.sh CanvasTest
#    or: ./compile_canvas.sh SimpleCanvasTest

if [ $# -eq 0 ]; then
    echo "Usage: $0 <TestName>"
    echo "Example: $0 CanvasTest"
    echo "Example: $0 SimpleCanvasTest"
    exit 1
fi

TEST_NAME=$1
OUTPUT_NAME=$(echo "$TEST_NAME" | tr '[:upper:]' '[:lower:]')

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-wasm"

echo "================================="
echo "Canvas Test Compiler"
echo "================================="
echo ""
echo "Test: $TEST_NAME"
echo "Output: $OUTPUT_NAME.{html,js,wasm}"
echo ""

# Check for required tools
if ! command -v haxe &> /dev/null; then
    echo "ERROR: haxe not found!"
    exit 1
fi

if ! command -v emcc &> /dev/null; then
    echo "ERROR: emcc not found!"
    echo "Please activate Emscripten first:"
    echo "  source /tmp/emsdk/emsdk_env.sh"
    exit 1
fi

# Check for libhl.a
if [ ! -f "$BUILD_DIR/bin/libhl.a" ]; then
    echo "ERROR: libhl.a not found!"
    echo "Please run: ./wasm/build_wasm.sh"
    exit 1
fi

cd "$SCRIPT_DIR"

# Step 1: Compile Haxe to C
echo "Step 1: Compiling Haxe to C..."
echo "  Flags: -D hl_emscripten -D no-threads"
haxe -D hl_emscripten -D no-threads -hl ${OUTPUT_NAME}.c -main $TEST_NAME

if [ ! -f "${OUTPUT_NAME}.c" ]; then
    echo "ERROR: Haxe compilation failed!"
    exit 1
fi

echo "✓ Generated ${OUTPUT_NAME}.c"
echo ""

# Step 2: Compile C to WASM
echo "Step 2: Compiling C to WASM..."
echo "  Template: canvas_template.html"
echo "  Optimization: -Oz"

emcc ${OUTPUT_NAME}.c -o ${OUTPUT_NAME}.html \
    --shell-file canvas_template.html \
    -I. \
    -I"$PROJECT_ROOT/src" \
    -L"$BUILD_DIR/bin" \
    -lhl \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s EXPORTED_FUNCTIONS='["_main"]' \
    -s ERROR_ON_UNDEFINED_SYMBOLS=0 \
    -Oz

if [ ! -f "${OUTPUT_NAME}.wasm" ]; then
    echo "ERROR: WASM compilation failed!"
    exit 1
fi

echo ""
echo "================================="
echo "Build Complete!"
echo "================================="
echo ""
echo "✓ ${OUTPUT_NAME}.html ($(du -h ${OUTPUT_NAME}.html | cut -f1))"
echo "✓ ${OUTPUT_NAME}.js ($(du -h ${OUTPUT_NAME}.js | cut -f1))"
echo "✓ ${OUTPUT_NAME}.wasm ($(du -h ${OUTPUT_NAME}.wasm | cut -f1))"
echo ""
echo "To test:"
echo "  cd $SCRIPT_DIR"
echo "  python3 -m http.server 8080"
echo "  # Open http://localhost:8080/${OUTPUT_NAME}.html"
echo ""
