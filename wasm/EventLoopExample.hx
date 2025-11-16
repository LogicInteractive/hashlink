// Example: Using WASM Event Loop and Timers
class EventLoopExample {
    static var counter:Int = 0;
    static var animationTime:Float = 0;

    static function main() {
        trace("=== WASM Event Loop Example ===\n");

        // Example 1: setTimeout (runs once)
        trace("[setTimeout Example]");
        trace("Setting timeout for 1 second...");
        WasmTimer.setTimeout(function() {
            trace("✓ Timeout fired after 1 second!");
        }, 1000);

        // Example 2: setInterval (runs repeatedly)
        trace("\n[setInterval Example]");
        trace("Starting interval (every 500ms, 5 times)...");
        var intervalId = WasmTimer.setInterval(function() {
            counter++;
            trace('  Interval tick #$counter');
            if (counter >= 5) {
                WasmTimer.clearTimeout(intervalId);
                trace("✓ Interval stopped after 5 ticks");
                startGameLoop();
            }
        }, 500);
    }

    static function startGameLoop() {
        trace("\n[Game Loop Example]");
        trace("Starting 60 FPS game loop...");

        var frameCount = 0;
        var startTime = Sys.time();

        // Using main loop (good for games)
        WasmTimer.setMainLoop(function() {
            frameCount++;
            var elapsed = Sys.time() - startTime;

            // Run for 3 seconds
            if (elapsed >= 3.0) {
                var fps = frameCount / elapsed;
                trace('✓ Game loop completed!');
                trace('  Frames: $frameCount');
                trace('  Duration: ${Math.round(elapsed * 100) / 100}s');
                trace('  Average FPS: ${Math.round(fps)}');

                // Stop the loop
                WasmTimer.setMainLoop(null, 0);
                startAnimationFrame();
            }
        }, 60);
    }

    static function startAnimationFrame() {
        trace("\n[RequestAnimationFrame Example]");
        trace("Using requestAnimationFrame (synced with display)...");

        var rafFrames = 0;
        var rafStart = Sys.time();

        function rafLoop(time:Float) {
            rafFrames++;
            var elapsed = Sys.time() - rafStart;

            if (elapsed < 2.0) {
                WasmTimer.requestAnimationFrame(rafLoop);
            } else {
                var fps = rafFrames / elapsed;
                trace('✓ Animation frame test completed!');
                trace('  Frames: $rafFrames');
                trace('  Duration: ${Math.round(elapsed * 100) / 100}s');
                trace('  Average FPS: ${Math.round(fps)}');
                trace("\n=== All Event Loop Tests Complete! ===");
            }
        }

        WasmTimer.requestAnimationFrame(rafLoop);
    }
}
