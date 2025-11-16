// Test async features, timers, and event loop in WASM
class AsyncTest {
    static function main() {
        trace("=== Testing Async Features in WASM ===\n");

        testTimers();
        testSleep();
        testDate();
        testSysTime();

        trace("\n=== Async Tests Complete ===");
    }

    static function testTimers() {
        trace("[Timers]");
        var start = Sys.time();
        trace('  Start time: $start');

        // Note: haxe.Timer requires event loop which may not work in WASM
        try {
            var elapsed = Sys.time() - start;
            trace('  Elapsed: $elapsed seconds');
            trace('  ✓ Sys.time() works');
        } catch (e:Dynamic) {
            trace('  ✗ Error: $e');
        }
    }

    static function testSleep() {
        trace("\n[Sleep]");
        try {
            var before = Sys.time();
            // Note: Sys.sleep blocks the main thread
            // In WASM this may not work as expected
            trace('  Attempting Sys.sleep(0.1)...');
            Sys.sleep(0.1);
            var after = Sys.time();
            var duration = after - before;
            trace('  Sleep duration: $duration seconds');

            if (duration >= 0.09 && duration <= 0.2) {
                trace('  ✓ Sys.sleep() works');
            } else {
                trace('  ⚠ Sleep worked but timing seems off');
            }
        } catch (e:Dynamic) {
            trace('  ✗ Sys.sleep() not supported: $e');
        }
    }

    static function testDate() {
        trace("\n[Date/Time]");
        try {
            var now = Date.now();
            trace('  Current date: $now');
            trace('  Timestamp: ${now.getTime()}');
            trace('  Year: ${now.getFullYear()}');
            trace('  Month: ${now.getMonth() + 1}');
            trace('  Day: ${now.getDate()}');
            trace('  ✓ Date.now() works');
        } catch (e:Dynamic) {
            trace('  ✗ Date error: $e');
        }
    }

    static function testSysTime() {
        trace("\n[System Time]");
        try {
            var t1 = Sys.time();
            // Do some work
            var sum = 0;
            for (i in 0...1000) {
                sum += i;
            }
            var t2 = Sys.time();
            var duration = t2 - t1;

            trace('  Time 1: $t1');
            trace('  Time 2: $t2');
            trace('  Duration: $duration seconds');
            trace('  Sum (for work): $sum');

            if (duration >= 0) {
                trace('  ✓ Sys.time() measurement works');
            }
        } catch (e:Dynamic) {
            trace('  ✗ Sys.time() error: $e');
        }
    }
}
