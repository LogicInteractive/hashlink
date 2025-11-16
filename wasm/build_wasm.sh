#!/bin/bash
set -e

# HashLink WASM Build Script
# Builds libhl.a with Emscripten for WebAssembly target

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-wasm"

echo "================================="
echo "HashLink WASM Build"
echo "================================="
echo ""

# Check for Emscripten
if ! command -v emcc &> /dev/null; then
    echo "ERROR: emcc not found!"
    echo ""
    echo "Please install and activate Emscripten:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    echo ""
    exit 1
fi

echo "✓ Found Emscripten: $(emcc --version | head -1)"
echo ""

# Create build directory
echo "Creating build directory: $BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo ""
echo "Configuring with CMake..."
echo "  Toolchain: cmake/Emscripten.cmake"
echo "  Build type: MinSizeRel"
echo "  WITH_VM: OFF"
echo "  Threading: DISABLED"
echo ""

emcmake cmake "$PROJECT_ROOT" \
    -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/Emscripten.cmake" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DWITH_VM=OFF \
    -DBUILD_SHARED_LIBS=OFF

# Build
echo ""
echo "Building libhl.a..."
echo ""

emmake make -j$(nproc) VERBOSE=1

# Check output
echo ""
echo "================================="
echo "Build Complete!"
echo "================================="
echo ""

if [ -f "$BUILD_DIR/bin/libhl.a" ]; then
    SIZE=$(du -h "$BUILD_DIR/bin/libhl.a" | cut -f1)
    echo "✓ libhl.a created: $SIZE"
    echo "  Location: $BUILD_DIR/bin/libhl.a"
else
    echo "✗ ERROR: libhl.a not found!"
    exit 1
fi

echo ""
echo "Next steps:"
echo "  cd $SCRIPT_DIR"
echo "  ./run_test.sh"
echo ""
