// Haxe wrapper for WASM event loop and timers
#if hl

class WasmTimer {

    // Set main loop (for games, 60 FPS animations)
    public static function setMainLoop(callback:Void->Void, fps:Int = 60):Void {
        #if hl_emscripten
        _setMainLoop(callback, fps);
        #else
        throw "WasmTimer.setMainLoop only works in WASM builds";
        #end
    }

    // setTimeout equivalent - runs once after delay
    public static function setTimeout(callback:Void->Void, delayMs:Int):Int {
        #if hl_emscripten
        return _setTimeout(callback, delayMs, false);
        #else
        throw "WasmTimer.setTimeout only works in WASM builds";
        return -1;
        #end
    }

    // setInterval equivalent - runs repeatedly
    public static function setInterval(callback:Void->Void, intervalMs:Int):Int {
        #if hl_emscripten
        return _setTimeout(callback, intervalMs, true);
        #else
        throw "WasmTimer.setInterval only works in WASM builds";
        return -1;
        #end
    }

    // Clear timeout/interval
    public static function clearTimeout(timerId:Int):Void {
        #if hl_emscripten
        _clearTimeout(timerId);
        #end
    }

    // Non-blocking sleep (requires Asyncify)
    public static function sleep(ms:Int):Void {
        #if hl_emscripten
        _asyncSleep(ms);
        #else
        Sys.sleep(ms / 1000.0);
        #end
    }

    // Request animation frame (60 FPS, synced with display)
    public static function requestAnimationFrame(callback:Float->Void):Void {
        #if hl_emscripten
        _requestAnimationFrame(callback);
        #else
        throw "WasmTimer.requestAnimationFrame only works in WASM builds";
        #end
    }

    // Native bindings
    @:hlNative("std", "hl_set_main_loop")
    static function _setMainLoop(callback:Void->Void, fps:Int):Void {}

    @:hlNative("std", "hl_set_timeout")
    static function _setTimeout(callback:Void->Void, delayMs:Int, repeat:Bool):Int { return 0; }

    @:hlNative("std", "hl_clear_timeout")
    static function _clearTimeout(timerId:Int):Void {}

    @:hlNative("std", "hl_async_sleep")
    static function _asyncSleep(ms:Int):Void {}

    @:hlNative("std", "hl_request_animation_frame")
    static function _requestAnimationFrame(callback:Float->Void):Void {}
}

#end
