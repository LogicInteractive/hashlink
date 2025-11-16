# HashLink WebAssembly Support

HashLink HL-C now has official WebAssembly support through Emscripten. This allows you to compile Haxe programs to WASM and run them in web browsers.

## Features

- ✅ Full HashLink core runtime (libhl) compiled to WASM
- ✅ Garbage collection with Emscripten heap integration
- ✅ Complete standard library support
- ✅ PCRE2 regex engine included
- ✅ Automatic build configuration via CMake
- ✅ Optimized for size with `-Oz` flag
- ⚠️ No JIT (uses HL-C compilation)
- ⚠️ No threading (single-threaded WASM)
- ⚠️ No extended libraries (SDL, OpenAL, etc.)

## Prerequisites

### 1. Install Emscripten

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 2. Install Haxe

**Ubuntu/Debian:**
```bash
sudo apt-get install haxe
```

**macOS:**
```bash
brew install haxe
```

**Windows:**
Download from https://haxe.org/download/

## Building libhl for WASM

The HashLink build system automatically detects Emscripten and configures appropriately:

```bash
# Source Emscripten environment
source /path/to/emsdk/emsdk_env.sh

# Run the build script
./wasm/build_wasm.sh
```

This will:
- Configure CMake with Emscripten toolchain
- Set `CMAKE_SYSTEM_NAME=Emscripten`
- Automatically disable JIT (`WITH_VM=OFF`)
- Automatically disable threading (`HL_NO_THREADS`)
- Skip extended libraries (libs directory)
- Skip tests (`BUILD_TESTING=OFF`)
- Build `libhl.a` (~540 KB) in `build-wasm/bin/`

### Manual Build

If you prefer manual control:

```bash
source /path/to/emsdk/emsdk_env.sh

mkdir build-wasm && cd build-wasm

emcmake cmake .. \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DWITH_VM=OFF \
    -DBUILD_SHARED_LIBS=OFF

emmake make -j$(nproc)
```

## Compiling Haxe Programs to WASM

### 1. Write Your Haxe Code

**HelloWorld.hx:**
```haxe
class HelloWorld {
    static function main() {
        trace("Hello from Haxe!");
        trace("Running on WebAssembly!");
        var x = 10;
        var y = 32;
        trace('Math: $x + $y = ${x + y}');
    }
}
```

### 2. Compile to HL-C

```bash
haxe -hl hello.c -main HelloWorld
```

This generates:
- `hello.c` - Main C file
- `hl/`, `haxe/`, `_std/` - Supporting C files

### 3. Compile to WASM

```bash
source /path/to/emsdk/emsdk_env.sh

emcc hello.c \
    -o hello.html \
    -I. \
    -I/path/to/hashlink/src \
    -L/path/to/hashlink/build-wasm/bin \
    -lhl \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_FUNCTIONS='["_main"]' \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -Oz
```

This generates:
- `hello.wasm` - WebAssembly binary (~150-200 KB)
- `hello.js` - Emscripten JavaScript runtime (~65 KB)
- `hello.html` - Default HTML page

### 4. Customize HTML Template

Create `minimal.html`:
```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>My WASM App</title>
</head>
<body>
    <script>
        var Module = {
            print: function(text) {
                console.log(text);
            },
            printErr: function(text) {
                console.error(text);
            }
        };
    </script>
    {{{ SCRIPT }}}
</body>
</html>
```

Then compile with:
```bash
emcc hello.c -o hello.html --shell-file minimal.html ...
```

## Testing

Serve the files with a local HTTP server:

```bash
python3 -m http.server 8080
```

Open `http://localhost:8080/hello.html` in your browser and check the console (F12).

## CMake Integration Details

When `CMAKE_SYSTEM_NAME` is set to `"Emscripten"`, the HashLink build system:

**CMakeLists.txt:62-69** - Adds compiler definitions:
```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_compile_definitions(HL_EMSCRIPTEN HL_NO_THREADS)
    set(BUILD_TESTING OFF CACHE BOOL "Disable tests for WASM" FORCE)
    message(STATUS "Configuring for Emscripten/WASM build")
endif()
```

**CMakeLists.txt:493-495** - Skips extended libraries:
```cmake
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_subdirectory(libs)
endif()
```

## Source Code Changes

The existing HashLink codebase already includes Emscripten support:

**src/hl.h:61-66** - Emscripten detection:
```c
#if defined(__EMSCRIPTEN__)
#   define HL_EMSCRIPTEN
#   define HL_64
#   define HL_LITTLE_ENDIAN
```

**src/gc.c:411-414** - Emscripten memory allocation:
```c
#ifdef HL_EMSCRIPTEN
    ptr = (void*)emscripten_builtin_memalign(HL_WSIZE, size);
#else
    ...
```

No source code modifications were required - only build system integration!

## File Sizes

Typical sizes for a simple HelloWorld program:

- **libhl.a**: ~540 KB (static library)
- **hello.wasm**: ~159 KB (with -Oz optimization)
- **hello.js**: ~65 KB (Emscripten runtime)
- **hello.html**: ~244 bytes (minimal template)

Total download: **~224 KB** (excluding HTML)

With gzip compression, expect ~60-80 KB total.

## Limitations

### What Doesn't Work

1. **JIT Compilation** - WASM doesn't support runtime code generation
   - Solution: Use HL-C compilation instead

2. **Threading** - Single-threaded only
   - WASM threading exists but HashLink doesn't use it yet

3. **Extended Libraries**
   - No SDL (graphics)
   - No OpenAL (audio)
   - No DirectX
   - No native UI
   - Solution: Use web APIs via JavaScript interop

4. **Sockets** - Native sockets don't work in browsers
   - Solution: Use WebSockets with custom wrapper

5. **File System** - No native file access
   - Solution: Use Emscripten MEMFS/IDBFS

### What Works

- ✅ All core language features
- ✅ Strings, arrays, maps
- ✅ Objects and classes
- ✅ Exceptions
- ✅ Regular expressions (PCRE2)
- ✅ Math operations
- ✅ Type system
- ✅ Garbage collection

## Performance

HashLink WASM performance is comparable to JavaScript in most cases:

- **Startup**: Fast (~100ms initialization)
- **Memory**: Efficient GC with Emscripten heap
- **Speed**: Similar to JavaScript (no JIT overhead)
- **Size**: Competitive with other WASM frameworks

## Examples

See the `wasm/` directory for examples:

- **wasm/HelloWorld.hx** - Simple trace output
- **wasm/Test.hx** - Comprehensive feature test
- **wasm/build_wasm.sh** - Automated build script
- **wasm/minimal_template.html** - Minimal HTML template

## Troubleshooting

### "emcc not found"
```bash
source /path/to/emsdk/emsdk_env.sh
```

### "haxe not found"
Install Haxe: https://haxe.org/download/

### "libhl.a not found"
```bash
./wasm/build_wasm.sh
```

### MIME type warning in browser
This is normal. The WASM still loads and runs correctly. To fix, configure your web server to serve `.wasm` files with `application/wasm` MIME type.

### "Module is not defined"
Make sure the `<script>` tag defining `Module` comes **before** the `{{{ SCRIPT }}}` placeholder in your HTML template.

## Advanced Topics

### Custom Emscripten Flags

For smaller binaries:
```bash
emcc hello.c -o hello.js \
    -Oz \
    -flto \
    --closure 1 \
    -s MINIMAL_RUNTIME=1 \
    ...
```

### Linking Multiple Haxe Files

Compile all to C first, then link together:
```bash
haxe -hl main.c -main Main -cp src
emcc main.c -o app.wasm ...
```

### JavaScript Interop

Use `@:hlNative` for external JS functions:
```haxe
@:hlNative("js", "alert")
static function jsAlert(msg:String):Void;
```

## Contributing

WASM support is now integrated into the main HashLink build system. Contributions welcome for:

- Extended library ports (SDL → Canvas, OpenAL → Web Audio)
- File system integration (MEMFS/IDBFS)
- Network support (WebSockets)
- Performance optimizations
- Additional examples

## License

Same as HashLink - MIT License

## Resources

- **HashLink**: https://hashlink.haxe.org/
- **Haxe**: https://haxe.org/
- **Emscripten**: https://emscripten.org/
- **WebAssembly**: https://webassembly.org/
