/*
 * HashLink WASM HTTP Support
 * Uses browser XMLHttpRequest for haxe.Http compatibility
 */

#ifdef HL_EMSCRIPTEN
#include <emscripten.h>
#include <hl.h>
#include <string.h>

// HTTP request callback structure
typedef struct {
    vclosure *onData;
    vclosure *onError;
    vclosure *onStatus;
    int id;
} http_request_t;

#define MAX_HTTP_REQUESTS 256
static http_request_t *active_requests[MAX_HTTP_REQUESTS];
static int next_request_id = 1;

// Forward declarations for callbacks from JavaScript
EMSCRIPTEN_KEEPALIVE void http_on_data_callback(int id, const char *data);
EMSCRIPTEN_KEEPALIVE void http_on_error_callback(int id, const char *error);
EMSCRIPTEN_KEEPALIVE void http_on_status_callback(int id, int status);

// JavaScript XMLHttpRequest wrapper
EM_JS(void, xhr_request, (int id, const char* method, const char* url, const char* headers, const char* body), {
    const requestId = id;
    const methodStr = UTF8ToString(method);
    const urlStr = UTF8ToString(url);
    const headersStr = headers ? UTF8ToString(headers) : null;
    const bodyStr = body ? UTF8ToString(body) : null;

    try {
        const xhr = new XMLHttpRequest();
        xhr.open(methodStr, urlStr, true);  // true = async

        // Set headers if provided
        if (headersStr) {
            const headerLines = headersStr.split('\n');
            for (let line of headerLines) {
                if (line.trim()) {
                    const colonIdx = line.indexOf(':');
                    if (colonIdx > 0) {
                        const key = line.substring(0, colonIdx).trim();
                        const value = line.substring(colonIdx + 1).trim();
                        xhr.setRequestHeader(key, value);
                    }
                }
            }
        }

        xhr.onload = function() {
            const status = xhr.status;

            // Call status callback
            Module._http_on_status_callback(requestId, status);

            if (status >= 200 && status < 400) {
                // Success
                const responseText = xhr.responseText || "";

                // Allocate string in Emscripten heap
                const len = lengthBytesUTF8(responseText) + 1;
                const ptr = Module._malloc(len);
                stringToUTF8(responseText, ptr, len);

                Module._http_on_data_callback(requestId, ptr);
                Module._free(ptr);
            } else {
                // HTTP error
                const errorMsg = "HTTP Error " + status + ": " + xhr.statusText;
                const len = lengthBytesUTF8(errorMsg) + 1;
                const ptr = Module._malloc(len);
                stringToUTF8(errorMsg, ptr, len);

                Module._http_on_error_callback(requestId, ptr);
                Module._free(ptr);
            }
        };

        xhr.onerror = function() {
            const errorMsg = "Network error";
            const len = lengthBytesUTF8(errorMsg) + 1;
            const ptr = Module._malloc(len);
            stringToUTF8(errorMsg, ptr, len);

            Module._http_on_error_callback(requestId, ptr);
            Module._free(ptr);
        };

        xhr.ontimeout = function() {
            const errorMsg = "Request timeout";
            const len = lengthBytesUTF8(errorMsg) + 1;
            const ptr = Module._malloc(len);
            stringToUTF8(errorMsg, ptr, len);

            Module._http_on_error_callback(requestId, ptr);
            Module._free(ptr);
        };

        // Send request
        xhr.send(bodyStr);

    } catch (e) {
        const errorMsg = "XHR Error: " + e.message;
        const len = lengthBytesUTF8(errorMsg) + 1;
        const ptr = Module._malloc(len);
        stringToUTF8(errorMsg, ptr, len);

        Module._http_on_error_callback(requestId, ptr);
        Module._free(ptr);
    }
});

// Helper: Convert UTF8 string to vstring
static vstring* utf8_to_vstring(const char *utf8) {
    int len = hl_utf8_length((vbyte*)utf8, 0);
    uchar *wide = (uchar*)hl_gc_alloc_noptr((len + 1) * sizeof(uchar));
    hl_from_utf8(wide, len, utf8);

    vstring *str = (vstring*)hl_gc_alloc_noptr(sizeof(vstring));
    str->bytes = wide;
    str->length = len;

    return str;
}

// Callback: Data received successfully
EMSCRIPTEN_KEEPALIVE
void http_on_data_callback(int id, const char *data) {
    http_request_t *req = NULL;

    // Find request
    for (int i = 0; i < MAX_HTTP_REQUESTS; i++) {
        if (active_requests[i] && active_requests[i]->id == id) {
            req = active_requests[i];
            break;
        }
    }

    if (!req || !req->onData || !req->onData->hasValue) {
        return;
    }

    // Convert C string to Haxe string
    vstring *str = utf8_to_vstring(data);

    // Call Haxe callback
    vdynamic *args[1];
    args[0] = (vdynamic*)str;
    hl_dyn_call(req->onData, args, 1);

    // Cleanup request
    for (int i = 0; i < MAX_HTTP_REQUESTS; i++) {
        if (active_requests[i] && active_requests[i]->id == id) {
            active_requests[i] = NULL;
            break;
        }
    }
}

// Callback: Error occurred
EMSCRIPTEN_KEEPALIVE
void http_on_error_callback(int id, const char *error) {
    http_request_t *req = NULL;

    // Find request
    for (int i = 0; i < MAX_HTTP_REQUESTS; i++) {
        if (active_requests[i] && active_requests[i]->id == id) {
            req = active_requests[i];
            break;
        }
    }

    if (!req || !req->onError || !req->onError->hasValue) {
        return;
    }

    // Convert C string to Haxe string
    vstring *str = utf8_to_vstring(error);

    // Call Haxe callback
    vdynamic *args[1];
    args[0] = (vdynamic*)str;
    hl_dyn_call(req->onError, args, 1);

    // Cleanup request
    for (int i = 0; i < MAX_HTTP_REQUESTS; i++) {
        if (active_requests[i] && active_requests[i]->id == id) {
            active_requests[i] = NULL;
            break;
        }
    }
}

// Callback: Status code received
EMSCRIPTEN_KEEPALIVE
void http_on_status_callback(int id, int status) {
    http_request_t *req = NULL;

    // Find request
    for (int i = 0; i < MAX_HTTP_REQUESTS; i++) {
        if (active_requests[i] && active_requests[i]->id == id) {
            req = active_requests[i];
            break;
        }
    }

    if (!req || !req->onStatus || !req->onStatus->hasValue) {
        return;
    }

    // Call Haxe callback with status code
    vdynamic *args[1];
    vdynamic statusDyn;
    statusDyn.t = &hlt_i32;
    statusDyn.v.i = status;
    args[0] = &statusDyn;

    hl_dyn_call(req->onStatus, args, 1);
}

// HashLink primitive: Create HTTP request
HL_PRIM int hl_http_request(vstring *method, vstring *url, vstring *headers, vstring *body,
                             vclosure *onData, vclosure *onError, vclosure *onStatus) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_HTTP_REQUESTS; i++) {
        if (!active_requests[i]) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        // No free slots
        return -1;
    }

    // Create request structure
    http_request_t *req = (http_request_t*)hl_gc_alloc_noptr(sizeof(http_request_t));
    req->id = next_request_id++;
    req->onData = onData;
    req->onError = onError;
    req->onStatus = onStatus;

    active_requests[slot] = req;

    // Convert strings to C
    const char *methodStr = method ? hl_to_utf8(method->bytes) : "GET";
    const char *urlStr = url ? hl_to_utf8(url->bytes) : "";
    const char *headersStr = headers ? hl_to_utf8(headers->bytes) : NULL;
    const char *bodyStr = body ? hl_to_utf8(body->bytes) : NULL;

    // Make request
    xhr_request(req->id, methodStr, urlStr, headersStr, bodyStr);

    return req->id;
}

DEFINE_PRIM(_I32, http_request, _STRING _STRING _STRING _STRING _FUN(_VOID, _STRING) _FUN(_VOID, _STRING) _FUN(_VOID, _I32));

#endif // HL_EMSCRIPTEN
