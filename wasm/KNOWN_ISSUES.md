# HashLink WASM - Known Issues and Limitations

**Last Updated:** November 16, 2025
**Status:** Production-ready for core features, limitations for async/networking

---

## Critical Issues

### 1. Function Signature Mismatch Warning ⚠️

**Warning during compilation:**
```
wasm-ld: warning: function signature mismatch: hl_hi64remove
>>> defined as (i32, i64) -> void in libhl.a(types.c.o)
>>> defined as (i32, i64) -> i32 in libhl.a(maps.c.o)
```

**Impact:** Non-critical
- WASM compiles and runs correctly
- No runtime errors observed
- Function has two different signatures in different compilation units

**Status:** Known linker warning, does not affect functionality

**Fix:** Low priority - harmonize function signatures in future

---

## Architectural Limitations (Expected)

These are WASM platform limitations, not HashLink bugs:

### 2. No JIT Compilation ❌

**Issue:** JIT/VM mode not available in WASM

**Reason:** WebAssembly doesn't support runtime code generation

**Workaround:** Use HL-C compilation (already the default for WASM)

**Status:** Expected limitation, documented

---

### 3. No Threading ❌

**Issue:** Multi-threading not supported

**Reason:**
- HashLink uses platform-specific threading (pthreads/Windows threads)
- WASM threading exists but not integrated yet
- HL_NO_THREADS defined for WASM builds

**Impact:**
- `Sys.thread()` - Not available
- `Mutex`, `Lock` - Not available
- Concurrent operations - Not available

**Workaround:** Use single-threaded design patterns

**Status:** Expected limitation

**Future:** Could integrate WASM threads (Atomics + SharedArrayBuffer)

---

### 4. Blocking Operations May Freeze Browser ⚠️

**Issue:** Synchronous blocking operations freeze the browser

**Affected:**
- `Sys.sleep()` - Blocks main thread (may work but freezes UI)
- Synchronous socket operations
- Blocking file I/O

**Reason:** JavaScript runs on single thread, blocking = UI freeze

**Workaround:**
- Use emscripten_sleep() for async sleep (requires Asyncify)
- Use async I/O patterns
- Use web APIs (fetch, WebSocket) via JS interop

**Status:** Platform limitation, needs async wrappers

---

## Networking Limitations

### 5. No Native Sockets ❌

**Issue:** `src/std/socket.c` native sockets don't work in browsers

**Reason:** Browser security model doesn't allow raw TCP/UDP

**Impact:**
- `sys.net.Socket` - Not available
- Direct TCP/UDP connections - Not available
- Raw socket operations - Not available

**Workaround:**
- Use WebSocket for TCP-like connections
- Use WebRTC DataChannel for UDP-like connections
- Implement wrapper: native socket → WebSocket bridge

**Status:** Expected, requires WebSocket integration

**Example Integration Needed:**
```haxe
// Would need JS interop
@:hlNative("js", "websocket_connect")
static function connect(url:String):WebSocket;
```

---

### 6. No HTTP Client (libuv not available) ❌

**Issue:** `uv.hdll` (libuv) not built for WASM

**Reason:**
- libuv depends on platform-specific I/O
- WASM builds skip libs/ directory (CMakeLists.txt:493)
- Browser has its own HTTP via `fetch()`

**Impact:**
- No `haxe.Http` using native implementation
- No event loop from libuv
- No async I/O patterns

**Workaround:**
- Use browser `fetch()` API via JS interop
- Use XMLHttpRequest via JS interop
- Implement wrapper for Haxe HTTP client

**Status:** Expected, needs browser API integration

**Example Integration Needed:**
```haxe
@:hlNative("js", "fetch")
static function httpGet(url:String):Promise<String>;
```

---

### 7. No Event Loop ❌

**Issue:** HashLink's event loop (libuv) not available

**Reason:** libuv.hdll not built for WASM

**Impact:**
- `haxe.Timer` callbacks - May not work
- Async callbacks - Limited
- Event-driven code - Needs adaptation

**Workaround:**
- Use browser's event loop (setTimeout, setInterval)
- Use requestAnimationFrame for game loops
- Implement JS interop for callbacks

**Status:** Needs browser API integration

**Example:**
```haxe
@:hlNative("js", "setTimeout")
static function setTimeout(callback:Void->Void, ms:Int):Int;
```

---

## File System Limitations

### 8. No Direct File I/O ❌

**Issue:** `Sys.read()`, `Sys.write()` don't work in browsers

**Reason:** Browser security model prevents direct filesystem access

**Impact:**
- File reading/writing - Not available by default
- Directory operations - Not available
- File existence checks - Not available

**Workaround:**
- Use Emscripten MEMFS (virtual in-memory filesystem)
- Use Emscripten IDBFS (IndexedDB persistence)
- Use File API + FileReader for user-selected files
- Use fetch() for reading bundled assets

**Status:** Needs Emscripten FS integration

**Integration Options:**

**Option 1: MEMFS (In-Memory)**
```c
// Setup in C, before main()
EM_ASM(
  FS.mkdir('/data');
  FS.mount(MEMFS, {}, '/data');
);
```

**Option 2: IDBFS (Persistent)**
```c
EM_ASM(
  FS.mkdir('/persistent');
  FS.mount(IDBFS, {}, '/persistent');
  FS.syncfs(true, function(err) {});
);
```

**Option 3: User File Selection**
```javascript
// In JS
document.getElementById('fileInput').onchange = function(e) {
  const file = e.target.files[0];
  const reader = new FileReader();
  reader.onload = function(ev) {
    // Pass to WASM
  };
  reader.readAsText(file);
};
```

---

## Time and Timing

### 9. Sys.sleep() Limitations ⚠️

**Issue:** `Sys.sleep()` may block browser UI

**Status:** Works but not recommended

**Reason:** Blocks JavaScript main thread

**Impact:**
- UI freezes during sleep
- Browser "page unresponsive" warnings
- Poor user experience

**Workaround:**
- Use Emscripten Asyncify + emscripten_sleep()
- Use setTimeout() for delays
- Redesign to avoid blocking waits

**Integration with Asyncify:**
```bash
emcc -s ASYNCIFY=1 -s ASYNCIFY_STACK_SIZE=32768 ...
```

Then in C:
```c
#include <emscripten.h>
emscripten_sleep(100); // Non-blocking sleep!
```

---

### 10. Sys.time() Works ✅

**Status:** WORKS

**Verified:** Returns accurate timestamps

**Implementation:** Uses Emscripten's time functions

**No issues observed**

---

### 11. Date.now() Works ✅

**Status:** WORKS

**Verified:** Returns correct dates/times

**No issues observed**

---

## Process and System

### 12. No Process Spawning ❌

**Issue:** `Sys.command()`, process execution not available

**Reason:** WASM sandbox has no OS process access

**Impact:**
- Can't execute shell commands
- Can't spawn child processes
- Can't use system()

**Workaround:** Not possible in browser - architectural limitation

**Status:** Expected limitation

---

### 13. Environment Variables Limited ⚠️

**Issue:** `Sys.getEnv()` may not work as expected

**Reason:** No real environment in browser

**Workaround:**
- Define custom env in JS before loading WASM
- Use Emscripten ENV object

**Status:** Partial support possible

---

## Extended Libraries (Not Available)

These libraries are **intentionally skipped** for WASM builds (CMakeLists.txt:493):

### 14. No SDL (Graphics) ❌

**Status:** Not built for WASM

**Reason:** Skipped in build

**Workaround:**
- Use HTML Canvas via JS interop
- Use WebGL via JS interop
- Port SDL calls to Canvas API

**Future:** Could port SDL to SDL2 Emscripten port

---

### 15. No OpenAL (Audio) ❌

**Status:** Not built for WASM

**Reason:** Skipped in build

**Workaround:**
- Use Web Audio API via JS interop
- Use HTMLAudioElement via JS interop

---

### 16. No DirectX ❌

**Status:** Windows-only, N/A for WASM

**Workaround:** Use WebGL

---

### 17. No OpenGL (Native) ❌

**Status:** Not available in browser

**Workaround:** Use WebGL via Emscripten's OpenGL ES emulation

---

## Memory and Performance

### 18. Memory Growth Enabled ✅

**Status:** WORKS

**Configuration:** `-s ALLOW_MEMORY_GROWTH=1`

**Impact:** WASM heap can grow as needed

**No issues observed**

---

### 19. Garbage Collection Works ✅

**Status:** WORKS

**Verified:** Emscripten heap integration functional

**Implementation:** Uses emscripten_builtin_memalign/free

**No issues observed**

---

## Build System

### 20. Extended Libs Auto-Skipped ✅

**Status:** WORKS AS DESIGNED

**Implementation:** CMakeLists.txt:493-495

```cmake
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_subdirectory(libs)
endif()
```

**No issues**

---

### 21. Tests Auto-Disabled ✅

**Status:** WORKS AS DESIGNED

**Implementation:** CMakeLists.txt:64

```cmake
set(BUILD_TESTING OFF CACHE BOOL "Disable tests for WASM" FORCE)
```

**No issues**

---

## Summary Table

| Feature | Status | Workaround Available | Priority |
|---------|--------|---------------------|----------|
| Core runtime | ✅ Works | N/A | N/A |
| Standard library | ✅ Works | N/A | N/A |
| Math operations | ✅ Works | N/A | N/A |
| Strings/Arrays/Maps | ✅ Works | N/A | N/A |
| Exceptions | ✅ Works | N/A | N/A |
| Garbage collection | ✅ Works | N/A | N/A |
| Sys.time() | ✅ Works | N/A | N/A |
| Date.now() | ✅ Works | N/A | N/A |
| JIT | ❌ No | Use HL-C | Expected |
| Threading | ❌ No | Single-threaded | Expected |
| Sys.sleep() | ⚠️ Blocks UI | Asyncify or setTimeout | Medium |
| Native sockets | ❌ No | WebSocket | High |
| HTTP client (uv) | ❌ No | fetch() API | High |
| Event loop | ❌ No | Browser event loop | High |
| File I/O | ❌ No | MEMFS/IDBFS | Medium |
| Process spawning | ❌ No | None | Expected |
| SDL (graphics) | ❌ No | Canvas API | Medium |
| OpenAL (audio) | ❌ No | Web Audio API | Medium |

---

## Recommended Integration Priorities

For a production-ready WASM implementation, consider adding:

1. **High Priority:**
   - WebSocket wrapper for networking
   - Browser fetch() wrapper for HTTP
   - Timer/setTimeout integration

2. **Medium Priority:**
   - Emscripten MEMFS for file operations
   - Canvas API for graphics
   - Web Audio API for sound
   - Asyncify for non-blocking sleep

3. **Low Priority:**
   - IDBFS for persistent storage
   - WebGL wrappers
   - Performance optimizations

---

## Testing Status

**Tested and Working:**
- ✅ HelloWorld example (basic I/O)
- ✅ Test.hx (comprehensive features)
- ✅ AdvancedTest.hx (advanced language features)
- ✅ Math operations
- ✅ String operations
- ✅ Collections (arrays, maps)
- ✅ Objects and classes
- ✅ Exceptions
- ✅ Generics and abstracts
- ✅ Pattern matching

**Not Yet Tested:**
- ⏳ Async operations
- ⏳ Timer callbacks
- ⏳ File I/O with MEMFS
- ⏳ WebSocket integration
- ⏳ HTTP via fetch()

---

## Conclusion

**Production Readiness:**
- ✅ **Core runtime:** Production-ready
- ⚠️ **Async/Networking:** Needs integration work
- ❌ **Extended libs:** Not available (use web APIs)

**For typical WASM use cases (games, data processing, visualization), the core runtime is sufficient. For networking/async operations, additional integration work is needed.**
