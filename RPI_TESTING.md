# Testing ARM64 JIT on Raspberry Pi

## Quick Start

### 1. Get the Code on Your Raspberry Pi

```bash
# Clone the repository and checkout the branch
git clone https://github.com/LogicInteractive/hashlink.git
cd hashlink
git checkout claude/arm-port-vi-01W7cnxC7ajBnBH9UTafTUX5-01P5UrNX1XKXvnZ15A28PbET
```

### 2. Install Dependencies

```bash
# On Raspberry Pi OS (Debian-based)
sudo apt-get update
sudo apt-get install -y build-essential libpthread-stubs0-dev libpcre3-dev haxe
```

### 3. Build HashLink with ARM64 JIT

```bash
make clean
make
```

This should compile natively on the Raspberry Pi with ARM64 JIT enabled automatically.

### 4. Verify the Build

```bash
./hl --version
# Should output: 1.16.0

file hl libhl.so
# Should show: ARM aarch64 executables
```

### 5. Test with C Program (Quick Validation)

```bash
# Compile the test program
gcc -O3 -DHL_64 -DHL_JIT_ARM64 -I. -Isrc -o test_jit_execute test_jit_execute.c -L. -lhl -lpthread -lm

# Run it
LD_LIBRARY_PATH=. ./test_jit_execute
```

Expected output:
```
HashLink ARM64 JIT Direct Execution Test
=========================================

Test 1: HashLink initialization... OK
Test 2: Architecture check... ARM64 ✓
Test 3: JIT backend availability... OK (JIT functions linked)

Summary:
--------
✓ HashLink runtime initialized
✓ ARM64 JIT backend detected
✓ Ready for bytecode execution
```

### 6. Test with Haxe Programs

#### Test 1: Basic Arithmetic and Loops

```bash
# TestBasic.hx is already in the repo
haxe -hl test_basic.hl -main TestBasic
LD_LIBRARY_PATH=. ./hl test_basic.hl
```

**Current Expected Output:**
```
SIGNAL 11
```

⚠️ **Note**: Haxe programs currently crash. The JIT compiles successfully but crashes at runtime. This is a known issue being actively debugged.

#### Test 2: Simple Program (No Arrays)

```bash
# TestSimple.hx is also in the repo
haxe -hl test_simple.hl -main TestSimple
LD_LIBRARY_PATH=. ./hl test_simple.hl
```

#### Test 3: Your Own Haxe Program

Create a test file:

```haxe
class MyTest {
    static function factorial(n:Int):Int {
        if (n <= 1) return 1;
        return n * factorial(n - 1);
    }

    static function main() {
        trace("Factorial test:");
        for (i in 1...11) {
            trace('  $i! = ${factorial(i)}');
        }
    }
}
```

Compile and run:

```bash
haxe -hl mytest.hl -main MyTest
LD_LIBRARY_PATH=. ./hl mytest.hl
```

## Current Status ⚠️

**AS OF 2025-11-17: Programs currently crash at runtime**

The ARM64 JIT implementation has made significant progress but is **not yet functional** for running Haxe programs. Current issues:

❌ **Runtime Crash**: All programs (even empty main() functions) crash with SIGSEGV at PC=0x0
❌ **Root Cause**: JIT-compiled code attempts to call a NULL function pointer
❌ **Under Investigation**: vclosure offset bugs have been fixed, but additional issues remain

### What's Been Fixed
✅ **MOVZ/MOVK Encoding**: Critical instruction encoding bugs resolved
✅ **vclosure Offsets**: Fixed closure->fun, closure->value, closure->hasValue loading
✅ **GET_REG Issues**: All 38 operations converted to stack-based access
✅ **Build System**: Compiles successfully with no errors
✅ **Infrastructure**: LOAD_VREG/STORE_VREG macros, instruction cache flush

### What Needs Debugging
❌ **NULL Function Pointer**: Entry point function calls address 0x0
❌ **Global Functions**: May not be properly initialized for JIT access
❌ **Indirect Calls**: One or more operations generating invalid BLR instructions

**See ARM64_VCLOSURE_INVESTIGATION.md for detailed debugging information.**

## Troubleshooting

### If you get "command not found: haxe"

```bash
# Install Haxe from apt
sudo apt-get install haxe

# Or download from haxe.org if you need a newer version
```

### If you get "Failed to load function std@..."

Make sure to set LD_LIBRARY_PATH:
```bash
LD_LIBRARY_PATH=. ./hl your_program.hl
```

Or install libhl.so system-wide:
```bash
sudo make install
# Then you can run: ./hl your_program.hl
```

### If the build fails

Check that you're on a 64-bit Raspberry Pi OS:
```bash
uname -m
# Should output: aarch64

# If it shows "armv7l" or similar, you need 64-bit OS
```

## Performance Testing

To see how fast the JIT is:

```haxe
class Benchmark {
    static function fibonacci(n:Int):Int {
        if (n <= 1) return n;
        return fibonacci(n-1) + fibonacci(n-2);
    }

    static function main() {
        var start = Sys.time();
        var result = fibonacci(35);
        var elapsed = Sys.time() - start;
        trace('fib(35) = $result');
        trace('Time: ${elapsed}s');
    }
}
```

```bash
haxe -hl benchmark.hl -main Benchmark
time LD_LIBRARY_PATH=. ./hl benchmark.hl
```

## Reporting Results

Please report back:
1. Which Raspberry Pi model you're using (3, 4, 5, Zero 2 W)
2. Output of `cat /proc/cpuinfo | grep Model`
3. Whether the basic tests pass
4. Any errors or issues you encounter

## Next Steps After Testing

Once basic testing is complete, we can:
1. Implement the missing 4% (closures + exceptions)
2. Optimize performance-critical paths
3. Add more comprehensive tests
4. Submit pull request to upstream HashLink

---

**Status**: ARM64 JIT compiles but crashes at runtime - debugging in progress
**Last Updated**: 2025-11-17 (vclosure offset bugs fixed, but crash persists)
**Branch**: claude/arm-port-vi-01W7cnxC7ajBnBH9UTafTUX5-01P5UrNX1XKXvnZ15A28PbET
**Latest Commit**: 87f051b (Fix ARM64 vclosure structure field offset bugs)
**Investigation Report**: See ARM64_VCLOSURE_INVESTIGATION.md
