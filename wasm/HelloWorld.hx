class HelloWorld {
    static function main() {
        trace("Hello from Haxe compiled to HashLink HL-C!");
        trace("This is REAL Haxe code running on WASM!");

        var x = 10;
        var y = 32;
        trace('Math test: $x + $y = ${x + y}');

        trace("Success! Haxe → HL-C → WASM pipeline working!");
    }
}
