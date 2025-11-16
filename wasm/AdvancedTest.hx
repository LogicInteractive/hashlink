// Advanced Haxe Features Test for WASM
class AdvancedTest {
    static function main() {
        trace("=== Advanced Haxe WASM Test ===");

        testEnums();
        testAbstractTypes();
        testGenerics();
        testAnonymousStructures();
        testIterators();
        testStringInterpolation();
        testMath();
        testArrayOperations();
        testExceptions();

        trace("=== All Advanced Tests Passed! ===");
    }

    static function testEnums() {
        trace("\n[Enums]");
        var color = Color.RGB(255, 128, 0);
        switch(color) {
            case Color.RGB(r, g, b):
                trace('  RGB Color: R=$r G=$g B=$b');
            case Color.HSL(h, s, l):
                trace('  HSL Color: H=$h S=$s L=$l');
        }
    }

    static function testAbstractTypes() {
        trace("\n[Abstract Types]");
        var meters:Meters = new Meters(100);
        var km = meters.toKilometers();
        trace('  $meters meters = $km kilometers');
    }

    static function testGenerics() {
        trace("\n[Generics]");
        var box = new Box<String>("Hello WASM");
        trace('  Box contains: ${box.value}');

        var intBox = new Box<Int>(42);
        trace('  IntBox contains: ${intBox.value}');
    }

    static function testAnonymousStructures() {
        trace("\n[Anonymous Structures]");
        var person = { name: "Alice", age: 30, city: "Tokyo" };
        trace('  Person: ${person.name}, ${person.age}, ${person.city}');

        var point:{x:Float, y:Float} = { x: 10.5, y: 20.3 };
        trace('  Point: (${point.x}, ${point.y})');
    }

    static function testIterators() {
        trace("\n[Iterators]");
        var range = new IntRange(1, 5);
        var values = [];
        for (i in range) {
            values.push(i);
        }
        trace('  Range 1-5: ${values.join(", ")}');
    }

    static function testStringInterpolation() {
        trace("\n[String Interpolation]");
        var name = "WASM";
        var version = 1.0;
        var message = 'Running $name v$version';
        trace('  $message');

        var complex = 'Math: ${10 + 20} = ${10 * 3}';
        trace('  $complex');
    }

    static function testMath() {
        trace("\n[Math Operations]");
        var a = 15.7;
        var b = 4.3;
        trace('  $a + $b = ${a + b}');
        trace('  sqrt(${a * a}) = ${Math.sqrt(a * a)}');
        trace('  sin(PI/2) = ${Math.sin(Math.PI / 2)}');
        trace('  max($a, $b) = ${Math.max(a, b)}');
    }

    static function testArrayOperations() {
        trace("\n[Array Operations]");
        var numbers = [1, 2, 3, 4, 5];
        var doubled = numbers.map(x -> x * 2);
        trace('  Map x*2: ${doubled.join(", ")}');

        var evens = numbers.filter(x -> x % 2 == 0);
        trace('  Filter evens: ${evens.join(", ")}');

        var sum = Lambda.fold(numbers, (x, acc) -> x + acc, 0);
        trace('  Sum: $sum');
    }

    static function testExceptions() {
        trace("\n[Exceptions]");
        try {
            throw new CustomException("Test error", 404);
        } catch (e:CustomException) {
            trace('  Caught: ${e.message} (code: ${e.code})');
        }
    }
}

enum Color {
    RGB(r:Int, g:Int, b:Int);
    HSL(h:Float, s:Float, l:Float);
}

abstract Meters(Float) {
    public inline function new(v:Float) this = v;
    public inline function toKilometers():Float return this / 1000;
    @:to public inline function toString():String return Std.string(this);
}

class Box<T> {
    public var value:T;
    public function new(v:T) {
        this.value = v;
    }
}

class IntRange {
    var min:Int;
    var max:Int;

    public function new(min:Int, max:Int) {
        this.min = min;
        this.max = max;
    }

    public function iterator() {
        return new IntRangeIterator(min, max);
    }
}

class IntRangeIterator {
    var current:Int;
    var max:Int;

    public function new(min:Int, max:Int) {
        this.current = min;
        this.max = max;
    }

    public function hasNext():Bool {
        return current <= max;
    }

    public function next():Int {
        return current++;
    }
}

class CustomException {
    public var message:String;
    public var code:Int;

    public function new(msg:String, code:Int) {
        this.message = msg;
        this.code = code;
    }
}
