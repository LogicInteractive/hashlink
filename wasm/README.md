# HashLink WASM Proof of Concept

This directory contains a proof-of-concept for running HashLink HL-C compiled programs on WebAssembly.

## Prerequisites

### Linux / macOS

1. **Emscripten SDK**
   ```bash
   # Install Emscripten
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **Haxe Compiler**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install haxe

   # macOS
   brew install haxe

   # Or download from https://haxe.org/download/
   ```

3. **CMake** (version 3.13+)
   ```bash
   cmake --version
   ```

### Windows

**See [WINDOWS.md](WINDOWS.md) for complete Windows setup guide.**

Quick install:
```cmd
# Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
emsdk install latest
emsdk activate latest
emsdk_env.bat

# Install via Chocolatey
choco install haxe cmake python
```

## Quick Start

### Linux / macOS

```bash
# From hashlink root directory
./wasm/build_wasm.sh    # Build libhl for WASM
./wasm/run_test.sh      # Run test
# Open http://localhost:8080/test.html
```

### Windows

```cmd
REM From hashlink root directory
wasm\build_wasm.bat     & REM Build libhl for WASM
wasm\run_test.bat       & REM Run test
REM Open http://localhost:8080/test.html
```

**See [WINDOWS.md](WINDOWS.md) for detailed Windows instructions.**

### What These Do

**Build script:**
- Creates a `build-wasm` directory
- Builds libhl.a with Emscripten
- Outputs to `build-wasm/bin/libhl.a`

**Test script:**
- Compiles the Haxe test to C
- Compiles C to WASM
- Generates test.html, test.js, test.wasm
- Starts a local HTTP server
- Opens browser to http://localhost:8080/test.html

**View results:**
Open your browser's console to see the output from the HashLink program.

## Manual Build Steps

If you want to build manually:

```bash
# 1. Build libhl for WASM
mkdir -p build-wasm
cd build-wasm
emcmake cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Emscripten.cmake -DCMAKE_BUILD_TYPE=MinSizeRel
emmake make -j$(nproc)
cd ..

# 2. Compile Haxe test to C
cd wasm
haxe -hl test.c -main Test

# 3. Compile C to WASM
emcc test.c -o test.html \
    -I../src \
    -I../include/pcre \
    -L../build-wasm/bin \
    -lhl \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s EXPORTED_FUNCTIONS='["_main"]' \
    -Oz \
    --closure 1

# 4. Serve and test
python3 -m http.server 8080
# Open http://localhost:8080/test.html
```

## Expected Results

If successful, you should see in the browser console:

```
Hello from HashLink on WASM!
Testing basic operations:
Sum 1-100: 5050
String: Hello WASM
Float math: 42.5
```

## Troubleshooting

### Build fails with "EMSCRIPTEN not set"

Make sure you've sourced the emsdk environment:
```bash
source /path/to/emsdk/emsdk_env.sh
```

### "Cannot find -lhl" error

The libhl.a library wasn't built. Run:
```bash
./wasm/build_wasm.sh
```

### Browser shows nothing

1. Check browser console for errors
2. Make sure you're serving via HTTP (not file://)
3. Check that all .wasm, .js, .html files were generated

### Runtime errors in browser

Check which HashLink features you're using:
- ✓ Supported: Basic operations, strings, arrays, math
- ⚠️ Limited: File I/O (use Emscripten FS)
- ❌ Not supported: Threading, sockets, process spawning

## File Sizes

Typical sizes for minimal HelloWorld:

- **Unoptimized** (-O0): ~8-10 MB
- **Optimized** (-O3): ~2-3 MB
- **Size-optimized** (-Oz --closure 1): ~500 KB - 1 MB

## Next Steps

1. Test with more complex Haxe programs
2. Add Emscripten FS for file operations
3. Benchmark performance vs native HL
4. Optimize binary size further
5. Create browser API bindings

## Known Limitations

- No JIT compilation (uses HL-C compiled code)
- No threading support (single-threaded WASM)
- No raw sockets (would need WebSocket wrapper)
- No process spawning
- File I/O requires Emscripten filesystem

## Resources

- [Emscripten Documentation](https://emscripten.org/docs/)
- [HashLink Documentation](https://github.com/HaxeFoundation/hashlink/wiki)
- [Haxe Language](https://haxe.org/documentation/)
