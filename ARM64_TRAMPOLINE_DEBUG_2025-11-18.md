# ARM64 JIT Trampoline Debugging Session - November 18, 2025

## Executive Summary

**Status**: ARM64 JIT trampolines successfully implemented and initialized, but runtime crashes with NULL pointer dereference in `hl_type_get_global()`.

**Progress**:
- ✅ JIT compilation: 384 functions compile successfully
- ✅ Trampolines: C↔HL calling convention converters working
- ✅ Setup: Both `static_call` and `get_wrapper` initialized correctly
- ❌ Runtime: Crashes when JIT code calls native C functions

## Trampoline Implementation

### Components Implemented

#### 1. C → HL Trampoline (`callback_c2hl_arm64`)
```c
Location: src/jit.c:3442-3486

Function signature:
  void *callback_c2hl_arm64(void *_f, hl_type *t, void **args, vdynamic *ret)

Implementation:
  - Converts from C calling convention to HL JIT calling convention
  - Uses hl_vargs structure to marshal arguments
  - Handles type conversions (HBOOL, HUI8, HI32, HF32, HF64, HI64, etc.)
  - Calls low-level trampoline with inline assembly

Low-level trampoline (jit_c2hl_arm64_trampoline):
  - 160-byte stack frame (16-byte aligned per AAPCS64)
  - Saves callee-saved registers: X19-X28, X29 (FP), X30 (LR)
  - Loads 8 integer args (X0-X7) from hl_vargs.iargs[0-7]
  - Loads 8 float args (V0-V7) from hl_vargs.fargs[0-7]
  - Calls function pointer via BLR X20
  - Returns result in X0
```

#### 2. HL → C Trampoline (`jit_hl2c_arm64`)
```c
Location: src/jit.c:3490-3524

Function signature:
  void* jit_hl2c_arm64(vclosure* c, void** args, int nargs, bool ret_void)

Implementation:
  - Converts from HL JIT calling convention to C calling convention
  - 96-byte stack frame (16-byte aligned)
  - Saves callee-saved registers: X19, X20, X21, X22, X29, X30
  - Loads up to 3 arguments from args array to X0, X1, X2
  - Calls c->fun via BLR X9
  - Returns result in X0
```

#### 3. Wrapper Function (`get_wrapper_arm64`)
```c
Location: src/jit.c:3527-3529

Returns: Pointer to jit_hl2c_arm64 trampoline
Purpose: Provides HL→C trampoline for native function calls from JIT code
```

### Initialization

**Location**: `src/jit.c:3712-3726` (inside `hl_jit_code_arm64`)

**Setup code**:
```c
if (!call_jit_c2hl) {
    hl_setup.static_call = callback_c2hl_arm64;
    hl_setup.get_wrapper = get_wrapper_arm64;
    hl_setup.static_call_ref = false;
    call_jit_c2hl = (void*)1;  // Mark as initialized
}
```

**Debug output confirms**:
```
[TRAMPOLINE_CHECK] Reached trampoline setup code, call_jit_c2hl=(nil)
[TRAMPOLINE] Setting up ARM64 trampolines
[TRAMPOLINE] hl_setup.static_call=0x5555a8919bb4 (callback_c2hl_arm64)
[TRAMPOLINE] hl_setup.get_wrapper=0x5555a8919b00 (get_wrapper_arm64)
```

## Current Crash Analysis

### Crash Details

**Program**: `./hl /tmp/hello.hl`
**Exit code**: 139 (SIGSEGV - Segmentation Fault)

**GDB Backtrace**:
```
Thread 1 "hl" received signal SIGSEGV, Segmentation fault.
0x00007ffff7f4f780 in hl_type_get_global () from ./libhl.so

#0  0x00007ffff7f4f780 in hl_type_get_global () from ./libhl.so
#1  0x00007ffff5c350ac in ?? ()  ← JIT-compiled code
#2  0x00007ffff5c4eba4 in ?? ()  ← JIT-compiled code
```

### Register State at Crash

```
x0  = 0x0                 ← NULL POINTER (expected: hl_type*)
x1  = 0x55555559db28
x30 = 0x7ffff5c350ac      ← Return address (JIT code)
pc  = 0x7ffff7f4f780      ← hl_type_get_global entry point
```

### Crash Location

**Function**: `hl_type_get_global()` in libhl.so
**Instruction**: `ldr w2, [x0]` ← Attempting to dereference NULL pointer in X0
**Expected**: X0 should contain a valid `hl_type*` pointer
**Actual**: X0 = 0x0 (NULL)

### JIT Code Context

**Calling address**: 0x7ffff5c350ac (JIT-compiled code in heap)
**Operation context**: Based on stdout, likely related to global variable access or dynamic calls
**Last successful ops**: OCallClosure, OGetGlobal operations completed

## Root Cause Hypothesis

### Most Likely: Calling Convention Mismatch

The JIT-compiled code at `0x7ffff5c350ac` is calling `hl_type_get_global()` but:

1. **Incorrect argument setup**: The JIT code may not be correctly setting X0 before the call
2. **Trampoline issue**: The `jit_hl2c_arm64` trampoline may be corrupting X0
3. **Register preservation**: A callee-saved register may be getting clobbered

### Alternative Theories

1. **Wrong function pointer**: JIT code might be calling the wrong function entirely
2. **Stack corruption**: Stack frame setup might be incorrect, causing register corruption
3. **Missing initialization**: Some global state required by `hl_type_get_global` isn't set up

## Debug Evidence

### JIT Compilation Success

```
[JIT] Function 0 through 384 assigned addresses
[PROLOGUE] MOV X29, SP instruction generated correctly
[OGetGlobal] global 61 → type.kind=11 (last logged access)
```

All 384 functions compiled without errors. The crash happens **during execution**, not compilation.

### Trampoline Initialization Success

```
[ARM64_CODE] hl_jit_code_arm64 called!
[TRAMPOLINE_CHECK] Reached trampoline setup code, call_jit_c2hl=(nil)
[TRAMPOLINE] Setting up ARM64 trampolines
[TRAMPOLINE] hl_setup.static_call=0x5555a8919bb4 (callback_c2hl_arm64)
[TRAMPOLINE] hl_setup.get_wrapper=0x5555a8919b00 (get_wrapper_arm64)
```

Trampolines are correctly initialized before JIT code executes.

## Next Steps for Resolution

### Priority 1: Examine JIT Code at Crash Site
```bash
# Disassemble the failing JIT code
gdb ./hl
break hl_type_get_global
run /tmp/hello.hl
# When it breaks, examine:
x/20i $x30-32  # Disassemble caller code
info registers  # Check all register values
```

**Goal**: Understand what the JIT code is doing before calling `hl_type_get_global`

### Priority 2: Verify jit_hl2c_arm64 Trampoline

**Check**:
- Is X0 being preserved/restored correctly?
- Is the function pointer (c->fun) being loaded correctly?
- Are arguments being passed in the right registers?

**Current implementation loads only 3 args**:
```asm
ldr x0, [args, #0]
ldr x1, [args, #8]   (if nargs > 1)
ldr x2, [args, #16]  (if nargs > 2)
```

**Potential issue**: Not loading all 8 possible arguments?

### Priority 3: Check OCall/OCallMethod Implementation

The crash might be in how native function calls are generated:
- Verify `op_call_native` ARM64 implementation
- Check how native function wrappers are being called
- Ensure correct trampoline is being used

### Priority 4: Add More Debug Output

Add logging to:
- `jit_hl2c_arm64` - log entry, c->fun pointer, nargs
- `get_wrapper_arm64` - log when it's called and what it returns
- Native call sites in JIT code - log before calling through wrapper

## Technical Notes

### AAPCS64 Calling Convention
- Integer/pointer args: X0-X7
- Float/double args: V0-V7 (D0-D7)
- Return value: X0 (or V0 for float)
- Callee-saved: X19-X28, FP (X29), LR (X30)
- Stack: 16-byte aligned

### hl_vargs Structure
```c
typedef struct {
    int64_t iargs[8];   // 64 bytes: X0-X7
    double  fargs[8];   // 64 bytes: V0-V7
} hl_vargs;
```

Total size: 128 bytes (64 + 64)

### vclosure Structure
```c
typedef struct {
    hl_type *t;         // offset 0 (8 bytes)
    void *fun;          // offset 8 (8 bytes)
    int hasValue;       // offset 16 (4 bytes)
    int stackCount;     // offset 20 (4 bytes)
    void *value;        // offset 24 (8 bytes)
} vclosure;
```

Total size: 32 bytes

## Files Modified

- `src/jit.c:3383-3729` - ARM64 trampoline implementation
- `src/jit.c:3763-3778` - Architecture detection and routing
- Debug output added throughout for trampoline tracking

## Commits

1. `155491d` - Implement ARM64 dynamic call trampolines (C↔HL calling convention)
2. `dfc5eae` - Wire up ARM64 trampolines correctly - get_wrapper for native calls
3. `7743781` - Add comprehensive debug output for ARM64 JIT trampoline debugging

## References

- ARM Architecture Procedure Call Standard (AAPCS64)
- HashLink x86-64 JIT implementation (for comparison)
- Previous session notes on vclosure structure fixes
