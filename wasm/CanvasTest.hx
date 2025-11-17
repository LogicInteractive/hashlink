// Animated Canvas 2D demo showcasing graphics capabilities
class CanvasTest {
    static var canvas:WasmCanvas;
    static var frame:Int = 0;

    // Bouncing ball state
    static var ballX:Float = 400;
    static var ballY:Float = 300;
    static var ballVX:Float = 3;
    static var ballVY:Float = 2;
    static var ballRadius:Int = 20;

    static function main() {
        trace("=== Canvas 2D Graphics Test ===");
        trace("Initializing canvas...");

        canvas = new WasmCanvas("gameCanvas");

        trace("Starting animation loop...");

        // Start 60 FPS animation loop using standard haxe.Timer
        var timer = new haxe.Timer(Math.floor(1000 / 60));
        timer.run = animationLoop;

        trace("Timer created, callback assigned");
        trace("Main() is about to exit - MainLoop should start automatically");
    }

    static function animationLoop() {
        if (frame == 0) {
            trace("🎬 Animation loop STARTED! First frame rendering...");
        }

        frame++;

        // Clear canvas with dark background
        canvas.clear(20, 20, 30);

        // Draw grid pattern
        drawGrid();

        // Draw bouncing ball
        drawBouncingBall();

        // Draw rotating circles
        drawRotatingCircles();

        // Draw decorative shapes
        drawShapes();

        // Draw text overlay
        drawTextOverlay();

        // Update physics
        updatePhysics();
    }

    static function drawGrid() {
        // Draw grid lines
        for (x in 0...9) {
            var xPos = x * 100;
            canvas.drawLine(xPos, 0, xPos, 600, 1, 40, 40, 50, 255);
        }

        for (y in 0...7) {
            var yPos = y * 100;
            canvas.drawLine(0, yPos, 800, yPos, 1, 40, 40, 50, 255);
        }
    }

    static function drawBouncingBall() {
        // Draw ball with gradient effect (multiple circles)
        canvas.fillCircle(Math.floor(ballX), Math.floor(ballY), ballRadius, 255, 100, 100, 255);
        canvas.fillCircle(Math.floor(ballX - 5), Math.floor(ballY - 5), ballRadius - 5, 255, 150, 150, 200);
        canvas.fillCircle(Math.floor(ballX - 8), Math.floor(ballY - 8), ballRadius - 10, 255, 200, 200, 150);
    }

    static function drawRotatingCircles() {
        var centerX = 400;
        var centerY = 300;
        var numCircles = 8;
        var radius = 150;

        for (i in 0...numCircles) {
            var angle = (frame * 0.02) + (i * Math.PI * 2 / numCircles);
            var x = Math.floor(centerX + Math.cos(angle) * radius);
            var y = Math.floor(centerY + Math.sin(angle) * radius);

            // Rainbow colors
            var hue = (i / numCircles) * 360;
            var rgb = hueToRgb(hue);

            canvas.fillCircle(x, y, 15, rgb.r, rgb.g, rgb.b, 200);
        }
    }

    static function drawShapes() {
        // Draw some decorative rectangles in corners
        canvas.fillRect(10, 10, 100, 50, 100, 200, 255, 150);
        canvas.fillRect(690, 10, 100, 50, 255, 200, 100, 150);
        canvas.fillRect(10, 540, 100, 50, 200, 100, 255, 150);
        canvas.fillRect(690, 540, 100, 50, 255, 255, 100, 150);

        // Draw animated border
        var pulse = Math.floor(128 + Math.sin(frame * 0.05) * 127);
        canvas.drawLine(0, 0, 800, 0, 3, pulse, 100, 200, 255);
        canvas.drawLine(800, 0, 800, 600, 3, pulse, 100, 200, 255);
        canvas.drawLine(800, 600, 0, 600, 3, pulse, 100, 200, 255);
        canvas.drawLine(0, 600, 0, 0, 3, pulse, 100, 200, 255);
    }

    static function drawTextOverlay() {
        canvas.drawText("HashLink WASM - Canvas 2D Demo", 250, 30, 24, 255, 255, 255, 255);
        canvas.drawText('Frame: $frame', 20, 580, 16, 200, 200, 200, 255);

        var fps = "60 FPS";
        canvas.drawText(fps, 720, 580, 16, 200, 255, 200, 255);

        canvas.drawText('Ball: (${Math.floor(ballX)}, ${Math.floor(ballY)})', 300, 580, 14, 150, 200, 255, 255);
    }

    static function updatePhysics() {
        // Update ball position
        ballX += ballVX;
        ballY += ballVY;

        // Bounce off walls
        if (ballX - ballRadius < 0 || ballX + ballRadius > 800) {
            ballVX = -ballVX;
            ballX = ballX < 400 ? ballRadius : 800 - ballRadius;
        }

        if (ballY - ballRadius < 0 || ballY + ballRadius > 600) {
            ballVY = -ballVY;
            ballY = ballY < 300 ? ballRadius : 600 - ballRadius;
        }
    }

    // Helper function to convert HSV hue to RGB
    static function hueToRgb(hue:Float):{r:Int, g:Int, b:Int} {
        var h = hue / 60;
        var c = 255;
        var x = Math.floor(c * (1 - Math.abs((h % 2) - 1)));

        var r:Int, g:Int, b:Int;

        if (h < 1) { r = c; g = x; b = 0; }
        else if (h < 2) { r = x; g = c; b = 0; }
        else if (h < 3) { r = 0; g = c; b = x; }
        else if (h < 4) { r = 0; g = x; b = c; }
        else if (h < 5) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        return {r: r, g: g, b: b};
    }
}
