// Comprehensive test: HTTP + Timers working together
class FullTest {
    static var requestsComplete = 0;
    static var totalRequests = 3;
    static var timerTicks = 0;

    static function main() {
        trace("=== Full WASM Test: HTTP + Timers ===\n");

        // Start a timer that runs during HTTP requests
        trace("[Timer] Starting heartbeat timer...");
        var heartbeat = new haxe.Timer(1000);
        heartbeat.run = function() {
            timerTicks++;
            trace('[Timer] Heartbeat tick #$timerTicks');

            if (timerTicks >= 10) {
                heartbeat.stop();
                trace("[Timer] Heartbeat stopped after 10 ticks");
            }
        };

        // Delayed HTTP request using standard haxe.Timer.delay()
        trace("[Timer] Scheduling delayed HTTP request (2 seconds)...\n");
        haxe.Timer.delay(function() {
            trace("[HTTP] Timer triggered! Making delayed HTTP request...");
            makeHttpRequest("https://httpbin.org/delay/1", "Delayed GET");
        }, 2000);

        // Immediate HTTP requests
        makeHttpRequest("https://httpbin.org/get", "Simple GET");
        makeHttpRequest("https://httpbin.org/user-agent", "User-Agent GET");

        trace("\n=== Tests started ===");
        trace("Timers are running...");
        trace("HTTP requests are in flight...");
    }

    static function makeHttpRequest(url:String, label:String) {
        trace('[HTTP] Requesting: $label');
        trace('       URL: $url');

        var http = new WasmHttp(url);
        http.setHeader("User-Agent", "HashLink-WASM-Full-Test/1.0");

        http.onStatus = function(status) {
            trace('       Status: $status');
        };

        http.onData = function(data) {
            trace('       ✓ Success! Response: ${data.length} chars');
            requestComplete(label);
        };

        http.onError = function(error) {
            trace('       ✗ Error: $error');
            requestComplete(label);
        };

        http.request();
    }

    static function requestComplete(label:String) {
        requestsComplete++;
        trace('[HTTP] Completed: $label ($requestsComplete/$totalRequests)');

        if (requestsComplete >= totalRequests) {
            trace("\n=== All HTTP Requests Complete ===");
            trace('Timer ticks so far: $timerTicks');
            trace("(Timer will continue until 10 ticks)");
        }
    }
}
