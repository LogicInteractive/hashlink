/**
 * HashLink WASM Test - Proof of Concept
 *
 * This test exercises core HashLink runtime features to validate
 * WASM compilation and execution.
 */
class Test {
    static function main() {
        trace("Hello from HashLink on WASM!");
        trace("");

        // Test basic operations
        trace("Testing basic operations:");

        // Integer arithmetic
        var sum = 0;
        for (i in 0...101) {
            sum += i;
        }
        trace('Sum 1-100: $sum');

        // String operations
        var str = "Hello";
        var world = " WASM";
        var combined = str + world;
        trace('String: $combined');

        // Float arithmetic
        var x = 10.5;
        var y = 32.0;
        var result = x + y;
        trace('Float math: $result');

        // Array operations
        var arr = [1, 2, 3, 4, 5];
        var arraySum = 0;
        for (n in arr) {
            arraySum += n;
        }
        trace('Array sum: $arraySum');

        // Object creation
        var obj = { name: "WASM Test", value: 42 };
        trace('Object: ${obj.name} = ${obj.value}');

        // Function calls
        var squared = square(7);
        trace('Function call: square(7) = $squared');

        // Type casting
        var dynamic:Dynamic = "123";
        var parsed = Std.parseInt(dynamic);
        trace('Type cast: "$dynamic" -> $parsed');

        // Map/Dictionary
        var map = new Map<String, Int>();
        map.set("one", 1);
        map.set("two", 2);
        map.set("three", 3);
        trace('Map size: ${map.keys().length}');

        // Class instantiation
        var counter = new Counter(10);
        counter.increment();
        counter.increment();
        trace('Counter value: ${counter.getValue()}');

        // Exception handling
        try {
            riskyOperation();
        } catch (e:Dynamic) {
            trace('Caught exception: $e');
        }

        trace("");
        trace("All tests completed successfully!");
        trace("HashLink WASM runtime is working!");
    }

    static function square(x:Int):Int {
        return x * x;
    }

    static function riskyOperation():Void {
        throw "This is a test exception";
    }
}

class Counter {
    var value:Int;

    public function new(initial:Int) {
        this.value = initial;
    }

    public function increment():Void {
        value++;
    }

    public function getValue():Int {
        return value;
    }
}
