# ARM64 JIT Bug #4 - Unknown Corruption Source

**Date:** 2025-11-19
**Status:** 🔴 **OPEN - UNDER INVESTIGATION**
**Branch:** `claude/investigate-arm-port-01JdvdUoc1A2ks5TeCHaPDRk`

---

## Summary

After fixing THREE critical bugs in the ARM64 JIT, test programs still crash with the EXACT SAME memory corruption pattern. This indicates a FOURTH unknown bug exists that generates corrupted STR instructions with huge offsets.

---

## Bugs Fixed (That Didn't Solve It)

| Bug # | Description | Lines Fixed | Status |
|-------|-------------|-------------|--------|
| #1 | BUF_POS Desynchronization | 11 locations | ✅ FIXED |
| #2 | Alignment Padding Infinite Loop | 2 locations | ✅ FIXED |
| #3 | jit_buf() Reallocation Buffer Pointer Bug | 3 locations | ✅ FIXED |

All three bugs are objectively real and have been fixed. The code now:
- ✅ Uses `ARM_BUF_POS()` correctly everywhere
- ✅ Advances `buf.w` by 1 byte (not 4!) during alignment padding
- ✅ Updates `buf.w` (not `buf.b`) during buffer reallocation

---

## The Persistent Crash

**Despite all fixes**, the crash persists with IDENTICAL symptoms:

### Crash Details
```
Address:     0x7ffff5c378ac (in JIT code)
Instruction: str x10, [x29, #16128]
Offset:      #16128 = 0x3F00 (WAY beyond any valid stack frame!)
Function:    Function 16 ($String), framesize=48
Exit Code:   139 (SIGSEGV)
```

### Evidence of Widespread Corruption

Disassembly shows MULTIPLE corrupted instructions:
```asm
0x7ffff5c37884:  str x0, [x29, #16256]   ← CORRUPTED!
0x7ffff5c378ac:  str x10, [x29, #16128]  ← CRASH HERE
0x7ffff5c378bc:  str x0, [x29, #16096]   ← CORRUPTED!
0x7ffff5c378e8:  str x10, [x29, #15616]  ← CORRUPTED!

# Plus repeated instructions (corruption signature):
0x7ffff5c378d8:  mov x29, sp
0x7ffff5c378dc:  mov x29, sp
0x7ffff5c378e0:  mov x29, sp  ← THREE IDENTICAL!
```

---

## What We Know

### ✅ Confirmed Facts

1. **Not from `arm_str_imm()`**
   - Added debug logging to catch all STR generation with offset > 500
   - NO debug output = `arm_str_imm()` never called with large offsets
   - Corruption must come from a different mechanism

2. **Not from jump patching**
   - Added debug logging to `arm_patch_branch()`
   - All patches show reasonable offsets (4, 8, 12, 112, 220 bytes)
   - None patch near the crash location (offset 0x8ac = 2220)

3. **Not from buffer reallocation**
   - Added debug logging to `jit_buf()`
   - Shows clean operation: `buf.w` advances correctly
   - Buffer grows from 0 → 22732 bytes, no corruption during growth

4. **Corruption is deterministic**
   - ALWAYS crashes at same address `0x7ffff5c378ac`
   - ALWAYS same instruction `str x10, [x29, #16128]`
   - ALWAYS same function (Function 16, offset 0x854-0xb6c)

### ❌ What It's NOT

- ✗ BUF_POS/ARM_BUF_POS desynchronization (fixed)
- ✗ Alignment padding infinite loop (fixed)
- ✗ Buffer reallocation pointer bug (fixed)
- ✗ Normal code generation via `arm_str_imm()` (instrumented, never called)
- ✗ Jump patching gone wrong (instrumented, all patches clean)
- ✗ Random memory corruption (too consistent and deterministic)

---

## Theories to Investigate

### Theory #1: Direct B32() Calls with Wrong Values
**Hypothesis:** Some code might be calling `B32()` directly with pre-computed instruction words that have incorrect offsets baked in.

**Evidence:**
- Corruption appears as valid ARM64 instructions (correct encoding)
- But with impossibly large offsets
- Suggests instructions were GENERATED with wrong values, not corrupted afterward

**Next Steps:**
- Search for all `B32()` calls in ARM64 code
- Check if any compute instruction words manually (not via `arm_str_imm()`)
- Look for bit manipulation that might create wrong imm12 fields

### Theory #2: Virtual Register Stack Position Bug
**Hypothesis:** Virtual register (`vreg`) stack positions might be calculated incorrectly, causing `arm_str_imm()` to be called with positions that SEEM small but translate to huge offsets.

**Evidence:**
- STR instructions load/store from virtual register stack slots
- If `vreg->stackPos` is wrong, offset calculation could overflow
- ARM64 STR uses scaled offset: `imm12 << size` = actual byte offset
- imm12=2016, size=3 → offset = 2016 << 3 = 16128 ✓ matches crash!

**Next Steps:**
- Instrument `vreg` stack position calculation
- Check if any `stackPos` values exceed reasonable frame size
- Verify frame size calculation vs actual register allocation

### Theory #3: Buffer Overflow During Generation
**Hypothesis:** Some operation writes past the intended buffer position, overwriting later code.

**Evidence:**
- Repeated `mov x29, sp` instructions suggest buffer overwrite
- Pattern consistent with loop writing same instruction multiple times
- Could be array bounds issue or incorrect loop termination

**Next Steps:**
- Add bounds checking to B32() macro
- Instrument buffer writes with position validation
- Check for off-by-one errors in code generation loops

### Theory #4: Shared/Stale Context Between Functions
**Hypothesis:** The `jit_ctx` might be reused between functions without proper reset, causing position tracking to be wrong for subsequent functions.

**Evidence:**
- Crash always in same function (#16)
- Previous functions compile successfully
- Suggests accumulated error over multiple compilations

**Next Steps:**
- Check if `ctx` is reset between functions
- Verify `buf.w` synchronization at function boundaries
- Look for shared state that shouldn't be shared

---

## Debug Output Added

Currently instrumented:
1. ✅ `arm_str_imm()` - logs all STR with offset > 500
2. ✅ `arm_patch_branch()` - logs all jump patches
3. ✅ `jit_buf()` - logs buffer reallocations

**Result:** All show clean operation, no smoking gun found.

---

## Testing Environment

**Hardware:** Raspberry Pi 5 Model B (ARM Cortex-A76, 64-bit)
**OS:** Linux 6.6.51+rpt-rpi-2712
**Compiler:** GCC with `-O0 -g -DHL_64 -DHL_JIT_ARM64`

**Test Results:**
```
✅ Build:            SUCCESS
✅ test_jit_execute: SUCCESS (C-level JIT test)
✅ hl --version:     SUCCESS
❌ test_minimal.hl:  SEGFAULT at 0x7ffff5c378ac
```

---

## Code Generation Statistics

From debug output before crash:
- **Functions compiled:** 255+ (all standard library functions)
- **JIT buffer calls:** 1000+ (frequent, all clean)
- **Buffer size:** 22732 bytes allocated
- **Crash offset:** 0x8ac = 2220 bytes (within buffer, not overflow)

---

## Recommended Next Steps

### Immediate (High Priority)
1. **Instrument virtual register allocation**
   - Add logging to `stackPos` calculation
   - Check for overflow: `if (stackPos > framesize) error()`
   - Verify frame size matches total register usage

2. **Add safety checks to B32()**
   - Validate `buf.w` is within buffer bounds
   - Assert `ARM_BUF_POS() < bufSize`
   - Catch buffer overruns early

3. **Dump generated code before execution**
   - Disassemble all generated functions
   - Compare with expected patterns
   - Identify where corruption first appears

### Follow-up (Medium Priority)
4. **Binary search for problematic function**
   - Disable JIT for functions one at a time
   - Find which function's compilation introduces corruption
   - Narrow down to specific operation/instruction

5. **Compare with x86 code generation**
   - Run same bytecode on x86 JIT
   - Compare generated code patterns
   - Look for ARM64-specific quirks

### Long-term (Research)
6. **Valgrind/AddressSanitizer**
   - Build with ASAN to catch buffer overruns
   - Use memory debugging tools
   - May reveal subtle issues

---

## Related Files

- `ARM64_BUGS_FIXED_SUMMARY.md` - Bugs #1, #2, #3 documentation
- `ARM64_ROOT_CAUSE_FOUND.md` - Original Bug #1 analysis
- `ARM64_FIX_APPLIED.md` - Implementation details
- `src/jit.c:546-585` - Bug #3 fix location
- `src/jit.c:3382-3408` - Bug #2 fix location
- `src/jit.c:3757-3930` - Bug #1 fix locations

---

## Conclusion

Three critical bugs have been identified and fixed, but a fourth bug remains. The persistence of the EXACT SAME corruption pattern despite comprehensive fixes suggests:

1. **The remaining bug is subtle and systematic**
2. **It's likely in virtual register handling or offset calculation**
3. **It's NOT in the obvious buffer management code**
4. **It manifests during code generation, not buffer management**

The investigation continues. The three fixes already made are valuable and should be committed, as they fix real bugs even if they don't solve this particular crash.

---

**Last Updated:** 2025-11-19 21:30 UTC
**Investigator:** Claude (Anthropic)
**Hardware Tested:** Raspberry Pi 5 (ARM64)
