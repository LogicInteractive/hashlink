# ARM64 JIT - Complete Analysis Summary

**Date:** 2025-11-19
**Branch:** `claude/investigate-arm-port-01JdvdUoc1A2ks5TeCHaPDRk`

---

## Bugs Found and Fixed

### ✅ Bug #1: BUF_POS Desynchronization (Commit f6b4bf2)
**Fixed:** 11 locations
**Problem:** ARM64 used `BUF_POS()` (reads `buf.b`) but wrote via `B32()` (increments `buf.w`)
**Impact:** `opsPos[]` array filled with wrong positions → jump patching corrupted instructions

### ✅ Bug #2: Alignment Padding Infinite Loop (Commit 6f4f69e)
**Fixed:** 2 locations in `jit_build_arm64()`
**Problem:** Alignment code checked `ARM_BUF_POS()` but wrote via `buf.b++`
**Impact:** Infinite loop at startup when building error handlers

### ✅ Bug #3: jit_buf() Reallocation Bug (Commit 9ca3c4e)
**Fixed:** 3 locations in `jit_buf()`
**Problem:** Buffer reallocation used `BUF_POS()` to get position, then set `buf.b` but didn't update `buf.w`
**Impact:** All B32() writes after reallocation used wrong pointer → widespread corruption

### ✅ Bug #4: LDUR Opcode Encoding (Commit 3d32def)
**Fixed:** 1 location in `arm_ldur_imm()` (line 6763)
**Problem:** Used `(0x39 << 24)` which generates STR instead of `(0x38 << 24) | (1 << 22)` for LDUR
**Impact:** Load instructions actually performed stores → SIGSEGV

---

## Outstanding Issues

### ⚠️ Potential Bug #5: ctx->functionPos (Line 3889)
**Status:** UNCONFIRMED - Found but not tested
**Problem:** `ctx->functionPos = BUF_POS();` in shared code (not ARM64-specific)
**Impact:** Likely minimal (functionPos only used in x86 code at line 1866)
**Should Fix:** Yes, for correctness even if not causing current crash

### 🔴 Bug #X: Unknown Corruption Source
**Status:** UNDER INVESTIGATION
**Symptoms:** Program still crashes with corrupted STR instructions showing huge offsets (16KB+)

**Evidence:**
- Crash always at same location (Function 16, offset 0x8ac)
- Instruction: `str x10, [x29, #16128]` (imm12=2016)
- Debug logging shows `arm_str_imm()` NEVER called with large offsets
- Jump patching logs show all patches are clean
- Buffer reallocation logs show correct operation

**Theories:**
1. **Direct B32() calls** - Some code might compute STR instructions manually
2. **VReg stack position bug** - stackPos might have wrong values (imm12=2016 exactly matches 16128>>3)
3. **Buffer overflow** - Something writes past intended position
4. **Stale context** - ctx might not be reset between functions

---

## Code Quality Issues Found

### Line 3889: Mixed Buffer Pointer Usage
```c
ctx->functionPos = BUF_POS();  // ❌ Should use ARM_BUF_POS() for ARM64
```

**Recommendation:** Add conditional like other fixes:
```c
#ifdef HL_JIT_ARM64
    ctx->functionPos = ARM_BUF_POS();
#else
    ctx->functionPos = BUF_POS();
#endif
```

---

## Test Results (Raspberry Pi 5)

| Test | Result |
|------|--------|
| Build | ✅ SUCCESS |
| test_jit_execute (C-level) | ✅ SUCCESS |
| hl --version | ✅ SUCCESS |
| test_minimal.hl | ❌ SEGFAULT |

**Crash Details:**
- Location: 0x7ffff5c378ac
- Instruction: str x10, [x29, #16128]
- Function: #16 ($String), framesize=48
- Exit code: 139 (SIGSEGV)

---

## Debug Infrastructure Added

Current instrumentation:
1. ✅ `arm_str_imm()` - logs all STR with full details
2. ✅ `arm_patch_branch()` - logs all jump patches
3. ✅ `jit_buf()` - logs buffer reallocations
4. ✅ `hl_jit_code_arm64()` - logs finalization steps

**Result:** All show clean operation, no smoking gun found yet.

---

## Recommended Next Steps

### Immediate Actions

1. **Fix line 3889** (ctx->functionPos)
   - Add ARM64 conditional
   - Verify no regression

2. **Instrument vreg stack positions**
   ```c
   fprintf(stderr, "[VREG] r%d stackPos=%d size=%d\n",
           i, r->stackPos, r->size);
   ```
   - Look for positive stackPos values
   - Look for values > 255 that don't get 2-step treatment

3. **Add bounds assertion to B32()**
   ```c
   #define B32(val) do { \
       if ((unsigned char*)ctx->buf.w >= ctx->startBuf + ctx->bufSize) { \
           fprintf(stderr, "BUFFER OVERFLOW at pos %d\n", ARM_BUF_POS()); \
           ASSERT(99); \
       } \
       *ctx->buf.w++ = (unsigned int)(val); \
   } while(0)
   ```

### Investigation Priorities

1. **Theory #2 (VReg stackPos)** - Most promising
   - imm12=2016 exactly matches 16128>>3
   - Suggests offset calculation bug, not random corruption

2. **Theory #1 (Direct B32 calls)** - Check for manual instruction computation
   - Search for bit manipulation that creates STR instructions
   - Look for hard-coded offsets

3. **Theory #4 (Stale context)** - Check ctx reset between functions
   - Verify buf.w synchronization
   - Look for accumulated errors

---

## Technical Notes

### Buffer Union Behavior
```c
union {
    unsigned char *b;     // x86 uses this
    unsigned int *w;      // ARM64 uses this
} buf;
```
**Key insight:** These are SEPARATE pointers! Incrementing one doesn't affect the other.

### Offset Encoding in ARM64 STR
```
str x10, [x29, #16128]
```
Decodes to:
- Opcode: 0xF9 (bits 31-24)
- imm12: 2016 (bits 21-10)
- Rn: 29/X29 (bits 9-5)
- Rt: 10/X10 (bits 4-0)
- Actual offset: 2016 << 3 = 16128 bytes

**This is a VALID instruction** - just with an offset that doesn't make sense for a 48-byte stack frame!

### Why Corruption Appears Deterministic
- Always same function (#16)
- Always same offset (0x8ac / 2220 bytes into JIT code)
- Always same instruction pattern

**Conclusion:** NOT random memory corruption. This is a systematic bug in code generation or position calculation.

---

## Confidence Assessment

### Bugs #1-4: 100% Confirmed
All four bugs are real, have been fixed, and testing shows improvement:
- No more infinite loops
- No more widespread buffer corruption
- No more LDUR→STR opcode errors
- Compilation succeeds for 247+ functions

### Bug #5 (functionPos): 90% Confident Should Fix
- Clear use of wrong macro
- Low risk to fix
- May not be causing current crash but should be corrected anyway

### Unknown Bug Causing Crash: Under Investigation
- Symptoms are clear and deterministic
- Source not yet identified
- Most likely: vreg stackPos calculation error or direct B32() call

---

## Files Modified

| File | Bugs Fixed | Status |
|------|------------|--------|
| src/jit.c | #1, #2, #3, #4 | ✅ Committed |
| src/module.c | Logging | ✅ Committed |
| src/hlmodule.h | Helper function | ✅ Committed |

---

## Documentation Created

1. ARM64_ROOT_CAUSE_FOUND.md - Bug #1 analysis
2. ARM64_FIX_APPLIED.md - Bug #1 fix documentation
3. ARM64_BUGS_FIXED_SUMMARY.md - Bugs #1 & #2 summary
4. ARM64_BUG4_INVESTIGATION.md - Current crash investigation
5. **ARM64_ANALYSIS_SUMMARY.md** (this file) - Complete overview

---

## Timeline

1. **Initial investigation** - Found memory corruption pattern
2. **Bug #1** - BUF_POS desynchronization (11 locations)
3. **Bug #2** - Alignment infinite loop (2 locations)
4. **Bug #3** - jit_buf() reallocation (3 locations)
5. **Bug #4** - LDUR opcode encoding (1 location)
6. **Current** - Investigating remaining crash (likely Bug #5 or vreg issue)

---

## Hardware Tested

**Device:** Raspberry Pi 5 Model B
**CPU:** ARM Cortex-A76 (64-bit, 4 cores)
**OS:** Linux 6.6.51+rpt-rpi-2712
**Compiler:** GCC 13.2.0

---

## Conclusion

**Progress:** 4 critical bugs fixed, significant improvement in stability
**Status:** Program compiles 247+ functions successfully but still crashes during execution
**Next:** Focus on vreg stackPos investigation (Theory #2) as most promising lead

The fact that we've fixed 4 major bugs and the program now compiles successfully is excellent progress. The remaining crash appears to be a different issue - likely related to how stack positions are calculated or how STR instructions are generated in a specific code path we haven't instrumented yet.
