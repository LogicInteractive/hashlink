# HashLink WASM Investigation - Documentation Index

## Overview

This directory contains a comprehensive exploration and analysis of the HashLink codebase for WASM/Emscripten compilation support.

## Generated Documentation

### 1. **hashlink_overview.md** (621 lines, 21 KB)
Complete technical breakdown of the HashLink codebase:
- Directory structure with all major files
- Core runtime components (src/)
- Standard library breakdown (src/std/)
- Build system architecture (CMake, Make, Visual Studio)
- Platform abstraction layers
- Library dependencies and bundled libraries
- Minimal HL-C runtime requirements for WASM
- GitHub Actions CI/CD configuration
- Architecture summary and next steps

**Use this for:** Detailed understanding of each component, file sizes, dependencies, and technical details.

### 2. **hashlink_architecture.txt** (212 lines, 19 KB)
Visual architecture diagrams and relationships:
- Platform abstraction layer diagram
- Core runtime module dependencies
- Memory management (allocator + GC)
- Bytecode execution engines
- Standard library organization (by tier)
- Optional extended libraries
- Build output flow diagrams
- WASM compilation requirements checklist
- Threading model and Emscripten integration

**Use this for:** Quick visual understanding, architecture overview, and WASM requirements checklist.

## Key Findings Summary

### Existing WASM Support
HashLink already has partial Emscripten integration:
- ✓ `HL_EMSCRIPTEN` platform detection in `src/hl.h`
- ✓ Emscripten memory allocation hooks in `src/gc.c`
- ✓ Emscripten memory management in `src/allocator.c`
- ✓ System detection in `src/std/sys.c`

### Codebase Statistics
- **Core Runtime**: 11,703 LOC in src/
- **Standard Library**: 10,072 LOC in src/std/
- **Total (minimal)**: ~21,800 LOC
- **Extended Libraries**: 14+ optional modules (libs/)
- **Build Systems**: CMake (primary), Make, Visual Studio

### Critical Files for WASM
**Must Modify:**
1. `/home/user/hashlink/src/std/sys.c` (741 LOC) - Platform utilities (CRITICAL)
2. `/home/user/hashlink/src/gc.c` (1,528 LOC) - GC with Emscripten support
3. `/home/user/hashlink/src/std/file.c` - File I/O (WASM limitations)
4. `/home/user/hashlink/src/std/socket.c` - Network I/O (WASM limitations)
5. `/home/user/hashlink/src/std/thread.c` (1,176 LOC) - Disable with `-DHL_NO_THREADS`

**Should Review:**
- `/home/user/hashlink/CMakeLists.txt` - Add Emscripten toolchain
- `/home/user/hashlink/Makefile` - Add WASM target
- `/home/user/hashlink/src/hlc_main.c` - Entry point handling

### Can Skip for WASM
- `jit.c` (4,689 LOC) - x86/x64 only, not needed for WASM
- `main.c` - Bytecode interpreter (use HL-C instead)
- `code.c` - Bytecode parsing (not needed for HL-C)
- `module.c` - JIT module loading (not needed)
- Optional: `debugger.c`, `profile.c`

## Recommended WASM Compilation Path

**HL-C Mode (Recommended):**
```
Haxe Source Code (.hx)
    ↓
haxe -hl output.c -main MyApp
    ↓
Generated C Code (output.c)
    ↓
emcc output.c -o app.js -s WASM=1 -lhl
    ↓
app.wasm + app.js (Browser-ready!)
```

**NOT:** Using bytecode (.hl) + JIT, which only works on x86/x64

## Minimal WASM Runtime Estimate

**Unoptimized:** 6-8 MB
**With LTO:** 1-2 MB
**Aggressive optimization:** 500 KB - 1 MB

### Required Components
- allocator.c (Emscripten hooks)
- gc.c (Emscripten GC)
- src/std/: obj.c, types.c, string.c, bytes.c, buffer.c, cast.c, error.c, fun.c, maps.c, array.c, ucs2.c, sys.c (WASM stubs)
- PCRE2 (optional, for regex)
- hlc_main.c (entry point)

## Build System Overview

### CMake (Primary)
- Version: 3.13+
- Targets: libhl (shared), hl (VM executable)
- Option: `-DWITH_VM=OFF` to skip JIT (good for WASM/ARM)
- Supports: Android NDK, custom toolchains

### Make
- Platform detection via shell commands
- Optional libraries: each independently togglable
- 64/32-bit support

### Visual Studio
- Full projects for Windows
- Library projects for each module

## Platform Support

### Currently Supported
- Windows (32/64-bit, Desktop, Xbox)
- macOS (Intel, Apple Silicon)
- iOS, tvOS
- Linux
- Android
- BSD
- PlayStation, Nintendo Switch, Xbox (consoles)
- **Emscripten/WASM** (partial - ready to enhance)

### Architecture Detection
- 64-bit (default): `__x86_64__`, `_M_X64`, `__LP64__`, `__wasm64__`
- 32-bit: Manual override
- Compiler detection: MSVC, GCC, Clang, LLVM

## Threading Model

Can be disabled with `-DHL_NO_THREADS` for WASM:
```c
#ifndef HL_NO_THREADS
    #define HL_THREADS
    // __thread (GCC/Clang) or __declspec(thread) (MSVC)
#else
    #define HL_THREAD_VAR
    #define HL_THREAD_STATIC_VAR static
#endif
```

## Next Steps for WASM Support

### Phase 1: Core Infrastructure
1. Create Emscripten CMake toolchain
2. Refine `HL_EMSCRIPTEN` detection
3. Create `sys_wasm.c` stub implementations
4. Disable threading by default

### Phase 2: Standard Library
1. WASM stubs for file.c (Emscripten FS)
2. WASM stubs for socket.c (WebSocket)
3. WASM stubs for process.c (no-op)
4. Test core functionality

### Phase 3: Build & Testing
1. Add Emscripten to GitHub Actions
2. Create sample WASM project
3. Performance benchmarking
4. Binary size optimization

### Phase 4: Documentation
1. WASM build instructions
2. Limitations & known issues
3. Example projects

## Existing CI/CD Coverage

### GitHub Actions (.github/workflows/build.yml)
Currently tests:
- ✓ Linux (x86_64, x86, ARM)
- ✓ macOS (Intel, Apple Silicon)
- ✓ Windows (32/64-bit)
- ✓ Android

**Notable absence:**
- ✗ WASM/Emscripten (opportunity for enhancement)

## Architecture Layers

```
Platform Detection (hl.h)
    ↓
Core Runtime Abstraction
    ├─ Memory Management (allocator.c + gc.c)
    ├─ Execution Engines (jit.c, code.c, module.c)
    └─ Entry Points (hlc_main.c, main.c)
    ↓
Standard Library (src/std/ - 10K LOC)
    ├─ Type System (types.c, obj.c)
    ├─ Data Structures (array.c, maps.c, buffer.c)
    ├─ String/Unicode (string.c, ucs2.c)
    ├─ Platform Utilities (sys.c - CRITICAL)
    ├─ Network/File (socket.c, file.c)
    └─ Optional (thread.c, process.c, etc.)
    ↓
Optional Extended Libraries (libs/ - 14 modules)
    ├─ Graphics (SDL, Heaps, OpenGL/Mesa)
    ├─ Media (fmt with codecs, OpenAL)
    ├─ Networking (LibUV)
    ├─ Crypto (MbedTLS)
    └─ Databases (SQLite, MySQL)
```

## Key Code References

### Platform Detection
**File:** `src/hl.h` (lines 32-90)
```c
#if defined(__EMSCRIPTEN__)
#   define HL_EMSCRIPTEN
#   ifndef _GNU_SOURCE
#       define _GNU_SOURCE
#   endif
#endif
```

### Emscripten Memory Management
**File:** `src/gc.c` (lines 31, 1189, 1243)
```c
#if defined(HL_EMSCRIPTEN)
#   include <emscripten/heap.h>
#endif

#elif defined(HL_EMSCRIPTEN)
    return emscripten_builtin_memalign(GC_PAGE_SIZE, size);
    emscripten_builtin_free(ptr);
```

### System Identification
**File:** `src/std/sys.c` (line 117)
```c
#elif defined(HL_EMSCRIPTEN)
    return (vbyte*)USTR("Emscripten");
```

## File Organization

```
/home/user/hashlink/
├── WASM_EXPLORATION_INDEX.md       ← This file
├── hashlink_overview.md             ← Detailed breakdown
├── hashlink_architecture.txt        ← Visual diagrams
├── src/                             ← Core runtime
│   ├── hl.h                        ← Platform detection
│   ├── gc.c                        ← GC with Emscripten support
│   ├── allocator.c                 ← Memory with Emscripten hooks
│   └── std/                        ← Standard library
│       ├── sys.c                   ← CRITICAL for WASM
│       ├── file.c                  ← File I/O (WASM limited)
│       ├── socket.c                ← Network (WASM limited)
│       └── thread.c                ← Should disable for WASM
├── CMakeLists.txt                  ← Primary build (add Emscripten)
├── Makefile                        ← Make alternative
├── .github/workflows/              ← CI/CD (add WASM builds)
└── libs/                           ← Optional libraries (14 modules)
```

## Quick Reference Checklist

### For WASM Development
- [ ] Read `hashlink_overview.md` for detailed understanding
- [ ] Review `hashlink_architecture.txt` for visual diagrams
- [ ] Check `src/hl.h` for platform detection patterns
- [ ] Review `src/gc.c` for Emscripten integration examples
- [ ] Examine `src/std/sys.c` for platform-specific patterns
- [ ] Check `CMakeLists.txt` for build system structure
- [ ] Review `.github/workflows/build.yml` for CI/CD patterns

### For WASM Porting
1. Create `sys_wasm.c` with stub implementations
2. Modify build system to support Emscripten toolchain
3. Disable threading: `-DHL_NO_THREADS`
4. Create WASM-specific overrides for file.c and socket.c
5. Add GitHub Actions job for Emscripten builds
6. Test HL-C compilation: `haxe -hl output.c ...` → `emcc output.c -o app.js`

## Resources

- **HashLink Wiki:** https://github.com/HaxeFoundation/hashlink/wiki/
- **Emscripten Documentation:** https://emscripten.org/docs/
- **Haxe Language:** https://haxe.org/
- **WASM Specification:** https://webassembly.org/

---

*Documentation generated during comprehensive codebase exploration*
*All file paths are absolute: /home/user/hashlink/*
