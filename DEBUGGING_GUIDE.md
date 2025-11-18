# ARM64 JIT Debugging Guide

## Quick Reference for Debugging the Current NULL Pointer Issue

### Current Status

**Problem**: JIT code calls `hl_type_get_global(NULL)`, causing segfault
**Location**: `hl_type_get_global()` in libhl.so
**Caller**: JIT code at address 0x7ffff5c350ac

### Debug Commands

#### Build and Run
```bash
# Clean build
make clean && make

# Run with separated output
LD_LIBRARY_PATH=. ./hl /tmp/hello.hl 2>stderr.txt >stdout.txt
echo "Exit code: $?"

# View debug output
cat stderr.txt  # Trampoline setup messages
tail -50 stdout.txt  # Last operations before crash
```

#### GDB Debugging
```bash
# Basic crash analysis
LD_LIBRARY_PATH=. gdb ./hl
break hl_type_get_global
run /tmp/hello.hl
# When it breaks:
backtrace
info registers
x/10i $x30-32  # Disassemble caller (JIT code)
x/20i $pc      # Disassemble crash site
```

#### Advanced GDB - Examine JIT Code
```bash
LD_LIBRARY_PATH=. gdb ./hl
break hl_jit_code_arm64
run /tmp/hello.hl
continue  # Let it finish JIT compilation

# Set breakpoint at JIT code start
# Find entry point from debug output or:
print m->functions_ptrs[0]
break *0x<address>

continue
# Now examine registers, step through instructions
```

#### Disassemble Specific JIT Code Region
```bash
LD_LIBRARY_PATH=. gdb ./hl
run /tmp/hello.hl
# After crash:
x/100i 0x7ffff5c35000  # Start of JIT code region
x/50i 0x7ffff5c350ac-64  # Around crash call site
```

### Debug Output Interpretation

#### Successful Trampoline Setup
```
[JIT_CODE] Using HL_JIT_ARM64 path
[ARM64_CODE] hl_jit_code_arm64 called!
[TRAMPOLINE_CHECK] Reached trampoline setup code, call_jit_c2hl=(nil)
[TRAMPOLINE] Setting up ARM64 trampolines
[TRAMPOLINE] hl_setup.static_call=0x5555a8919bb4 (callback_c2hl_arm64)
[TRAMPOLINE] hl_setup.get_wrapper=0x5555a8919b00 (get_wrapper_arm64)
```

This means trampolines initialized correctly.

#### JIT Compilation Progress
```
[JIT] Function 0 assigned address at offset 0x0
[PROLOGUE] MOV X29, SP instruction at offset 12: 0x910003fd
[OGetGlobal] global 6 → type.kind=11
[OCallClosure] closure_type=10, nargs=2
```

Shows which operations are being compiled. Last operation before crash may be relevant.

### Register Analysis at Crash

```
x0  = 0x0           ← NULL (should be hl_type*)
x1  = valid pointer
x30 = 0x7ffff5c350ac ← Return address in JIT code
pc  = 0x7ffff7f4f780 ← hl_type_get_global
```

**Key Questions**:
1. What was X0 before the call?
2. Which JIT operation generated the call?
3. Is the correct wrapper being used?

### Adding More Debug Output

#### In jit_hl2c_arm64
Add at the start of the function (before inline assembly):
```c
fprintf(stderr, "[HL2C] Entry: c=%p, c->fun=%p, nargs=%d\n",
        c, c ? c->fun : NULL, nargs);
fflush(stderr);
```

#### In get_wrapper_arm64
```c
static void *get_wrapper_arm64( hl_type *t ) {
    fprintf(stderr, "[WRAPPER] get_wrapper_arm64 called for type %p\n", t);
    fflush(stderr);
    return (void*)jit_hl2c_arm64;
}
```

#### In OCall Operations
Look for `op_call_native` and add:
```c
fprintf(stderr, "[CALL_NATIVE] Calling %p with %d args\n", func_ptr, nargs);
fflush(stderr);
```

### Trampoline Calling Convention Check

#### Expected Register Usage (AAPCS64)

**Before calling C function**:
- X0 = first argument (often pointer)
- X1 = second argument
- X2 = third argument
- X3-X7 = additional arguments
- X30 (LR) = return address
- SP = stack pointer (16-byte aligned)

**After calling C function**:
- X0 = return value
- X19-X28 = preserved (callee-saved)
- X29 (FP) = preserved
- X30 (LR) = preserved

#### Verify Trampoline Preserves Registers

Set breakpoint in `jit_hl2c_arm64`:
```gdb
break jit_hl2c_arm64
run /tmp/hello.hl
# When it breaks:
info registers  # Before execution
stepi 20        # Step through assembly
info registers  # After execution
# Check X19-X30 unchanged
```

### Common Issues to Check

#### 1. Wrong Function Pointer
- JIT code might be loading wrong address
- Wrapper might be returning wrong trampoline
- Global function table might be corrupted

**Check**:
```gdb
print hl_setup.get_wrapper
print hl_setup.static_call
# Verify these match expected addresses from stderr.txt
```

#### 2. Stack Alignment
- SP must be 16-byte aligned before calls
- Check stack frame setup in trampolines

**Check**:
```gdb
break jit_hl2c_arm64
run /tmp/hello.hl
print/x $sp
# Should end in 0x...0 (aligned to 16)
```

#### 3. Argument Passing
- Arguments might not be loaded correctly
- args array might be invalid

**Check in jit_hl2c_arm64**:
```c
fprintf(stderr, "[HL2C] args[0]=%p, args[1]=%p\n",
        nargs > 0 ? args[0] : NULL,
        nargs > 1 ? args[1] : NULL);
```

### Test with Minimal Program

Create minimal Haxe program:
```haxe
class Test {
    static function main() {
        // Absolutely minimal - no output
    }
}
```

Compile and test:
```bash
haxe -hl test.hl -main Test
LD_LIBRARY_PATH=. ./hl test.hl 2>stderr.txt
```

If still crashes, the issue is in initialization, not user code.

### Expected Next Debugging Steps

1. **Disassemble JIT code at 0x7ffff5c350ac**
   - Find the BL/BLR instruction that calls hl_type_get_global
   - See what instruction loaded X0 before the call
   - Trace back to find where X0 should have been set

2. **Add logging to trampoline entry**
   - Confirm jit_hl2c_arm64 is being called
   - Verify c->fun is valid
   - Check arguments being passed

3. **Verify OCall implementation**
   - Find which OCall variant is being used
   - Check if it's using correct wrapper
   - Verify argument setup code

4. **Test hypothesis: wrong function being called**
   - Maybe JIT code is calling hl_type_get_global directly
   - Should be calling through wrapper
   - Check call site generation code

### Files to Examine

- `src/jit.c:3490-3524` - jit_hl2c_arm64 implementation
- `src/jit.c:3527-3529` - get_wrapper_arm64
- `src/jit.c:4000-4100` - OCall operations (ARM64)
- `src/jit.c:2000-2100` - op_call_native (check x86 for reference)

### Success Criteria

Program should run and print:
```
Hello World
```

Exit code should be 0 (not 139).

No segmentation faults.

### Contact/Resources

- ARM64 JIT implementation in `src/jit.c` (lines 3000-7500)
- AAPCS64 spec: https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst
- Previous debugging notes: ARM64_TRAMPOLINE_DEBUG_2025-11-18.md
- Status: ARM64_JIT_STATUS.md
