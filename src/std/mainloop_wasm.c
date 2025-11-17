/*
 * HashLink WASM - Standard haxe.MainLoop integration
 * Makes haxe.Timer and haxe.MainLoop work automatically in WASM
 */

#ifdef HL_EMSCRIPTEN
#include <emscripten.h>
#include <hl.h>

// Reference to haxe.MainLoop.tick function
static vclosure *mainloop_tick_closure = NULL;
static bool mainloop_running = false;

// Get the MainLoop.tick function dynamically
static vclosure* get_mainloop_tick() {
    if (mainloop_tick_closure != NULL) {
        return mainloop_tick_closure;
    }

    // Look up haxe.MainLoop.tick() function
    // The Haxe compiler generates: haxe_MainLoop_tick
    extern vdynamic* haxe_MainLoop_tick();

    // Create a closure for it
    hl_type_fun tf = { 0 };
    hl_type clt = { 0 };
    vclosure *cl = (vclosure*)hl_gc_alloc_noptr(sizeof(vclosure));

    tf.ret = &hlt_f64;  // Returns Float (wait time)
    clt.kind = HFUN;
    clt.fun = &tf;
    cl->t = &clt;
    cl->fun = haxe_MainLoop_tick;
    cl->hasValue = 1;

    mainloop_tick_closure = cl;
    return cl;
}

// Emscripten main loop callback - calls haxe.MainLoop.tick()
static void mainloop_tick_callback() {
    if (!mainloop_running) {
        return;
    }

    // Call haxe.MainLoop.tick() directly instead of through closure
    // This avoids potential issues with closure setup
    extern vdynamic* haxe_MainLoop_tick();

    vdynamic *result = haxe_MainLoop_tick();

    // tick() returns the wait time until next event
    // We run at ~60fps regardless to catch all events promptly
}

// Start the main loop automatically (called after main())
HL_PRIM void hl_mainloop_start() {
    if (mainloop_running) {
        return;  // Already running
    }

    mainloop_running = true;
    // Run at 60 FPS to process events promptly
    emscripten_set_main_loop(mainloop_tick_callback, 60, 1);
}

// Stop the main loop
HL_PRIM void hl_mainloop_stop() {
    mainloop_running = false;
    emscripten_cancel_main_loop();
}

// Check if MainLoop has events (called to see if we should keep running)
extern bool haxe_MainLoop_hasEvents();

HL_PRIM bool hl_mainloop_has_events() {
    return haxe_MainLoop_hasEvents();
}

DEFINE_PRIM(_VOID, mainloop_start, _NO_ARG);
DEFINE_PRIM(_VOID, mainloop_stop, _NO_ARG);
DEFINE_PRIM(_BOOL, mainloop_has_events, _NO_ARG);

#endif // HL_EMSCRIPTEN
