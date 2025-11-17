# Graphics Support for HashLink WASM - Implementation Plan

**Status:** Planning
**Priority:** HIGH - Essential for games and visual applications
**Complexity:** MEDIUM-HIGH - Multiple API options

---

## Available Graphics APIs

### Option 1: HTML5 Canvas 2D ⭐ EASIEST
**Best for:** 2D games, simple drawing, UI rendering

**Pros:**
- ✅ Simple API
- ✅ Direct browser support
- ✅ No complex setup
- ✅ Good for 2D sprites, shapes, text

**Cons:**
- ❌ Not suitable for 3D
- ❌ Lower performance than WebGL

**Emscripten Support:**
- Direct JavaScript canvas API access via EM_JS
- Can get canvas context and draw from C

### Option 2: WebGL (OpenGL ES 2.0/3.0) ⭐ RECOMMENDED
**Best for:** 3D graphics, games, high-performance 2D

**Pros:**
- ✅ Hardware accelerated
- ✅ OpenGL-like API (familiar)
- ✅ Excellent Emscripten support
- ✅ Works with existing OpenGL code
- ✅ Wide browser support

**Cons:**
- ⚠️ More complex than Canvas 2D
- ⚠️ Requires shaders

**Emscripten Support:**
- Full WebGL bindings in `html5_webgl.h`
- Can use standard OpenGL ES calls
- Automatic translation to WebGL

### Option 3: WebGPU 🚀 CUTTING EDGE
**Best for:** Modern 3D graphics, compute shaders

**Pros:**
- ✅ Modern API
- ✅ Better performance than WebGL
- ✅ Compute shader support

**Cons:**
- ❌ Limited browser support (Chrome, Edge only)
- ❌ API still evolving
- ❌ More complex

---

## Recommended Approach: Start with Canvas 2D

For immediate results and ease of use, implement Canvas 2D first, then add WebGL support.

---

## Implementation: Canvas 2D Integration

### Architecture

```
Haxe Code (WasmCanvas.hx)
         ↓
@:hlNative("std", "canvas_*")
         ↓
C Bindings (canvas_wasm.c)
         ↓
EM_JS JavaScript Bridge
         ↓
Browser Canvas 2D API
```

### File 1: `src/std/canvas_wasm.c`

```c
#ifdef HL_EMSCRIPTEN
#include <emscripten.h>
#include <hl.h>

// Initialize canvas and get 2D context
EM_JS(int, canvas_init, (const char* canvasId), {
    const id = UTF8ToString(canvasId);
    const canvas = document.getElementById(id) || document.querySelector('canvas');

    if (!canvas) {
        console.error('Canvas not found:', id);
        return 0;
    }

    const ctx = canvas.getContext('2d');
    if (!ctx) {
        console.error('Could not get 2D context');
        return 0;
    }

    // Store context globally
    if (!Module.canvasContexts) {
        Module.canvasContexts = {};
    }
    Module.canvasContexts[id] = {
        canvas: canvas,
        ctx: ctx
    };

    return 1;  // Success
});

// Clear canvas
EM_JS(void, canvas_clear, (const char* canvasId, int r, int g, int b), {
    const id = UTF8ToString(canvasId);
    const data = Module.canvasContexts[id];
    if (!data) return;

    const ctx = data.ctx;
    const canvas = data.canvas;

    ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
    ctx.fillRect(0, 0, canvas.width, canvas.height);
});

// Draw filled rectangle
EM_JS(void, canvas_fill_rect, (const char* canvasId, float x, float y, float w, float h, int r, int g, int b, int a), {
    const id = UTF8ToString(canvasId);
    const data = Module.canvasContexts[id];
    if (!data) return;

    const ctx = data.ctx;
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${a / 255.0})`;
    ctx.fillRect(x, y, w, h);
});

// Draw circle
EM_JS(void, canvas_fill_circle, (const char* canvasId, float x, float y, float radius, int r, int g, int b, int a), {
    const id = UTF8ToString(canvasId);
    const data = Module.canvasContexts[id];
    if (!data) return;

    const ctx = data.ctx;
    ctx.fillStyle = `rgba(${r}, ${g}, ${b}, ${a / 255.0})`;
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fill();
});

// Draw line
EM_JS(void, canvas_draw_line, (const char* canvasId, float x1, float y1, float x2, float y2, int r, int g, int b, float width), {
    const id = UTF8ToString(canvasId);
    const data = Module.canvasContexts[id];
    if (!data) return;

    const ctx = data.ctx;
    ctx.strokeStyle = `rgb(${r}, ${g}, ${b})`;
    ctx.lineWidth = width;
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
});

// Draw text
EM_JS(void, canvas_draw_text, (const char* canvasId, const char* text, float x, float y, int size, int r, int g, int b), {
    const id = UTF8ToString(canvasId);
    const data = Module.canvasContexts[id];
    if (!data) return;

    const ctx = data.ctx;
    const textStr = UTF8ToString(text);

    ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
    ctx.font = `${size}px sans-serif`;
    ctx.fillText(textStr, x, y);
});

// HashLink primitives
HL_PRIM bool hl_canvas_init(vstring *canvasId) {
    const char *id = canvasId ? hl_to_utf8(canvasId->bytes) : "canvas";
    return canvas_init(id) == 1;
}

HL_PRIM void hl_canvas_clear(vstring *canvasId, int r, int g, int b) {
    const char *id = canvasId ? hl_to_utf8(canvasId->bytes) : "canvas";
    canvas_clear(id, r, g, b);
}

HL_PRIM void hl_canvas_fill_rect(vstring *canvasId, double x, double y, double w, double h,
                                  int r, int g, int b, int a) {
    const char *id = canvasId ? hl_to_utf8(canvasId->bytes) : "canvas";
    canvas_fill_rect(id, x, y, w, h, r, g, b, a);
}

HL_PRIM void hl_canvas_fill_circle(vstring *canvasId, double x, double y, double radius,
                                    int r, int g, int b, int a) {
    const char *id = canvasId ? hl_to_utf8(canvasId->bytes) : "canvas";
    canvas_fill_circle(id, x, y, radius, r, g, b, a);
}

HL_PRIM void hl_canvas_draw_line(vstring *canvasId, double x1, double y1, double x2, double y2,
                                  int r, int g, int b, double width) {
    const char *id = canvasId ? hl_to_utf8(canvasId->bytes) : "canvas";
    canvas_draw_line(id, x1, y1, x2, y2, r, g, b, width);
}

HL_PRIM void hl_canvas_draw_text(vstring *canvasId, vstring *text, double x, double y, int size,
                                  int r, int g, int b) {
    const char *id = canvasId ? hl_to_utf8(canvasId->bytes) : "canvas";
    const char *textStr = hl_to_utf8(text->bytes);
    canvas_draw_text(id, textStr, x, y, size, r, g, b);
}

DEFINE_PRIM(_BOOL, canvas_init, _STRING);
DEFINE_PRIM(_VOID, canvas_clear, _STRING _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_fill_rect, _STRING _F64 _F64 _F64 _F64 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_fill_circle, _STRING _F64 _F64 _F64 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_draw_line, _STRING _F64 _F64 _F64 _F64 _I32 _I32 _I32 _F64);
DEFINE_PRIM(_VOID, canvas_draw_text, _STRING _STRING _F64 _F64 _I32 _I32 _I32 _I32);

#endif // HL_EMSCRIPTEN
```

### File 2: `wasm/WasmCanvas.hx`

```haxe
class WasmCanvas {
    var canvasId:String;

    public function new(canvasId:String = "canvas") {
        this.canvasId = canvasId;

        #if hl_emscripten
        if (!_init(canvasId)) {
            trace('Failed to initialize canvas: $canvasId');
        }
        #end
    }

    public function clear(r:Int = 0, g:Int = 0, b:Int = 0) {
        #if hl_emscripten
        _clear(canvasId, r, g, b);
        #end
    }

    public function fillRect(x:Float, y:Float, w:Float, h:Float,
                             r:Int, g:Int, b:Int, a:Int = 255) {
        #if hl_emscripten
        _fillRect(canvasId, x, y, w, h, r, g, b, a);
        #end
    }

    public function fillCircle(x:Float, y:Float, radius:Float,
                               r:Int, g:Int, b:Int, a:Int = 255) {
        #if hl_emscripten
        _fillCircle(canvasId, x, y, radius, r, g, b, a);
        #end
    }

    public function drawLine(x1:Float, y1:Float, x2:Float, y2:Float,
                             r:Int, g:Int, b:Int, width:Float = 1.0) {
        #if hl_emscripten
        _drawLine(canvasId, x1, y1, x2, y2, r, g, b, width);
        #end
    }

    public function drawText(text:String, x:Float, y:Float, size:Int = 16,
                             r:Int = 0, g:Int = 0, b:Int = 0) {
        #if hl_emscripten
        _drawText(canvasId, text, x, y, size, r, g, b);
        #end
    }

    @:hlNative("std", "canvas_init")
    static function _init(id:String):Bool { return false; }

    @:hlNative("std", "canvas_clear")
    static function _clear(id:String, r:Int, g:Int, b:Int):Void {}

    @:hlNative("std", "canvas_fill_rect")
    static function _fillRect(id:String, x:Float, y:Float, w:Float, h:Float,
                              r:Int, g:Int, b:Int, a:Int):Void {}

    @:hlNative("std", "canvas_fill_circle")
    static function _fillCircle(id:String, x:Float, y:Float, radius:Float,
                                r:Int, g:Int, b:Int, a:Int):Void {}

    @:hlNative("std", "canvas_draw_line")
    static function _drawLine(id:String, x1:Float, y1:Float, x2:Float, y2:Float,
                              r:Int, g:Int, b:Int, width:Float):Void {}

    @:hlNative("std", "canvas_draw_text")
    static function _drawText(id:String, text:String, x:Float, y:Float, size:Int,
                              r:Int, g:Int, b:Int):Void {}
}
```

### File 3: `wasm/CanvasTest.hx`

```haxe
class CanvasTest {
    static var canvas:WasmCanvas;
    static var frame:Int = 0;
    static var ballX:Float = 100;
    static var ballY:Float = 100;
    static var ballVelX:Float = 3;
    static var ballVelY:Float = 2;

    static function main() {
        trace("=== Canvas 2D Test ===");

        // Initialize canvas
        canvas = new WasmCanvas("canvas");
        canvas.clear(50, 50, 50);  // Dark gray background

        // Draw some shapes
        trace("Drawing static shapes...");
        canvas.fillRect(10, 10, 100, 50, 255, 0, 0);  // Red rectangle
        canvas.fillCircle(200, 100, 30, 0, 255, 0);  // Green circle
        canvas.drawLine(10, 200, 300, 250, 0, 0, 255, 3);  // Blue line
        canvas.drawText("HashLink WASM Canvas!", 50, 300, 24, 255, 255, 255);

        // Start animation loop with timer
        trace("Starting animation...");
        var timer = new haxe.Timer(16);  // ~60 FPS
        timer.run = animate;
    }

    static function animate() {
        frame++;

        // Clear canvas
        canvas.clear(30, 30, 40);  // Dark blue-gray

        // Update bouncing ball
        ballX += ballVelX;
        ballY += ballVelY;

        // Bounce off edges (assuming 800x600 canvas)
        if (ballX < 20 || ballX > 780) ballVelX = -ballVelX;
        if (ballY < 20 || ballY > 580) ballVelY = -ballVelY;

        // Draw bouncing ball
        canvas.fillCircle(ballX, ballY, 20, 255, 100, 100, 255);

        // Draw frame counter
        canvas.drawText('Frame: $frame', 10, 30, 16, 255, 255, 255);

        // Draw some decorative elements
        for (i in 0...5) {
            var angle = (frame + i * 72) * 0.01;
            var x = 400 + Math.cos(angle) * 150;
            var y = 300 + Math.sin(angle) * 150;
            canvas.fillCircle(x, y, 10, 100, 200, 255, 200);
        }

        // Stop after 600 frames (10 seconds)
        if (frame >= 600) {
            trace("Animation complete!");
        }
    }
}
```

### File 4: `wasm/canvas_template.html`

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>HashLink WASM Canvas</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            background: #222;
            display: flex;
            flex-direction: column;
            align-items: center;
            font-family: sans-serif;
        }
        h1 {
            color: #fff;
            margin-bottom: 20px;
        }
        canvas {
            border: 2px solid #666;
            background: #000;
        }
        #info {
            color: #aaa;
            margin-top: 20px;
            max-width: 800px;
        }
    </style>
</head>
<body>
    <h1>HashLink WASM - Canvas 2D Graphics</h1>
    <canvas id="canvas" width="800" height="600"></canvas>
    <div id="info">
        <p>Canvas initialized. Open console (F12) for debug output.</p>
        <p>This demonstrates HashLink WASM drawing to HTML5 Canvas.</p>
    </div>

    <script>
        var Module = {
            print: function(text) {
                console.log(text);
            },
            printErr: function(text) {
                console.error(text);
            },
            canvas: document.getElementById('canvas')
        };
    </script>
    {{{ SCRIPT }}}
</body>
</html>
```

---

## WebGL Implementation (Option 2)

For 3D graphics and better performance, WebGL integration:

```c
// src/std/webgl_wasm.c
#ifdef HL_EMSCRIPTEN
#include <emscripten/html5_webgl.h>
#include <GLES3/gl3.h>

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = 0;

HL_PRIM bool hl_webgl_init(vstring *canvasId) {
    const char *id = hl_to_utf8(canvasId->bytes);

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;

    ctx = emscripten_webgl_create_context(id, &attrs);
    if (ctx <= 0) return false;

    emscripten_webgl_make_context_current(ctx);
    return true;
}

HL_PRIM void hl_webgl_clear(int r, int g, int b, int a) {
    glClearColor(r/255.0f, g/255.0f, b/255.0f, a/255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// ... more WebGL functions
#endif
```

---

## Build Integration

### Update `CMakeLists.txt`:

```cmake
# WASM event loop, HTTP, and graphics integration
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    list(APPEND std_srcs
        src/std/event_loop_wasm.c
        src/std/mainloop_wasm.c
        src/std/http_wasm.c
        src/std/canvas_wasm.c
    )
endif()
```

### Compile with graphics:

```bash
emcc canvastest.c -o canvastest.html \
    --shell-file canvas_template.html \
    -I. -I../src -L../build-wasm/bin -lhl \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s USE_WEBGL2=1 \
    -Oz
```

---

## Expected Results

**Canvas 2D Test:**
- Static shapes: red rectangle, green circle, blue line, white text
- Animated bouncing ball
- Rotating circles pattern
- Frame counter
- 60 FPS smooth animation

**Browser Output:**
```
=== Canvas 2D Test ===
Drawing static shapes...
Starting animation...
Frame: 1
Frame: 2
...
Animation complete!
```

---

## Performance Comparison

| API | Setup Complexity | Performance | Use Case |
|-----|-----------------|-------------|----------|
| Canvas 2D | Easy | Good for 2D | Simple games, UI |
| WebGL | Medium | Excellent | 3D games, effects |
| WebGPU | Hard | Best | Modern 3D apps |

---

## Next Steps

1. **Implement Canvas 2D** (recommended first)
   - Add canvas_wasm.c
   - Create WasmCanvas.hx wrapper
   - Build and test CanvasTest

2. **Add WebGL support** (for 3D)
   - Integrate html5_webgl.h
   - Create WebGL wrapper
   - Test with 3D cube

3. **Mouse/Keyboard Input**
   - Add event listeners
   - Create input wrapper
   - Make interactive demos

**Which graphics API should we implement first?**
