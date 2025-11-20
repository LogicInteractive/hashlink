# ARM64 JIT Bug Investigation & Fixes

## Session Date: 2025-11-20

## Problem Statement
HashLink ARM64 JIT crashes on even the simplest programs (including `empty.hl`) with segmentation fault in native C functions. The crash occurs because function pointers are being truncated from 64-bit to 32-bit values.

### Crash Symptoms
```
Program received signal SIGSEGV, Segmentation fault.
0x00007ffff7f45f10 in hl_hbset_impl () from ./libhl.so

x0 = 0xf5c433ed          (TRUNCATED - missing upper 32 bits!)
Expected: 0x7ffff5c433ed (full 64-bit address)
```

The native C function `hl_hbset_impl()` receives a corrupted pointer in x0 (first argument register), causing it to crash when dereferencing.

---

## Root Causes Identified

### Bug #1: ARM_BUF_POS() Returns 32-bit Value ✅ FIXED

**Location**: `src/jit.c:340`

**Problem**:
The `ARM_BUF_POS()` macro computed buffer positions (which are used as function addresses) but cast the result to `int` (32-bit) instead of `int_val` (64-bit on ARM64).

```c
// BEFORE (BUGGY):
#define ARM_BUF_POS() ((int)((unsigned char*)ctx->buf.w - ctx->startBuf))

// AFTER (FIXED):
#define ARM_BUF_POS() ((int_val)((unsigned char*)ctx->buf.w - ctx->startBuf))
```

**Impact**:
- Function addresses stored in `m->functions_ptrs[]` were truncated
- Any code using ARM_BUF_POS() only got the lower 32 bits
- Example: `0x00007ffff5c433ed` → `0x00000000f5c433ed`

**Related Fixes**:
1. **Line 7919**: Changed `hl_jit_buf_pos()` return type from `int` to `int_val`
2. **src/hlmodule.h:163**: Updated function declaration to match
3. **src/module.c:675,685**: Changed callers to use `int_val` variables instead of `int`
4. **Line 332**: Fixed printf format from `%d` to `%ld` for ARM_BUF_POS()

---

### Bug #2: HI32 Vreg Stack Allocation Too Small ✅ FIXED

**Location**: `src/jit.c:3918-3934`

**Problem**:
The HashLink type system sometimes uses `HI32` (32-bit integer type) to represent function pointers and other pointer-sized values. On ARM64, this caused multiple issues:

1. **Stack Allocation**: HI32 vregs got `size=4` bytes, but ARM64 needs 8 bytes for pointers
2. **Data Corruption**: When 64-bit values were stored in 4-byte stack slots, they overflowed into adjacent vregs
3. **Truncation**: When 32-bit-sized data was loaded as 64-bit, only the lower 32 bits were valid

**Fix Applied**:
```c
for(i=0;i<f->nregs;i++) {
    vreg *r = R(i);
    r->t = f->regs[i];
    r->size = hl_type_size(r->t);
#ifdef HL_JIT_ARM64
    // CRITICAL FIX: On ARM64, HI32 must be 64-bit to preserve pointers mistyped as HI32
    // The type system sometimes uses HI32 for function pointers, causing truncation
    // when copy()/prepare_call_args() only copies r->size bytes
    if (r->t->kind == HI32) {
        if (i < 5) {  // Only print first 5 to avoid spam
            fprintf(stderr, "[HI32_FIX] F%d r%d: size 4->8\n", f->findex, i);
        }
        r->size = 8;  // Force 64-bit size for HI32 on ARM64
    }
#endif
    r->current = NULL;
    r->stack.holds = NULL;
    r->stack.id = i;
    r->stack.kind = RSTACK;
}
```

**Why This Matters**:
- `r->size` is used for stack space allocation (lines 3956-3958)
- Each vreg gets `size` bytes on the stack
- HI32 vregs now get 8 bytes instead of 4, preventing overflow
- The x86-style `copy()` function (if ever used on ARM64) now copies 64 bits for HI32

**Verification**:
Debug output confirms the fix is active:
```
[HI32_FIX] F24 r1: size 4->8
[HI32_FIX] F20 r4: size 4->8
[HI32_FIX] F15 r0: size 4->8
```

---

## Existing Safeguards (Already in Place)

The codebase already had some protections against this issue:

### LOAD_VREG Macro (Lines 4370-4400)
```c
bool is_arg_reg = (tmp_reg >= X0 && tmp_reg <= X7);
if (is_arg_reg) {
    _size = 3;  /* Force 64-bit load for all function arguments */
}
```
- Argument registers (X0-X7) always get 64-bit loads
- This works correctly but wasn't sufficient on its own

### STORE_VREG Macro (Lines 4402-4426)
```c
switch ((vr)->t->kind) {
    case HI32: _size = 3; break;  /* 64-bit to preserve pointers! */
    default: _size = 3; break;     /* 64-bit for HI64, pointers */
}
```
- HI32 values are stored as 64-bit
- Helps prevent truncation during stores

---

## Known Issues (Unreachable Debug Code)

The STORE_VREG macro has unreachable debug code (lines 4411-4414):
```c
switch ((vr)->t->kind) {
    case HUI8: case HBOOL: _size = 0; break;
if ((vr)->t->kind == HI32) {  // ← This is BETWEEN case labels!
    printf("[STORE_VREG] ...\n");  // Never executes
}
    case HUI16: _size = 1; break;
```

The `if` statement is between `case` labels, making it unreachable. The switch jumps directly to the matching case, skipping this code entirely.

**Impact**: No functional impact (just dead code), but the debug printf never runs.

---

## Current Status

### ✅ Fixed Issues
1. ARM_BUF_POS() now returns 64-bit values
2. HI32 vregs get proper 8-byte stack allocation
3. All related type declarations and format strings updated

### ❌ Remaining Issue
**Program still crashes** with same symptoms:
- SIGSEGV in `hl_hbset_impl()`
- x0 = `0xf5c433ed` (still truncated!)

### Why Fixes Weren't Sufficient

Despite fixing the identified bugs, the pointer is STILL being truncated. This indicates there's at least one more 32-bit truncation happening somewhere in:

1. **Instruction encoding**: `arm_ldr_imm()/arm_str_imm()` might have bugs
2. **Hidden casts**: Another `(int)` cast we haven't found
3. **Calling convention**: Issues with how native C functions receive arguments
4. **Runtime data**: Values might be pre-truncated in loaded `.hl` files

---

## Testing Methodology

### Test Program
```haxe
// empty.hl - Simplest possible HashLink program
class Empty {
    static function main() {}
}
```

Compiled to bytecode:
```bash
haxe --hl empty.hl -main Empty
```

### Test Execution
```bash
export LD_LIBRARY_PATH=.
./hl /tmp/empty.hl
```

### Debug with GDB
```bash
gdb -batch -ex "run /tmp/empty.hl" -ex "bt" -ex "info registers" ./hl
```

---

## Architecture Context

### ARM64 LP64 Data Model
- `int`: 32-bit
- `long`: 64-bit
- `void*`: 64-bit
- `intptr_t` (typedef'd as `int_val`): 64-bit

### HashLink Type System
```c
// From src/std/types.c
static int T_SIZES[] = {
    0,         // VOID
    1,         // I8
    2,         // I16
    4,         // I32      ← Problem: Used for pointers sometimes!
    8,         // I64
    HL_WSIZE,  // FUN      ← Should be used for function pointers
    HL_WSIZE,  // OBJ
    ...
};
```

The type system defines `HI32` as 4 bytes, but the Haxe compiler sometimes uses it for function pointers, creating a type mismatch on 64-bit platforms.

---

## Code Paths Analysis

### ARM64 OCallN Handler (Line 5670)
The ARM64 JIT has its own implementation for native calls that correctly uses 64-bit operations:

```c
case OCallN:
    // Move arguments to X0-X7
    for (int i = 0; i < nargs && i < 8; i++) {
        vreg *arg = R(o->extra[i]);
        if (arg) {
            LOAD_VREG((Arm64Reg)(X0 + i), arg);  // Uses 64-bit loads
        }
    }

    if (isNative) {
        void *fptr = ctx->m->functions_ptrs[o->p2];
        arm_load_imm64(ctx, X9, (uint64_t)fptr);  // 64-bit address
        arm_call_native(ctx, X9);
    }
```

This code looks correct and uses proper 64-bit operations throughout.

---

## Files Modified

1. **src/jit.c**
   - Line 340: ARM_BUF_POS() macro
   - Line 332: Printf format string
   - Lines 3922-3932: HI32 vreg size fix
   - Line 7919: hl_jit_buf_pos() return type

2. **src/hlmodule.h**
   - Line 163: hl_jit_buf_pos() declaration

3. **src/module.c**
   - Lines 675, 685: Variable types for hl_jit_buf_pos() callers

---

## Recommendations for Future Investigation

1. **Instrument ARM64 load/store functions**
   - Add logging to `arm_ldr_imm()` and `arm_str_imm()`
   - Verify size=3 generates correct 64-bit LDR/STR instructions

2. **Check instruction encoding**
   - Dump generated ARM64 machine code
   - Disassemble to verify instructions are correct

3. **Trace value flow**
   - Add logging when values are written to `functions_ptrs[]`
   - Track the exact path from buffer position to x0 register

4. **Examine calling convention**
   - Verify ARM64 ABI compliance in `arm_call_native()`
   - Check if arguments are being truncated during function calls

5. **Consider bytecode issues**
   - The Haxe→HashLink compiler might generate incorrect types
   - May need fixes in the compiler, not just the JIT

---

## References

- **ARM64 Architecture**: 64-bit pointers, LP64 data model
- **HashLink Docs**: https://hashlink.haxe.org/
- **Haxe Language**: https://haxe.org/
- **intptr_t**: POSIX type guaranteed to hold pointer values

---

## Commit History

- Initial investigation: Found pointer truncation bug
- Fixed ARM_BUF_POS() to return 64-bit values
- Fixed HI32 vreg stack allocation
- Added comprehensive documentation
- Status: Issue partially resolved, investigation continues
