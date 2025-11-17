// Test HTTP functionality in WASM using browser XMLHttpRequest
class HttpTest {
    static var testsComplete = 0;
    static var totalTests = 3;

    static function main() {
        trace("=== Testing HTTP in WASM ===\n");

        testSimpleGET();
        testGETWithHeaders();
        testPOST();

        trace("\n=== All requests sent ===");
        trace("Waiting for responses...");
    }

    static function testSimpleGET() {
        trace("[Test 1] Simple GET request");
        trace("  URL: https://httpbin.org/get");

        var http = new WasmHttp("https://httpbin.org/get");

        http.onStatus = function(status) {
            trace('  Status: $status');
        };

        http.onData = function(data) {
            trace("  ✓ Success! Response length: " + data.length + " chars");
            trace("  First 100 chars: " + data.substr(0, 100));
            testComplete();
        };

        http.onError = function(error) {
            trace('  ✗ Error: $error');
            testComplete();
        };

        http.request();
    }

    static function testGETWithHeaders() {
        trace("\n[Test 2] GET with custom headers");
        trace("  URL: https://httpbin.org/headers");

        var http = new WasmHttp("https://httpbin.org/headers");
        http.setHeader("User-Agent", "HashLink-WASM/1.0");
        http.setHeader("X-Custom-Header", "Test-Value");

        http.onStatus = function(status) {
            trace('  Status: $status');
        };

        http.onData = function(data) {
            trace("  ✓ Success! Got response");
            // Check if our custom header is in the response
            if (data.indexOf("X-Custom-Header") >= 0) {
                trace("  ✓ Custom header sent successfully");
            }
            testComplete();
        };

        http.onError = function(error) {
            trace('  ✗ Error: $error');
            testComplete();
        };

        http.request();
    }

    static function testPOST() {
        trace("\n[Test 3] POST request with data");
        trace("  URL: https://httpbin.org/post");

        var http = new WasmHttp("https://httpbin.org/post");
        http.setHeader("Content-Type", "application/x-www-form-urlencoded");
        http.setPostData("key=value&foo=bar&test=hello");

        http.onStatus = function(status) {
            trace('  Status: $status');
        };

        http.onData = function(data) {
            trace("  ✓ Success! POST completed");
            // Check if our posted data is in the response
            if (data.indexOf("key=value") >= 0) {
                trace("  ✓ POST data received by server");
            }
            testComplete();
        };

        http.onError = function(error) {
            trace('  ✗ Error: $error');
            testComplete();
        };

        http.request(true);
    }

    static function testComplete() {
        testsComplete++;
        if (testsComplete >= totalTests) {
            trace("\n=== All Tests Complete ===");
            trace('Results: $testsComplete/$totalTests tests finished');
        }
    }
}
