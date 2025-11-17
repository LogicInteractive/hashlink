# HTTP/HTTPS Support for HashLink WASM - Implementation Plan

**Status:** Planning
**Priority:** HIGH - Critical for web applications
**Complexity:** MEDIUM - Browser APIs available, need integration

---

## Current Situation

### What Doesn't Work ❌

- **sys.Http** - Uses sys.net.Socket (libuv) which isn't available in WASM
- **Native sockets** - No raw TCP/UDP in browser
- **uv.hdll** - Not built for WASM (intentionally skipped)

### What's Available ✅

Browser provides several HTTP APIs:

1. **fetch()** API (modern, Promise-based)
   - Supports GET, POST, PUT, DELETE, etc.
   - Headers, body, credentials
   - CORS-aware
   - Returns Response with status, headers, body

2. **XMLHttpRequest** (legacy, callback-based)
   - More compatible (older browsers)
   - Event-driven
   - Progress tracking

3. **Emscripten's emscripten_fetch()** (C API wrapper around fetch)
   - Low-level C interface
   - Async support
   - Integrates with Emscripten event loop

---

## Architecture Options

### Option 1: Use Haxe's HttpJs (Browser Target) ✅ RECOMMENDED

**Approach:** Leverage existing haxe.http.HttpJs implementation

**Pros:**
- ✅ Already exists and tested
- ✅ Uses browser XMLHttpRequest
- ✅ Async callbacks work
- ✅ Full Haxe API compatibility
- ✅ No C code needed

**Cons:**
- ⚠️ Requires Haxe to compile with `-D js` or equivalent
- ⚠️ HashLink might default to sys.Http

**Implementation:**
```haxe
// In Haxe compilation, add:
-D hl_emscripten
--macro allowPackage('js.html')  // Allow access to js.html.XMLHttpRequest

// Then HttpJs should be available automatically
```

### Option 2: C Wrapper for emscripten_fetch() ⚙️ ALTERNATIVE

**Approach:** Create C bindings to Emscripten's fetch API

**Pros:**
- ✅ Low-level control
- ✅ Async support built-in
- ✅ Direct Emscripten integration

**Cons:**
- ❌ More C code to write
- ❌ Need to manually implement HTTP protocol details
- ❌ Duplicate work (HttpJs already exists)

**Implementation:**
```c
// src/std/http_wasm.c
#ifdef HL_EMSCRIPTEN
#include <emscripten/fetch.h>

typedef struct {
    vclosure *onData;
    vclosure *onError;
    vclosure *onStatus;
} hl_http_request;

void http_download_succeeded(emscripten_fetch_t *fetch) {
    // Handle success callback
}

void http_download_failed(emscripten_fetch_t *fetch) {
    // Handle error callback
}

HL_PRIM void hl_http_request(vstring *url, vclosure *onData, vclosure *onError) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = http_download_succeeded;
    attr.onerror = http_download_failed;

    emscripten_fetch(&attr, hl_to_utf8(url->bytes));
}
#endif
```

### Option 3: JavaScript Interop ⚡ QUICK START

**Approach:** Call browser fetch() directly from Haxe

**Pros:**
- ✅ Extremely simple
- ✅ Modern Promise-based API
- ✅ Fast to prototype

**Cons:**
- ❌ Not integrated with haxe.Http API
- ❌ Requires custom wrapper
- ❌ Manual Promise handling

**Implementation:**
```haxe
@:hlNative("", "fetch_url")
extern function fetchUrl(url:String):js.lib.Promise<String>;

// In JavaScript:
Module.fetch_url = async function(url) {
    const response = await fetch(url);
    return await response.text();
};
```

---

## Recommended Approach: Option 1 (HttpJs)

Use Haxe's existing HttpJs implementation with conditional compilation.

### Implementation Steps

#### 1. Modify Haxe Compilation Defines

Update how WASM projects are compiled:

```bash
# When compiling for WASM, use:
haxe -hl output.c \
     -D hl_emscripten \
     -D js \  # Tell Haxe we're in a JS environment
     -main MyApp
```

**Issue:** This might cause type conflicts (hl vs js types)

#### 2. Create Custom Http Typedef for WASM

**File:** Create `src/std/http_wasm.hx` in HashLink source

```haxe
package hl;

#if hl_emscripten
// Use browser XMLHttpRequest via js.html
typedef Http = haxe.http.HttpJs;
#else
// Use native sys.Http
typedef Http = sys.Http;
#end
```

**Challenge:** Need to make js.html.XMLHttpRequest available to HashLink

#### 3. Emscripten XMLHttpRequest Implementation (C Bridge)

Create C stubs that call browser JavaScript:

**File:** `src/std/http_wasm.c`

```c
#ifdef HL_EMSCRIPTEN
#include <emscripten.h>
#include <hl.h>

typedef struct {
    vclosure *onData;
    vclosure *onError;
    vclosure *onStatus;
    int requestId;
} hl_http_callbacks;

static hl_http_callbacks *callbacks[256];
static int next_request_id = 1;

// Called from JavaScript when request completes
EM_JS(void, xhr_get_impl, (int requestId, const char* url), {
    const id = requestId;
    const urlStr = UTF8ToString(url);

    const xhr = new XMLHttpRequest();
    xhr.open('GET', urlStr, true);

    xhr.onload = function() {
        if (xhr.status >= 200 && xhr.status < 300) {
            // Call back to C with success
            Module._http_on_data(id, xhr.responseText, xhr.status);
        } else {
            Module._http_on_error(id, 'HTTP error: ' + xhr.status);
        }
    };

    xhr.onerror = function() {
        Module._http_on_error(id, 'Network error');
    };

    xhr.send();
});

// Callback from JS to C
EMSCRIPTEN_KEEPALIVE
void http_on_data(int requestId, const char *data, int status) {
    hl_http_callbacks *cb = callbacks[requestId];
    if (!cb) return;

    // Create Haxe string from response
    vstring *response = hl_alloc_string_utf8(data);
    vdynamic *args[1];
    args[0] = (vdynamic*)response;

    // Call onData callback
    if (cb->onData && cb->onData->hasValue) {
        hl_dyn_call(cb->onData, args, 1);
    }

    callbacks[requestId] = NULL;
}

EMSCRIPTEN_KEEPALIVE
void http_on_error(int requestId, const char *error) {
    hl_http_callbacks *cb = callbacks[requestId];
    if (!cb) return;

    vstring *errorMsg = hl_alloc_string_utf8(error);
    vdynamic *args[1];
    args[0] = (vdynamic*)errorMsg;

    if (cb->onError && cb->onError->hasValue) {
        hl_dyn_call(cb->onError, args, 1);
    }

    callbacks[requestId] = NULL;
}

HL_PRIM int hl_http_get(vstring *url, vclosure *onData, vclosure *onError) {
    int requestId = next_request_id++;

    hl_http_callbacks *cb = (hl_http_callbacks*)hl_gc_alloc_noptr(sizeof(hl_http_callbacks));
    cb->onData = onData;
    cb->onError = onError;
    cb->requestId = requestId;

    callbacks[requestId] = cb;

    xhr_get_impl(requestId, hl_to_utf8(url->bytes));

    return requestId;
}

DEFINE_PRIM(_I32, http_get, _STRING _FUN(_VOID, _STRING) _FUN(_VOID, _STRING));

#endif
```

---

## Testing Plan

### Test 1: Simple GET Request

```haxe
class HttpTest {
    static function main() {
        trace("Testing HTTP GET...");

        var http = new haxe.Http("https://httpbin.org/get");

        http.onData = function(data) {
            trace("Success! Response:");
            trace(data);
        };

        http.onError = function(error) {
            trace('Error: $error');
        };

        http.request();

        trace("Request sent, waiting for response...");
    }
}
```

### Test 2: POST Request

```haxe
var http = new haxe.Http("https://httpbin.org/post");
http.setPostData("key=value&foo=bar");
http.onData = function(data) trace(data);
http.request(true);  // true = POST
```

### Test 3: Headers

```haxe
var http = new haxe.Http("https://api.github.com/users/octocat");
http.setHeader("User-Agent", "HashLink-WASM/1.0");
http.setHeader("Accept", "application/json");
http.onData = function(data) {
    var json = haxe.Json.parse(data);
    trace('User: ${json.login}');
};
http.request();
```

---

## Implementation Checklist

### Phase 1: Basic GET Support ⏳
- [ ] Create src/std/http_wasm.c with XMLHttpRequest wrapper
- [ ] Implement hl_http_get() function
- [ ] Add EM_JS macros for JavaScript bridge
- [ ] Add to CMakeLists.txt for WASM builds
- [ ] Test with simple GET request

### Phase 2: Full HTTP Support ⏳
- [ ] Implement POST, PUT, DELETE methods
- [ ] Add header support
- [ ] Add request body support (postData, postBytes)
- [ ] Handle response headers
- [ ] Status code callbacks

### Phase 3: Error Handling ⏳
- [ ] Network errors
- [ ] HTTP error codes (4xx, 5xx)
- [ ] Timeout support
- [ ] Abort/cancel support

### Phase 4: Advanced Features ⏳
- [ ] CORS handling
- [ ] Credentials (withCredentials)
- [ ] Binary responses (Bytes)
- [ ] Progress callbacks
- [ ] File upload support

---

## Challenges & Solutions

### Challenge 1: CORS Restrictions

**Problem:** Browser enforces CORS - can't access arbitrary URLs

**Solution:**
- Document CORS requirements
- Suggest using CORS proxies for development
- Examples must use CORS-enabled endpoints (httpbin.org)

### Challenge 2: Async Callbacks

**Problem:** XMLHttpRequest is async, callbacks happen later

**Solution:**
- ✅ Event loop already working (from timer implementation)
- Callbacks execute via Emscripten event loop
- No changes needed - just use vclosure callbacks

### Challenge 3: Memory Management

**Problem:** Response data needs to be allocated correctly

**Solution:**
- Use `hl_alloc_string_utf8()` for text responses
- Use `hl_alloc_bytes()` for binary responses
- GC handles cleanup automatically

---

## Alternative: Modern fetch() API

If XMLHttpRequest proves difficult, we can use modern fetch():

```javascript
// EM_JS wrapper:
EM_JS(void, fetch_get, (int requestId, const char* url), {
    fetch(UTF8ToString(url))
        .then(response => response.text())
        .then(data => Module._http_on_data(requestId, data, 200))
        .catch(error => Module._http_on_error(requestId, error.message));
});
```

**Pros:**
- Cleaner API
- Promise-based
- More modern

**Cons:**
- Requires Promise handling
- Not supported in very old browsers

---

## Expected Outcome

After implementation:

```haxe
// Standard Haxe code works in WASM:
class MyApp {
    static function main() {
        var http = new haxe.Http("https://api.example.com/data");
        http.onData = function(response) {
            trace('Got data: $response');
        };
        http.request();
    }
}
```

**Result:**
- ✅ Standard `haxe.Http` API works
- ✅ Async callbacks via event loop
- ✅ HTTPS support automatic (browser handles SSL)
- ✅ Same code works native and WASM

---

## Next Step

**Decision needed:** Which approach should we implement first?

1. **Quick prototype** with Option 3 (JS interop) - 30 minutes
2. **Full implementation** with Option 1 (C wrapper) - 2-3 hours
3. **Try HttpJs** approach - investigate compatibility - 1 hour

**Recommendation:** Start with Option 1 (C wrapper with XMLHttpRequest) for maximum compatibility and control.
