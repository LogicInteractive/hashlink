// Minimal timer test - just one repeating timer
class SimpleTimerTest {
    static var count:Int = 0;

    static function main() {
        trace("=== Simple Timer Test ===");

        var timer = new haxe.Timer(500);
        timer.run = function() {
            count++;
            trace('Tick ${count}');

            if (count >= 10) {
                trace("Stopping timer");
                timer.stop();
                trace("Test complete!");
            }
        };

        trace("Timer started, will run 10 times");
    }
}
