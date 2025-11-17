// Basic test that should work with ARM64 JIT
class TestBasic {
    static function add(a:Int, b:Int):Int {
        return a + b;
    }

    static function fibonacci(n:Int):Int {
        if (n <= 1) return n;
        var a = 0;
        var b = 1;
        for (i in 2...n+1) {
            var temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }

    static function main() {
        trace("=== ARM64 JIT Test ===");
        trace("");

        // Test 1: Simple arithmetic
        var x = 5;
        var y = 10;
        var sum = add(x, y);
        trace('Test 1 - Addition: $x + $y = $sum');

        // Test 2: Loops
        trace("");
        trace("Test 2 - Fibonacci sequence:");
        for (i in 0...10) {
            trace('  fib($i) = ${fibonacci(i)}');
        }

        // Test 3: Conditionals
        trace("");
        trace("Test 3 - Conditionals:");
        var num = 42;
        if (num > 40) {
            trace('  $num is greater than 40');
        } else {
            trace('  $num is less than or equal to 40');
        }

        // Test 4: Arrays
        trace("");
        trace("Test 4 - Arrays:");
        var numbers = [1, 2, 3, 4, 5];
        var total = 0;
        for (n in numbers) {
            total += n;
        }
        trace('  Array: [1,2,3,4,5]');
        trace('  Sum: $total');

        // Test 5: Strings
        trace("");
        trace("Test 5 - Strings:");
        var hello = "Hello";
        var world = "World";
        var message = hello + " " + world + "!";
        trace('  $message');

        trace("");
        trace("=== All tests passed! ===");
    }
}
