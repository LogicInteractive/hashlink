# HashLink Codebase Structure and Organization

## Executive Summary

HashLink is a high-performance virtual machine and JIT compiler for Haxe. The codebase is organized into:
- **Core Runtime**: ~11,700 LOC (src/) - The minimal HL-C runtime
- **Extended Libraries**: 14+ optional modules (libs/) - Graphics, networking, crypto, etc.
- **Build Systems**: CMake and Make with multi-platform support
- **Existing WASM Support**: Partial Emscripten integration already in place

---

## 1. ROOT DIRECTORY STRUCTURE

```
/home/user/hashlink/
├── CMakeLists.txt          # Primary CMake build (v3.13+)
├── Makefile                # GNU Make build system
├── hl.sln                  # Visual Studio solution
├── libhl.vcxproj          # Visual Studio library project
├── hl.vcxproj             # Visual Studio executable project
├── README.md              # Project documentation
├── Brewfile               # macOS Homebrew dependencies
├── LICENSE                # MIT License
│
├── src/                   # Core runtime (minimal HL-C)
│   ├── hl.h              # Main header with platform detection
│   ├── hlc.h             # HL-C compilation target header
│   ├── hlc_main.c        # Entry point for HL-C compiled binaries
│   ├── hlmodule.h        # Module/code structure definitions
│   ├── hlsystem.h        # System interface definitions
│   ├── opcodes.h         # HL bytecode opcodes
│   ├── allocator.c/h     # Memory allocator interface
│   ├── gc.c              # Garbage collector (1,528 LOC)
│   ├── code.c            # Bytecode reading/parsing (1,101 LOC)
│   ├── jit.c             # JIT compiler (4,689 LOC)
│   ├── module.c          # Module loading/management (1,013 LOC)
│   ├── main.c            # VM entry point (315 LOC)
│   ├── debugger.c        # Debugger support (157 LOC)
│   └── profile.c         # Performance profiling (586 LOC)
│
├── src/std/              # Standard library implementations
│   ├── array.c           # Array operations
│   ├── buffer.c          # Buffer management
│   ├── bytes.c           # Byte operations
│   ├── cast.c            # Type casting (601 LOC)
│   ├── date.c            # Date/time functions
│   ├── debug.c           # Debug utilities (415 LOC)
│   ├── error.c           # Error handling
│   ├── file.c            # File I/O operations
│   ├── fun.c             # Function operations (465 LOC)
│   ├── maps.c            # Hash map implementation (with maps.h)
│   ├── math.c            # Math functions
│   ├── obj.c             # Object system (1,351 LOC)
│   ├── process.c         # Process management (319 LOC)
│   ├── random.c          # Random number generation
│   ├── regexp.c          # Regular expressions
│   ├── socket.c          # Network sockets (517 LOC)
│   ├── string.c          # String operations (383 LOC)
│   ├── sys.c             # System utilities (741 LOC) - *Platform-specific*
│   ├── sys_android.c     # Android-specific implementation
│   ├── sys_ios.m         # iOS-specific implementation
│   ├── thread.c          # Threading (1,176 LOC)
│   ├── track.c           # Memory tracking (7,956 LOC)
│   ├── types.c           # Type system (1,001 LOC)
│   ├── ucs2.c            # Unicode support
│   ├── sort.h            # Sorting algorithms
│   └── unicase.h         # Unicode case tables
│
├── libs/                 # Optional extended libraries
│   ├── fmt/              # Image format handling (PNG, JPEG, etc.)
│   ├── sdl/              # SDL2 graphics
│   ├── openal/           # OpenAL audio
│   ├── ssl/              # SSL/TLS (MbedTLS)
│   ├── uv/               # LibUV asynchronous I/O
│   ├── sqlite/           # SQLite database
│   ├── mysql/            # MySQL database
│   ├── ui/               # UI stub
│   ├── heaps/            # Heaps engine (3D graphics, meshes)
│   ├── video/            # Video codec support
│   ├── directx/          # DirectX (Windows only)
│   └── mesa/             # OpenGL/Mesa
│
├── include/              # Third-party headers and bundled libraries
│   ├── pcre/             # PCRE2 regex library (bundled source)
│   ├── mbedtls/          # MbedTLS crypto library
│   ├── sqlite/           # SQLite source
│   ├── zlib/             # Compression
│   ├── png/              # PNG support
│   ├── vorbis/           # Vorbis audio
│   ├── turbojpeg/        # JPEG compression
│   ├── gl/               # OpenGL headers
│   ├── libuv/            # LibUV headers
│   ├── meshoptimizer/    # Mesh optimization
│   ├── mdbg/             # macOS debugger support
│   ├── minimp3/          # MP3 decoding
│   └── [others...]       # Additional dependencies
│
├── other/
│   ├── cmake/            # CMake toolchain files and modules
│   │   ├── linux32.toolchain.cmake
│   │   ├── FindTurboJPEG.cmake
│   │   ├── FindLibUV.cmake
│   │   └── [Find*.cmake]  # Dependency discovery modules
│   ├── tests/            # Test programs
│   │   ├── HelloWorld.hx  # Basic test
│   │   └── Threads.hx     # Threading test
│   ├── haxelib/          # Haxe library templates
│   ├── uvsample/         # LibUV sample
│   ├── benchs/           # Benchmarks
│   ├── osx/              # macOS-specific build files
│   └── statics/          # Static assets
│
├── .github/workflows/
│   └── build.yml         # GitHub Actions CI/CD configuration
│
└── .git/                 # Git repository metadata
```

---

## 2. CORE RUNTIME COMPONENTS (src/ directory)

### 2.1 Platform Detection & Configuration (hl.h - 1,027 LOC)

```c
Platform defines automatically detected:
├── HL_WIN / HL_WIN_DESKTOP  (Windows)
├── HL_MAC / HL_IOS / HL_TVOS (Apple platforms)
├── HL_ANDROID               (Android)
├── HL_LINUX                 (Linux)
├── HL_EMSCRIPTEN           (Emscripten/WASM) ← Already supported!
├── HL_BSD                   (BSD systems)
├── HL_PS / HL_NX / HL_XBO / HL_XBS (Consoles)
├── HL_64 / HL_32           (Architecture)
├── HL_VCC / HL_GCC / HL_CLANG (Compiler)
├── HL_MOBILE               (iOS/Android/tvOS)
├── HL_CONSOLE              (PlayStation/Switch/Xbox)
└── HL_THREADS / HL_NO_THREADS (Threading support)
```

Key Features:
- **Unicode Support**: char16_t (uchar) on Unix/WASM, wchar_t on Windows
- **Export/Import Macros**: Cross-platform visibility control
- **Debug/Release**: Conditional debugging features
- **Architecture-specific Code**: 32/64-bit, endian detection

### 2.2 Type System (hl.h continuation - lines 334-450+)

Defined types in hl_type_kind:
```
HVOID, HUI8, HUI16, HI32, HI64, HF32, HF64, HBOOL, HBYTES, HDYN,
HFUN, HOBJ, HARRAY, HTYPE, HREF, HVIRTUAL, HDYNOBJ, HABSTRACT,
HENUM, HNULL, HMETHOD, HSTRUCT, HPACKED, HGUID
```

Type structures:
- `hl_type_fun`: Function types with args, return type, closure info
- `hl_type_obj`: Object types with fields, prototypes, bindings
- `hl_type_virtual`: Virtual field lookup
- `hl_enum_construct`: Enum construction info
- `hl_type_enum`: Enum definitions

### 2.3 Memory Management

**Allocator (allocator.c/h - 638 LOC)**
- Layered allocation strategy
- Free list caching (MAX_FL_CACHED = 16)
- Page-aligned allocation for GC
- Platform-specific implementations:
  - Windows: VirtualAlloc
  - POSIX: mmap
  - **Emscripten: emscripten_builtin_memalign/free**

**Garbage Collector (gc.c - 1,528 LOC)**
- Mark-and-sweep collector
- 9 partitions with variable-sized blocks
- Free list per partition
- Platform-specific page allocation:
  - HL_CONSOLE: sys_alloc_align()
  - HL_EMSCRIPTEN: emscripten_builtin_memalign()
  - Others: mmap with collision detection
- Prefetch optimization for DRAM

### 2.4 Bytecode & Code Loading (code.c - 1,101 LOC)

Binary format reading:
- Variable-length integer encoding
- String/bytes/double/float constants
- Type definitions
- Function definitions with opcodes
- Debug information

Operations on bytecode:
- hl_code_read(): Parse .hl files
- hl_code_hash_*(): Hash-based code verification
- hl_code_free(): Memory cleanup
- Module loading and patching

### 2.5 JIT Compiler (jit.c - 4,689 LOC)

**Architecture Support**: x86/x86-64 only
- Does NOT support ARM (explicit error)
- Requires HL-C native compilation for ARM/WASM

CPU Register allocation (x86/x86-64):
```
Registers: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI (x86)
          + R8-R15 (x86-64)
```

Instruction types:
```
MOV, LEA, PUSH, ADD, SUB, IMUL, DIV, IDIV, CDQ, CDQE, POP, RET, CALL
AND, OR, XOR, CMP, TEST, NOP, SHL, SHR, SAR, INC, DEC, JMP
FSTP, FSTP32, FLD, FLD32, FLDCW (FPU)
MOVSD, MOVSS, COMISD, COMISS, ADDSD, SUBSD, MULSD, DIVSD (SSE)
```

Key constraint: JIT output is x86/x86-64 machine code only.

### 2.6 Module System (module.c - 1,013 LOC)

```c
struct hl_module {
    hl_code *code;              // Parsed bytecode
    int codesize;               // Generated code size
    int globals_size;           // Global variables storage
    unsigned char *globals_data;// Global data segment
    void **functions_ptrs;      // Function pointers (JIT or native)
    void *jit_code;             // Generated JIT code
    hl_debug_infos *jit_debug;  // Debug information
    jit_ctx *jit_ctx;          // JIT context
    hl_module_context ctx;     // Module context
};
```

Functions:
- hl_module_alloc(): Create module from code
- hl_module_init(): Initialize module
- hl_module_patch(): Hot reload support
- hl_module_debug(): Debugger setup

### 2.7 Entry Points

**hlc_main.c (179 LOC)** - For HL-C compiled binaries
- Symbol resolution (platform-specific)
- Stack capture (Windows, Linux, macOS with backtrace)
- Entry point execution
- Exception handling

**main.c (315 LOC)** - For .hl bytecode interpreter
- File-based .hl code loading
- Hot reload support (file change detection)
- Module initialization
- Safe execution with exception catching

---

## 3. STANDARD LIBRARY (src/std/ - 10,072 LOC)

### Core Functionality Matrix

| Module | LOC | Purpose | Platform-Specific |
|--------|-----|---------|-------------------|
| obj.c | 1,351 | Object system, reflection | Yes (sys_) |
| thread.c | 1,176 | Threading, mutexes, TLS | Yes (pthread/Windows) |
| types.c | 1,001 | Type system, reflection | Yes (debug info) |
| sys.c | 741 | System utilities | **YES - Critical** |
| socket.c | 517 | Network I/O | Yes (platform sockets) |
| fun.c | 465 | Function operations | Mostly portable |
| string.c | 383 | String manipulation | Mostly portable |
| cast.c | 601 | Type casting | Platform-dependent |
| debug.c | 415 | Debug symbols | Yes (platform stacktrace) |
| buffer.c | 418 | Buffer management | Mostly portable |
| bytes.c | 301 | Byte operations | Mostly portable |
| error.c | 301 | Error handling | Mostly portable |
| maps.c | 319 | Hash maps | Mostly portable |
| process.c | 319 | Process management | Yes (fork/CreateProcess) |
| file.c | ~200 | File I/O | Yes (platform paths) |
| random.c | 150 | RNG | Uses C stdlib |
| date.c | 135 | Date/time | Yes (system time) |
| track.c | 250 | Memory tracking | Debug-only |
| regexp.c | 140 | Regex (PCRE2) | Via pcre2 library |
| ucs2.c | 180 | Unicode | Mostly portable |

### Platform-Specific Implementations

**sys.c (741 LOC)** - Critical for platform support
- hl_sys_string(): Get OS name (returns "Windows", "Linux", "Mac", "Emscripten", etc.)
- hl_sys_locale(): Get system locale
- hl_sys_time(): Get current time
- hl_file_*(): File operations (UTF-16 on Windows, UTF-8 elsewhere)
- Directory operations

**Conditional includes:**
- sys_android.c: Android-specific
- sys_ios.m: iOS Objective-C interface

---

## 4. BUILD SYSTEM ARCHITECTURE

### 4.1 CMake (Primary - CMakeLists.txt)

```cmake
Version: 3.13+
Output: bin/ (unified output directory)

Targets:
├── libhl (shared library)
│   ├── PCRE2 (all source files, bundled)
│   ├── GC (gc.c)
│   ├── Standard Library (src/std/*.c)
│   └── Platform libraries (pthread, dl, ws2_32, etc.)
│
└── hl (VM executable) [if WITH_VM=ON]
    ├── code.c
    ├── jit.c
    ├── main.c
    ├── module.c
    ├── debugger.c
    ├── profile.c
    └── libhl
```

Configuration options:
```cmake
WITH_VM                  (default: ON except ARM/aarch64 without x86_64)
BUILD_SHARED_LIBS        (default: ON)
DOWNLOAD_DEPENDENCIES    (default: OFF, ON for Android)
FLAT_INSTALL_TREE        (default: MSVC)
```

Compiler features required:
- C11 (`c_std_11`)
- C++11 (`cxx_std_11`)

### 4.2 GNU Make (Makefile - 15,419 bytes)

Platform detection:
```makefile
LBITS := $(shell getconf LONG_BIT)
MARCH ?= $(shell uname -m)
ARCH ?= $(shell uname -m)
UNAME := $(shell uname)
```

Architecture support:
- Default: 64-bit (MARCH=64)
- Optional: 32-bit (MARCH=32)
- Special: Windows MinGW, Cygwin, MSVC

Library compilation:
- Optional: fmt, sdl, ssl, openal, ui, uv, mysql, sqlite, heaps
- Each can be independently enabled/disabled

### 4.3 Visual Studio

Projects:
- hl.sln: Solution file
- libhl.vcxproj: Library build
- hl.vcxproj: Executable build
- Each library has .vcxproj in libs/xxx/

---

## 5. PLATFORM ABSTRACTION LAYERS

### 5.1 Compiler Detection

```c
HL_VCC          // MSVC
HL_MINGW        // MinGW
HL_CYGWIN       // Cygwin
HL_GCC          // GCC
HL_CLANG        // Clang
HL_LLVM         // LLVM backend

// Compiler-specific pragmas in hlc.h
```

### 5.2 Operating System Detection

```c
HL_WIN / HL_WIN_DESKTOP    // Windows Desktop
HL_MAC                     // macOS
HL_IOS                     // iOS
HL_TVOS                    // tvOS
HL_ANDROID                 // Android
HL_LINUX                   // Linux
HL_BSD                     // BSD (FreeBSD, NetBSD, OpenBSD)
HL_EMSCRIPTEN             // Emscripten/WASM ← ALREADY INTEGRATED
HL_PS / HL_NX / HL_XBO / HL_XBS  // Consoles
```

### 5.3 Existing WASM/Emscripten Support

Currently integrated in codebase:

**In gc.c:**
```c
#if defined(HL_EMSCRIPTEN)
    #include <emscripten/heap.h>
    // Uses: emscripten_builtin_memalign(align, size)
    //       emscripten_builtin_free(ptr)
#endif
```

**In allocator.c:**
- Emscripten memory allocation via `emscripten_builtin_memalign()`
- Freeing via `emscripten_builtin_free()`

**In sys.c:**
```c
#elif defined(HL_EMSCRIPTEN)
    return (vbyte*)USTR("Emscripten");
```

### 5.4 Threading Model

Two modes controlled by `HL_THREADS`:
```c
#ifndef HL_NO_THREADS
    #define HL_THREADS
    // MSVC: __declspec(thread)
    // GCC/Clang: __thread
#else
    #define HL_THREAD_VAR
    #define HL_THREAD_STATIC_VAR static
#endif
```

**Can be disabled for WASM:** Pass `-DHL_NO_THREADS` to disable threading.

---

## 6. LIBRARY DEPENDENCIES

### 6.1 Bundled (Included in source)

| Library | Location | Purpose | Size |
|---------|----------|---------|------|
| PCRE2 | include/pcre/ | Regular expressions | Multiple .c files |
| MbedTLS | include/mbedtls/ | Crypto/SSL | Complete source |
| SQLite | include/sqlite/src/ | Database | sqlite3.c/h |
| zlib | include/zlib/ | Compression | Bundled source |
| PNG | include/png/ | PNG images | libpng |
| Vorbis | include/vorbis/ | Audio codec | libvorbis |
| TurboJPEG | include/turbojpeg/ | JPEG | turbojpeg |
| Minimp3 | include/minimp3/ | MP3 decode | Single header |
| MeshOptimizer | include/meshoptimizer/ | Mesh optimization | C++ library |

### 6.2 System Dependencies (Optional)

Detected via CMake FindXXX modules:
- libSDL2: Graphics rendering
- libOpenAL: Audio output
- libUV: Async I/O
- libMbedTLS: SSL/TLS (or bundled)
- libPNG, TurboJPEG, Vorbis: Codecs

---

## 7. BUILD OUTPUTS

### libhl (Shared Library)

- **Name**: libhl.so (Linux/BSD), libhl.dylib (macOS), libhl.dll (Windows)
- **Size**: ~2-5 MB (with codecs), ~500KB (minimal)
- **Exports**: hl.h and hlc.h headers
- **Symbols**: ~200+ public functions and types

### hl (Executable)

- **Name**: hl (Unix), hl.exe (Windows)
- **Size**: ~1-2 MB
- **Purpose**: Bytecode interpreter for .hl files
- **Optional**: Can be disabled with `-DWITH_VM=OFF`

### hlc (HL-C Compiler Output)

- Generated from Haxe via: `haxe -hl src/_main.c`
- Statically linked or links against libhl
- Runs natively (no JIT overhead for WASM!)

---

## 8. MINIMAL HL-C RUNTIME FOR WASM

### What's Required for WASM Compilation

**Core libhl components (required):**
1. allocator.c - Memory allocation (Emscripten hooks already present)
2. gc.c - Garbage collection (Emscripten support present)
3. src/std/ - Standard library
   - obj.c - Object system, REQUIRED
   - types.c - Type system, REQUIRED
   - cast.c - Type casting, REQUIRED
   - string.c - String operations, REQUIRED
   - bytes.c - Byte operations, REQUIRED
   - buffer.c - Buffer management, REQUIRED
   - error.c - Error handling, REQUIRED
   - fun.c - Function operations, REQUIRED
   - maps.c - Hash maps, REQUIRED
   - array.c - Arrays, REQUIRED (basic operations)
   - ucs2.c - Unicode, REQUIRED
   - date.c - Date/time (platform-dependent), OPTIONAL
   - random.c - RNG, OPTIONAL (uses C stdlib)
   - sys.c - Platform utilities (MINIMAL stub for WASM), REQUIRED
   - regexp.c - Regex (requires PCRE2), OPTIONAL
   - file.c - File I/O (limited in WASM), OPTIONAL
   - socket.c - Networking (limited in WASM), OPTIONAL
   - thread.c - Threading (SHOULD DISABLE for WASM), OPTIONAL
   - process.c - Process mgmt (N/A for WASM), OPTIONAL
   - debug.c - Debug info (optional), OPTIONAL
   - track.c - Memory tracking (debug), OPTIONAL
4. PCRE2 - For regex support (optional)

**Can disable for WASM:**
- JIT compiler (jit.c) - Emscripten compiles to WASM anyway
- Debugger (debugger.c) - Unless need runtime debugging
- Profile (profile.c) - Optional profiling
- Threading (thread.c) - Set `-DHL_NO_THREADS`

---

## 9. GITHUB ACTIONS BUILD MATRIX

Supported configurations (see .github/workflows/build.yml):

### Operating Systems
- Linux (x86_64, x86, ARM)
- macOS (Intel x86_64, Apple Silicon ARM64)
- Windows (x86, x86_64)
- Android (via CMake toolchain)

### Build Systems
- GNU Make
- CMake (Unix Makefiles, Ninja, Visual Studio)
- Visual Studio 2019/2022
- MinGW

### Configurations
- Debug/Release
- 32-bit/64-bit
- Static/Shared libraries

**Notable absence:** No WASM/Emscripten in CI pipeline (yet) - opportunity for contribution!

---

## 10. ARCHITECTURE SUMMARY

### Compilation Flow

```
Haxe Code
    ↓
[Haxe Compiler]
    ↓
    ├─→ .hl (bytecode) → [hl VM with JIT] → Native x86/x86-64 code
    │
    └─→ _main.c (C source) → [C Compiler] → libhl + Generated code
                               ↓
                          [hlc] Executable
                          [hlc.wasm] (if Emscripten)
```

### Two Compilation Modes

1. **Interpreter Mode**: Load .hl bytecode at runtime
   - Slower but flexible
   - JIT compilation of hot functions (x86/x86-64 only)

2. **HL-C Mode**: Compile to C first, then compile with C compiler
   - Faster execution
   - Portable to any platform (WASM, ARM, etc.)
   - No JIT needed

### For WASM Support

- Use HL-C mode: `haxe -hl output.c ...`
- Compile with Emscripten: `emcc output.c -lhl ...`
- Existing Emscripten integration in GC and allocator provides hooks

---

## 11. KEY FILES FOR WASM PORTING

### Must Modify
1. `/home/user/hashlink/src/hl.h` - Add WASM64 detection
2. `/home/user/hashlink/src/gc.c` - Expand Emscripten handling
3. `/home/user/hashlink/src/std/sys.c` - WASM-specific stubs
4. `/home/user/hashlink/src/std/file.c` - WASM filesystem handling
5. `/home/user/hashlink/src/std/socket.c` - WASM network restrictions
6. `/home/user/hashlink/src/std/thread.c` - Disable for WASM

### Should Review
- `/home/user/hashlink/CMakeLists.txt` - Add Emscripten toolchain
- `/home/user/hashlink/Makefile` - Add WASM target
- `/home/user/hashlink/src/hlc_main.c` - WASM entry point handling
- `/home/user/hashlink/src/allocator.c` - Memory strategies

### Reference Implementation
- Current Emscripten support: `#ifdef HL_EMSCRIPTEN` blocks
- Already uses: `emscripten_builtin_memalign()`, `emscripten_builtin_free()`

---

## SUMMARY

HashLink is well-structured for WASM porting:
- **Modular architecture**: Core runtime ~12K LOC, easily prunable
- **Multi-platform ready**: Already detects 10+ OS variants
- **Emscripten hooks**: GC and allocator already integrated
- **HL-C advantage**: Can skip JIT, use Emscripten compilation directly
- **Build system flexibility**: CMake supports custom toolchains

The minimal HL-C runtime for WASM would be ~6-8 MB unoptimized, with potential
for significant size reduction through LTO and dead code elimination.
