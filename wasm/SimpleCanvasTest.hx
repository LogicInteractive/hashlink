// Simple Canvas 2D test without animation to verify basic functionality
class SimpleCanvasTest {
    static function main() {
        trace("=== Simple Canvas Test (No Animation) ===");

        #if hl_emscripten
        trace("Initializing canvas...");
        var canvas = new WasmCanvas("gameCanvas");

        trace("Drawing shapes...");

        // Clear to dark blue
        canvas.clear(20, 20, 40);
        trace("✓ Cleared canvas");

        // Draw a red rectangle
        canvas.fillRect(100, 100, 200, 150, 255, 0, 0, 255);
        trace("✓ Drew red rectangle");

        // Draw a green circle
        canvas.fillCircle(400, 300, 80, 0, 255, 0, 255);
        trace("✓ Drew green circle");

        // Draw a blue line
        canvas.drawLine(50, 50, 750, 550, 5, 0, 100, 255, 255);
        trace("✓ Drew blue line");

        // Draw white text
        canvas.drawText("HashLink WASM Canvas 2D", 250, 50, 24, 255, 255, 255, 255);
        trace("✓ Drew text");

        trace("=== Test Complete ===");
        trace("All canvas operations succeeded!");
        #else
        trace("ERROR: This test requires WASM compilation");
        #end
    }
}
