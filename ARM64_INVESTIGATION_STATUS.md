# ARM64 JIT Investigation - Current Status

**Date:** 2025-11-20
**Branch:** `claude/investigate-arm-port-01JdvdUoc1A2ks5TeCHaPDRk`
**Latest Commit:** d183a7b (EXPERIMENTAL: HI32 globals lookup)

---

## Current State

### What's Been Fixed ✅

1. **Makefile** - Added `-DHL_JIT_ARM64` flag for ARM64 builds
2. **T_SIZES[HI32]** - Changed from 4 to 8 bytes on ARM64
3. **LOAD_VREG** - Uses 64-bit loads (_size=3) for HI32 type
4. **STORE_VREG** - Uses 64-bit stores (_size=3) for HI32 type
5. **OInt opcode** - Zero-extends when loading integer constants
6. **OGetGlobal** - Uses 64-bit loads from globals
7. **OSetGlobal** - Uses 64-bit stores to globals

### The Persistent Problem ❌

**Crash:**
```
Thread 1 "hl" received signal SIGSEGV, Segmentation fault.
0x00007ffff7f45f10 in hl_hbset_impl () from ./libhl.so

x0 = 0xf5c433ed  ← TRUNCATED!
Expected: 0x7ffff5c433ed (JIT buffer address)
```

**Crash Location:** Native C function `hl_hbset_impl()` receives truncated pointer in x0

**Timing:** Crash occurs AFTER constant initialization, when program starts executing

---

## Analysis

### What We Know

1. **JIT buffer location:** 0x7ffff5c34000 (106KB)
2. **Truncated value:** 0xf5c433ed
3. **Expected value:** 0x7ffff5c433ed (JIT buffer + 62445 bytes offset)
4. **Value type:** JIT function address, NOT heap pointer

### The Mystery

The value `0xf5c433ed` is a **valid JIT function offset** that somehow lost its upper 32 bits.

Possible truncation points:
1. ❓ **Constant initialization** - bytecode stores 32-bit values
2. ❓ **Function pointer storage** - m->functions_ptrs[] handling
3. ❓ **Argument passing** - LOAD_VREG for function arguments
4. ❓ **Type casting** - somewhere an int cast instead of int_val

### Recent Experimental Fix (d183a7b)

**Change:** On HL_64, HI32 constants use globals lookup instead of ints[] array

**Theory:** If HI32 fields contain pointers (mistyped by Haxe compiler), they should load from `globals_data` (which has proper 64-bit pointers) instead of `ints[]` array (which has 32-bit values).

**Status:** UNTESTED - requires ARM64 hardware

**Risk:** If `idx` is truly an ints[] index and we use it as globals index, we'll load garbage.

---

## Next Steps

### Priority 1: Test Experimental Fix

**On ARM64 hardware:**
```bash
cd /home/user/hashlink
git pull
make clean && make
./hl /tmp/empty.hl
```

**Expected outcomes:**
- ✅ BEST: Program runs without crash
- ⚠️ GOOD: Different crash with more information
- ❌ SAME: Still crashes with x0=0xf5c433ed (fix didn't help)

### Priority 2: Add More Debugging

If experimental fix doesn't work, add detailed logging:

```c
// In hl_module_init_constant, default case:
void *ptr = *(void**)(m->globals_data + m->globals_indexes[idx]);
fprintf(stderr, "[CONST_INIT] Loading ptr from globals_data+%d: %p\n",
        m->globals_indexes[idx], ptr);
*(void**)addr = ptr;
```

This will show if globals_data contains truncated pointers.

### Priority 3: Check Function Pointer Flow

Trace where `0xf5c433ed` originates:

1. **JIT compilation:**
   ```c
   // module.c:703
   m->functions_ptrs[f->findex] = (void*)(int_val)fpos;  // Store offset
   ```

2. **Address conversion:**
   ```c
   // module.c:712
   m->functions_ptrs[f->findex] = jit_code + offset;  // Convert to address
   ```

3. **Usage in OCallN:**
   ```c
   // jit.c:5678
   void *fptr = ctx->m->functions_ptrs[o->p2];  // Load for native call
   ```

Check if any of these use 32-bit intermediate types.

---

## Architectural Issues

### The HI32 Dilemma

**Problem:** Haxe compiler uses HI32 (32-bit integer type) for:
- Actual 32-bit integers ✓
- Function pointers (should be 64-bit!) ✗
- Object references (should be 64-bit!) ✗

**On 32-bit platforms:** Works fine (int = pointer = 32 bits)
**On 64-bit platforms:** Breaks (int = 32 bits, pointer = 64 bits)

**Why this happens:** Haxe language spec doesn't distinguish pointer-sized integers from fixed-size integers.

### The Bytecode Compatibility Problem

**Bytecode format** (unchangeable without breaking compatibility):
- HI32 values stored in `ints[]` array as 32-bit
- Field indexes multiply by HL_WSIZE
- Structure layouts assume compile-time type sizes

**Runtime changes** (what we can control):
- T_SIZES[HI32] at runtime
- Field offset calculations
- Load/store instruction sizes

**Tension:** Bytecode compiled with HI32=4 bytes, but runtime uses HI32=8 bytes
→ Field offsets mismatch? Object layouts wrong?

Actually, **field offsets ARE calculated at runtime** using `hl_type_size()`, so changing T_SIZES should work!

---

## Theory: Where is 0xf5c433ed Coming From?

### Hypothesis 1: Bytecode Constants

**Problem:** Bytecode `ints[]` array contains `0xf5c433ed`
**Why:** Compiled on 32-bit or with address truncated
**Fix:** Experimental commit d183a7b (use globals lookup)

### Hypothesis 2: Function Pointer Table

**Problem:** `m->functions_ptrs[]` has truncated value
**Check:**
```c
// After line 712 in module.c:
fprintf(stderr, "[FPTR] F%d: %p\n", f->findex, m->functions_ptrs[f->findex]);
```

### Hypothesis 3: Native Call Arguments

**Problem:** LOAD_VREG loads 32 bits for non-argument registers
**Status:** Should be fixed (LOAD_VREG uses _size=3 for HI32)
**Double-check:** Add debug to LOAD_VREG macro

---

## Files Modified

| File | Lines | Status | Purpose |
|------|-------|--------|---------|
| Makefile | 206-207 | ✅ | Add -DHL_JIT_ARM64 flag |
| src/std/types.c | 48-52 | ✅ | T_SIZES[HI32]=8 on ARM64 |
| src/jit.c | 4390,4413 | ✅ | LOAD/STORE_VREG 64-bit for HI32 |
| src/jit.c | 4514 | ✅ | OInt zero-extends constants |
| src/module.c | 615-621 | ⚠️ EXPERIMENTAL | HI32 uses globals lookup |

---

## Confidence Assessment

**High confidence fixes:**
- T_SIZES change (proven to update field offsets correctly)
- LOAD_VREG/STORE_VREG 64-bit for HI32 (prevents vreg truncation)
- OInt zero-extension (correct for integer constants)

**Low confidence fixes:**
- HI32 globals lookup (might break if idx is ints[] index)

**Unknown:**
- Where exactly the truncation occurs
- Whether bytecode contains pre-truncated values
- Whether there are OTHER truncation points we haven't found

---

## Contact / Testing

**This code MUST be tested on ARM64 hardware (Raspberry Pi 5).**

Cross-compilation from x86_64 won't add the proper -DHL_JIT_ARM64 flag without manual intervention.

**To build on ARM64:**
```bash
uname -m  # Should show: aarch64
make clean && make
./hl --version
./hl /tmp/empty.hl
```

---

**Last Updated:** 2025-11-20 23:30 UTC
**Status:** Awaiting ARM64 hardware test results
