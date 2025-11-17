#ifdef HL_EMSCRIPTEN
#include <emscripten.h>
#include <hl.h>

// JavaScript Canvas 2D API bindings using EM_JS
EM_JS(void, js_canvas_init, (const char* canvasId), {
    const id = UTF8ToString(canvasId);
    const canvas = document.getElementById(id);
    if (!canvas) {
        console.error('Canvas element not found:', id);
        return;
    }

    // Store canvas reference globally
    if (!Module.canvases) Module.canvases = {};
    Module.canvases[id] = canvas;

    // Get 2D context
    const ctx = canvas.getContext('2d');
    if (!Module.canvasContexts) Module.canvasContexts = {};
    Module.canvasContexts[id] = ctx;

    console.log('Canvas initialized:', id, canvas.width, 'x', canvas.height);
});

EM_JS(void, js_canvas_clear, (const char* canvasId, int r, int g, int b), {
    const id = UTF8ToString(canvasId);
    const ctx = Module.canvasContexts ? Module.canvasContexts[id] : null;
    if (!ctx) {
        console.error('Canvas context not found:', id);
        return;
    }

    // Debug: log first 3 clears
    if (!Module.clearCount) Module.clearCount = 0;
    if (Module.clearCount < 3) {
        console.log('🎨 js_canvas_clear called:', id, 'rgb(' + r + ',' + g + ',' + b + ')', 'ctx:', ctx);
        Module.clearCount++;
    }

    const canvas = Module.canvases[id];
    ctx.fillStyle = 'rgb(' + r + ',' + g + ',' + b + ')';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
});

EM_JS(void, js_canvas_fill_rect, (const char* canvasId, int x, int y, int width, int height, int r, int g, int b, int a), {
    const id = UTF8ToString(canvasId);
    const ctx = Module.canvasContexts ? Module.canvasContexts[id] : null;
    if (!ctx) return;

    ctx.fillStyle = 'rgba(' + r + ',' + g + ',' + b + ',' + (a / 255.0) + ')';
    ctx.fillRect(x, y, width, height);
});

EM_JS(void, js_canvas_fill_circle, (const char* canvasId, int x, int y, int radius, int r, int g, int b, int a), {
    const id = UTF8ToString(canvasId);
    const ctx = Module.canvasContexts ? Module.canvasContexts[id] : null;
    if (!ctx) {
        console.error('Canvas context not found for fillCircle:', id);
        return;
    }

    // Debug: log first 3 circles
    if (!Module.circleCount) Module.circleCount = 0;
    if (Module.circleCount < 3) {
        console.log('⭕ js_canvas_fill_circle called:', id, 'pos:(' + x + ',' + y + ') r:' + radius, 'rgba(' + r + ',' + g + ',' + b + ',' + a + ')', 'ctx:', ctx);
        Module.circleCount++;
    }

    ctx.fillStyle = 'rgba(' + r + ',' + g + ',' + b + ',' + (a / 255.0) + ')';
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fill();
});

EM_JS(void, js_canvas_draw_line, (const char* canvasId, int x1, int y1, int x2, int y2, int lineWidth, int r, int g, int b, int a), {
    const id = UTF8ToString(canvasId);
    const ctx = Module.canvasContexts ? Module.canvasContexts[id] : null;
    if (!ctx) return;

    ctx.strokeStyle = 'rgba(' + r + ',' + g + ',' + b + ',' + (a / 255.0) + ')';
    ctx.lineWidth = lineWidth;
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
});

EM_JS(void, js_canvas_draw_text, (const char* canvasId, const char* text, int x, int y, int fontSize, int r, int g, int b, int a), {
    const id = UTF8ToString(canvasId);
    const ctx = Module.canvasContexts ? Module.canvasContexts[id] : null;
    if (!ctx) return;

    const textStr = UTF8ToString(text);
    ctx.fillStyle = 'rgba(' + r + ',' + g + ',' + b + ',' + (a / 255.0) + ')';
    ctx.font = fontSize + 'px monospace';
    ctx.fillText(textStr, x, y);
});

// HashLink string to UTF8
static const char* vstring_to_utf8(vstring *str) {
    if (str == NULL) return NULL;
    return hl_to_utf8(str->bytes);
}

// HashLink primitives
HL_PRIM void hl_canvas_init(vstring *canvasId) {
    const char *id = vstring_to_utf8(canvasId);
    js_canvas_init(id);
}

HL_PRIM void hl_canvas_clear(vstring *canvasId, int r, int g, int b) {
    EM_ASM({ console.log('🔧 HL_PRIM hl_canvas_clear called'); });
    const char *id = vstring_to_utf8(canvasId);
    EM_ASM({ console.log('🔧 String converted, calling js_canvas_clear...'); });
    js_canvas_clear(id, r, g, b);
    EM_ASM({ console.log('🔧 js_canvas_clear returned'); });
}

HL_PRIM void hl_canvas_fill_rect(vstring *canvasId, int x, int y, int width, int height, int r, int g, int b, int a) {
    const char *id = vstring_to_utf8(canvasId);
    js_canvas_fill_rect(id, x, y, width, height, r, g, b, a);
}

HL_PRIM void hl_canvas_fill_circle(vstring *canvasId, int x, int y, int radius, int r, int g, int b, int a) {
    EM_ASM({ console.log('🔧 HL_PRIM hl_canvas_fill_circle called'); });
    const char *id = vstring_to_utf8(canvasId);
    js_canvas_fill_circle(id, x, y, radius, r, g, b, a);
}

HL_PRIM void hl_canvas_draw_line(vstring *canvasId, int x1, int y1, int x2, int y2, int lineWidth, int r, int g, int b, int a) {
    const char *id = vstring_to_utf8(canvasId);
    js_canvas_draw_line(id, x1, y1, x2, y2, lineWidth, r, g, b, a);
}

HL_PRIM void hl_canvas_draw_text(vstring *canvasId, vstring *text, int x, int y, int fontSize, int r, int g, int b, int a) {
    const char *id = vstring_to_utf8(canvasId);
    const char *txt = vstring_to_utf8(text);
    js_canvas_draw_text(id, txt, x, y, fontSize, r, g, b, a);
}

// Register primitives with HashLink
DEFINE_PRIM(_VOID, canvas_init, _STRING);
DEFINE_PRIM(_VOID, canvas_clear, _STRING _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_fill_rect, _STRING _I32 _I32 _I32 _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_fill_circle, _STRING _I32 _I32 _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_draw_line, _STRING _I32 _I32 _I32 _I32 _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, canvas_draw_text, _STRING _STRING _I32 _I32 _I32 _I32 _I32 _I32 _I32);

#endif
