/*
 * HashLink WASM Event Loop Integration
 * Uses Emscripten's main loop and browser APIs for timers
 */

#ifdef HL_EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#include <hl.h>

// Timer callback structure
typedef struct {
    vclosure *callback;
    int interval_ms;
    int repeat;
    int id;
    int active;
} hl_timer;

#define MAX_TIMERS 256
static hl_timer timers[MAX_TIMERS];
static int next_timer_id = 1;

// Main loop callback for Emscripten
static vclosure *main_loop_callback = NULL;

void hl_emscripten_main_loop() {
    if (main_loop_callback && main_loop_callback->hasValue) {
        hl_dyn_call(main_loop_callback, NULL, 0);
    }
}

// Set main loop (for game loops, animations)
HL_PRIM void hl_set_main_loop(vclosure *callback, int fps) {
    main_loop_callback = callback;
    if (callback) {
        int simulate_infinite_loop = 1; // Keep running
        emscripten_set_main_loop(hl_emscripten_main_loop, fps, simulate_infinite_loop);
    } else {
        emscripten_cancel_main_loop();
    }
}

// Timer tick function (called by Emscripten)
static void timer_tick(void *arg) {
    int timer_id = (int)(intptr_t)arg;

    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].id == timer_id && timers[i].active) {
            if (timers[i].callback && timers[i].callback->hasValue) {
                hl_dyn_call(timers[i].callback, NULL, 0);
            }

            // If not repeating, deactivate
            if (!timers[i].repeat) {
                timers[i].active = 0;
            }
            break;
        }
    }
}

// Create a timer (setTimeout/setInterval replacement)
HL_PRIM int hl_set_timeout(vclosure *callback, int delay_ms, bool repeat) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) return -1; // No free slots

    int timer_id = next_timer_id++;
    timers[slot].callback = callback;
    timers[slot].interval_ms = delay_ms;
    timers[slot].repeat = repeat;
    timers[slot].id = timer_id;
    timers[slot].active = 1;

    // Use Emscripten's async call
    if (repeat) {
        // setInterval equivalent
        emscripten_async_call(timer_tick, (void*)(intptr_t)timer_id, delay_ms);
    } else {
        // setTimeout equivalent
        emscripten_async_call(timer_tick, (void*)(intptr_t)timer_id, delay_ms);
    }

    return timer_id;
}

// Clear a timer
HL_PRIM void hl_clear_timeout(int timer_id) {
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].id == timer_id) {
            timers[i].active = 0;
            break;
        }
    }
}

// Non-blocking sleep using Emscripten
HL_PRIM void hl_async_sleep(int ms) {
    emscripten_sleep(ms); // Requires -s ASYNCIFY=1
}

// Request animation frame (for smooth 60fps animations)
static vclosure *raf_callback = NULL;

static bool raf_tick(double time, void *userData) {
    if (raf_callback && raf_callback->hasValue) {
        vdynamic d;
        vdynamic *args[1];
        d.t = &hlt_f64;
        d.v.d = time;
        args[0] = &d;
        hl_dyn_call(raf_callback, args, 1);
    }
    return true;  // Continue animation loop
}

HL_PRIM void hl_request_animation_frame(vclosure *callback) {
    raf_callback = callback;
    emscripten_request_animation_frame(raf_tick, NULL);
}

DEFINE_PRIM(_VOID, set_main_loop, _FUN(_VOID, _NO_ARG) _I32);
DEFINE_PRIM(_I32, set_timeout, _FUN(_VOID, _NO_ARG) _I32 _BOOL);
DEFINE_PRIM(_VOID, clear_timeout, _I32);
DEFINE_PRIM(_VOID, async_sleep, _I32);
DEFINE_PRIM(_VOID, request_animation_frame, _FUN(_VOID, _F64));

#endif // HL_EMSCRIPTEN
