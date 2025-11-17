// Ultra simple test - just return a value
class TestSimple {
    static function add(a:Int, b:Int):Int {
        return a + b;
    }

    static function main() {
        var result = add(5, 10);
        // Don't use trace - just compute
        var x = 0;
        for (i in 0...result) {
            x = x + 1;
        }
    }
}
