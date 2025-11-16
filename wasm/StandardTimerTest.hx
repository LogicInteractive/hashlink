// Test standard haxe.Timer in WASM (no custom WasmTimer API needed!)
class StandardTimerTest {
    static var counter:Int = 0;

    static function main() {
        trace("=== Standard haxe.Timer Test in WASM ===\n");

        // Test 1: One-shot timer (using delay)
        trace("[Test 1] One-shot timer (1 second)");
        haxe.Timer.delay(function() {
            trace("  ✓ One-shot timer executed!");
        }, 1000);

        // Test 2: Repeating timer
        trace("[Test 2] Repeating timer (500ms interval)");
        var repeatingTimer = new haxe.Timer(500);
        repeatingTimer.run = function() {
            counter++;
            trace('  Tick #$counter');

            if (counter >= 10) {
                trace("  ✓ Stopping after 10 ticks");
                repeatingTimer.stop();
            }
        };

        // Test 3: Multiple timers
        trace("[Test 3] Multiple simultaneous timers");
        haxe.Timer.delay(function() {
            trace("  Timer A: 2 seconds");
        }, 2000);

        haxe.Timer.delay(function() {
            trace("  Timer B: 3 seconds");
        }, 3000);

        haxe.Timer.delay(function() {
            trace("  Timer C: 4 seconds");
            trace("\n=== All Tests Complete ===");
        }, 4000);

        trace("\nTimers scheduled. Waiting for events...");
        trace("(This uses standard haxe.Timer - no custom API!)");
    }
}
