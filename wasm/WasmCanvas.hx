// Wrapper for WASM Canvas 2D support using browser Canvas API
class WasmCanvas {
    var canvasId:String;

    public function new(canvasId:String) {
        this.canvasId = canvasId;

        #if hl_emscripten
        _canvasInit(canvasId);
        #else
        trace("Canvas 2D not supported on this platform");
        #end
    }

    // Clear canvas with RGB color
    public function clear(r:Int = 0, g:Int = 0, b:Int = 0) {
        #if hl_emscripten
        _canvasClear(canvasId, r, g, b);
        #end
    }

    // Fill rectangle with RGBA color
    public function fillRect(x:Int, y:Int, width:Int, height:Int, r:Int = 255, g:Int = 255, b:Int = 255, a:Int = 255) {
        #if hl_emscripten
        _canvasFillRect(canvasId, x, y, width, height, r, g, b, a);
        #end
    }

    // Fill circle with RGBA color
    public function fillCircle(x:Int, y:Int, radius:Int, r:Int = 255, g:Int = 255, b:Int = 255, a:Int = 255) {
        #if hl_emscripten
        _canvasFillCircle(canvasId, x, y, radius, r, g, b, a);
        #end
    }

    // Draw line with RGBA color
    public function drawLine(x1:Int, y1:Int, x2:Int, y2:Int, lineWidth:Int = 1, r:Int = 255, g:Int = 255, b:Int = 255, a:Int = 255) {
        #if hl_emscripten
        _canvasDrawLine(canvasId, x1, y1, x2, y2, lineWidth, r, g, b, a);
        #end
    }

    // Draw text with RGBA color
    public function drawText(text:String, x:Int, y:Int, fontSize:Int = 16, r:Int = 255, g:Int = 255, b:Int = 255, a:Int = 255) {
        #if hl_emscripten
        _canvasDrawText(canvasId, text, x, y, fontSize, r, g, b, a);
        #end
    }

    // Native C function bindings
    @:hlNative("std", "canvas_init")
    static function _canvasInit(canvasId:String):Void {}

    @:hlNative("std", "canvas_clear")
    static function _canvasClear(canvasId:String, r:Int, g:Int, b:Int):Void {}

    @:hlNative("std", "canvas_fill_rect")
    static function _canvasFillRect(canvasId:String, x:Int, y:Int, width:Int, height:Int, r:Int, g:Int, b:Int, a:Int):Void {}

    @:hlNative("std", "canvas_fill_circle")
    static function _canvasFillCircle(canvasId:String, x:Int, y:Int, radius:Int, r:Int, g:Int, b:Int, a:Int):Void {}

    @:hlNative("std", "canvas_draw_line")
    static function _canvasDrawLine(canvasId:String, x1:Int, y1:Int, x2:Int, y2:Int, lineWidth:Int, r:Int, g:Int, b:Int, a:Int):Void {}

    @:hlNative("std", "canvas_draw_text")
    static function _canvasDrawText(canvasId:String, text:String, x:Int, y:Int, fontSize:Int, r:Int, g:Int, b:Int, a:Int):Void {}
}
