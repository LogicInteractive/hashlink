// Test standard haxe.Timer and async features in WASM
class AsyncTest {
    static var counter:Int = 0;

    static function main() {
        trace("=== Testing Standard haxe.Timer in WASM ===\n");

        testHaxeTimer();
        testHaxeTimerDelay();
        testMultipleTimers();
        testSysTime();
        testDate();

        trace("\n=== Tests Scheduled ===");
        trace("(Event loop will process timers automatically)");
    }

    static function testHaxeTimer() {
        trace("[Test 1] Standard haxe.Timer (repeating)");
        try {
            var timer = new haxe.Timer(500);  // 500ms interval
            timer.run = function() {
                counter++;
                trace('  Tick #$counter (using standard haxe.Timer!)');

                if (counter >= 5) {
                    trace('  ✓ Stopping after 5 ticks');
                    timer.stop();
                }
            };
            trace("  Timer created: 500ms interval");
        } catch (e:Dynamic) {
            trace('  ✗ Error creating timer: $e');
        }
    }

    static function testHaxeTimerDelay() {
        trace("\n[Test 2] haxe.Timer.delay (one-shot)");
        try {
            haxe.Timer.delay(function() {
                trace("  ✓ One-shot timer executed after 1 second!");
            }, 1000);
            trace("  Delay scheduled: 1000ms");
        } catch (e:Dynamic) {
            trace('  ✗ Error with delay: $e');
        }
    }

    static function testMultipleTimers() {
        trace("\n[Test 3] Multiple simultaneous timers");
        try {
            haxe.Timer.delay(function() {
                trace("  Timer A: Executed after 2 seconds");
            }, 2000);

            haxe.Timer.delay(function() {
                trace("  Timer B: Executed after 3 seconds");
            }, 3000);

            haxe.Timer.delay(function() {
                trace("  Timer C: Executed after 4 seconds");
                trace("\n  ✓ All timed events completed!");
            }, 4000);

            trace("  3 timers scheduled (2s, 3s, 4s)");
        } catch (e:Dynamic) {
            trace('  ✗ Error with multiple timers: $e');
        }
    }

    static function testSysTime() {
        trace("\n[Test 4] Sys.time() measurement");
        try {
            var t1 = Sys.time();
            // Do some work
            var sum = 0;
            for (i in 0...10000) {
                sum += i;
            }
            var t2 = Sys.time();
            var duration = t2 - t1;

            trace('  Duration: $duration seconds');
            trace('  ✓ Sys.time() works');
        } catch (e:Dynamic) {
            trace('  ✗ Sys.time() error: $e');
        }
    }

    static function testDate() {
        trace("\n[Test 5] Date.now()");
        try {
            var now = Date.now();
            trace('  Current: $now');
            trace('  Year: ${now.getFullYear()}');
            trace('  ✓ Date.now() works');
        } catch (e:Dynamic) {
            trace('  ✗ Date error: $e');
        }
    }
}
