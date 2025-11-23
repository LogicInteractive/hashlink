# ARM64 Port Investigation - Current Status and Next Steps

**Date:** 2025-11-23
**Current Branch:** `claude/investigate-arm-port-01JdvdUoc1A2ks5TeCHaPDRk`
**Current Machine:** x86_64 (investigation was done on ARM64)

---

## Summary of Situation

### What's Been Done ✅

1. **Identified and implemented 5 runtime fixes:**
   - Makefile: Add `-DHL_JIT_ARM64` flag
   - T_SIZES[HI32] = 8 bytes on ARM64
   - module.c: Zero-extend HI32 constants
   - jit.c OInt: Zero-extend immediate values
   - LOAD/STORE_VREG: Use 64-bit operations for HI32

2. **Verified fixes on ARM64 hardware (Raspberry Pi 5):**
   - HL_JIT_ARM64 present in build commands
   - T_SIZES[HI32] = 8 confirmed in libhl.so .rodata
   - All code changes present and active

3. **Created comprehensive documentation:**
   - ARM64_DEEP_ANALYSIS_HI32_TRUNCATION.md
   - ARM64_INVESTIGATION_STATUS.md
   - ARM64_JIT_BUG_INVESTIGATION.md
   - CRITICAL_BUILD_INSTRUCTIONS.md
   - BYTECODE_TRUNCATION_ROOT_CAUSE.md

### Current Problem ❌

**Despite all fixes, program still crashes:**
```
Thread 1 "hl" received signal SIGSEGV, Segmentation fault.
0x00007ffff7f45f10 in hl_hbset_impl () from ./libhl.so

x0 = 0xf5c433ed  (truncated - should be 0x7ffff5c433ed)
```

### Competing Theories

**Theory A: Bytecode Truncation** (BYTECODE_TRUNCATION_ROOT_CAUSE.md)
- Claims `0xf5c433ed` is stored in bytecode's `ints[]` array
- Concludes problem is in Haxe compiler/bytecode generation
- **Logical flaw:** JIT pointers don't exist during bytecode generation

**Theory B: Runtime Truncation Still Exists** (ANALYSIS_ALTERNATIVE_HYPOTHESIS.md)
- Points out JIT pointers can't be in bytecode (generated at runtime)
- Suggests truncation happens during runtime in missed code path
- Proposes vreg layout, field offset, or type confusion issues
- **Needs evidence:** Comprehensive logging to find actual truncation point

---

## Critical Blockers

### Blocker 1: jit.c Won't Compile ⚠️

**Problem:** Duplicate function definitions in src/jit.c:
- `jit_buf()` (lines 554 and 775)
- `_jit_error()` (lines 599 and 2001)
- `alloc_static_closure()` (lines 3159 and 3250)
- Plus duplicate case statements in ARM64 switch

**Impact:**
- Cannot build on x86_64
- Cannot add debug logging
- Cannot test hypotheses

**Cause:** Code corruption from editing sessions

### Blocker 2: Can't Test on ARM64 from Here

**Problem:** Currently on x86_64 machine
- Can't reproduce crash
- Can't add logging and test
- Can't verify theories

**Impact:** Investigation is stuck without ARM64 access

### Blocker 3: Missing Concrete Evidence

**What we don't know:**
1. Which vreg contains the bad pointer
2. Complete register state at crash
3. Stack frame layout at crash
4. Actual bytecode contents of test_minimal.hl
5. Call stack leading to hl_hbset_impl
6. Whether pointer was ever correct or wrong from start

---

## Recommended Action Plan

### Option A: Fix jit.c and Continue Investigation 🔧

**If you want to continue debugging:**

1. **Clean up jit.c:**
   ```bash
   # Option 1: Revert to last working version
   git log --oneline --all src/jit.c | head -20
   # Find commit before duplicates, checkout that version

   # Option 2: Manually remove duplicates
   # Keep x86 versions, remove ARM64 duplicates
   ```

2. **Add comprehensive logging:**
   ```c
   // In LOAD_VREG macro (src/jit.c):
   if (vr && (vr)->t->kind == HI32) {
       int_val loaded_value;
       // ... after load ...
       fprintf(stderr, "[LOAD_HI32] X%d <- vreg%d pos=%d val=0x%016lx\n",
               tmp_reg, (vr)->stack.id, (vr)->stackPos, loaded_value);
   }

   // In hl_hbset_impl wrapper (src/std/maps.c):
   void *hbset_debug(void *map, void *key, void *val) {
       fprintf(stderr, "[HBSET] map=%p key=%p val=%p\n", map, key, val);
       if (((uintptr_t)map & 0xFFFFFFFF00000000ULL) == 0) {
           fprintf(stderr, "[HBSET] ERROR: map pointer truncated!\n");
           // Add backtrace here
       }
       return hl_hbset_impl(map, key, val);
   }
   ```

3. **Build and test on ARM64:**
   - Must test on actual ARM64 hardware
   - Logging will show where truncation happens
   - Can trace exact code path

### Option B: Check Bytecode Format 📝

**If you believe it's a bytecode issue:**

1. **Fix dump_bytecode compilation:**
   ```bash
   # Need to build just libhl.so, not full HL
   cd src/std
   make types.o obj.o
   # ... build minimal set of objects
   ```

2. **Examine test_minimal.hl:**
   - Look for suspicious integer constants
   - Check if any values near 0xf5c433ed
   - Verify HI32 field initialization

3. **Recompile test on ARM64:**
   - If Haxe available on ARM64, recompile test program there
   - See if native ARM64 bytecode generation helps

### Option 3: Start Fresh with Minimal Test Case 🆕

**If stuck:**

1. **Create absolute minimal Haxe program:**
   ```haxe
   class Minimal {
       static function main() {
           trace("Hello");
       }
   }
   ```

2. **Test incremental complexity:**
   - Just trace: works or crashes?
   - Add function call: works or crashes?
   - Add object creation: works or crashes?
   - Add map operation: works or crashes?

3. **Find exact trigger:**
   - Which Haxe construct causes crash?
   - Does it use HI32 types?
   - Can we create simpler repro?

### Option 4: Consult Upstream 📧

**If investigation is stuck:**

1. **Report to HashLink developers:**
   - Comprehensive analysis already done
   - All fixes attempted and verified
   - Still crashes with pointer truncation
   - May be fundamental bytecode format limitation

2. **Report to Haxe developers:**
   - Possible compiler issue with ARM64 target
   - HI32 used for pointers on 64-bit platforms
   - May need separate pointer-sized int type

---

## Key Questions That Need Answers

### Runtime Questions:
1. **Where exactly does x0 get loaded from?**
   - From a vreg? Which one?
   - From an object field? Which object?
   - From a global?

2. **What's the complete register state at crash?**
   - All X0-X30 registers
   - Stack pointer
   - Program counter

3. **What's the call stack?**
   - Which JIT function calls hl_hbset_impl?
   - What operation triggered this?
   - Can we see the JIT disassembly?

4. **Are field offsets correct?**
   - Print rt->fields_indexes for test objects
   - Compare with hl_type_size() calculations
   - Check for mismatches

### Bytecode Questions:
1. **What's actually in test_minimal.hl?**
   - How many integer constants?
   - Any values near 0xf5c433ed?
   - What are the object constant field types?

2. **What does the Haxe source look like?**
   - Which operations are in the test?
   - What types does Haxe assign?
   - Any explicit casts or type annotations?

3. **Does empty.hl have the same issue?**
   - Or is it specific to certain operations?
   - What's different between empty and test_minimal?

---

## My Assessment

### What I Believe (Based on Logic):

**The "bytecode truncation" theory has a fundamental flaw:**
- JIT function pointers are generated at runtime
- They cannot be stored in bytecode
- `0xf5c433ed` must be truncated at runtime, not compile time

**Most likely scenarios:**
1. **Missed truncation point:** Another `(int)` cast in a code path we haven't checked
2. **Vreg layout bug:** stackPos calculation doesn't account for 8-byte HI32
3. **Field offset mismatch:** Initialization uses different offsets than JIT access
4. **Type confusion:** Value isn't actually a pointer, wrong type somewhere

### What's Needed:

**Concrete evidence through:**
1. Comprehensive logging at every load/store
2. Stack frame validation
3. Field offset verification
4. Bytecode inspection (if dump tool can be built)
5. Minimal test case to isolate exact trigger

### Why We're Stuck:

- jit.c won't compile (duplicates)
- On x86_64, can't test ARM64 code
- No logging to trace actual code flow
- Conclusions drawn without concrete evidence

---

## Conclusion

The investigation has made significant progress:
- ✅ Identified the HI32 pointer size issue
- ✅ Implemented comprehensive fixes
- ✅ Verified fixes on ARM64 hardware
- ❌ Still crashes (indicates missing something)

**Next critical decision:**
- Continue debugging runtime with logging?
- Accept it's bytecode and report upstream?
- Create minimal repro to understand trigger?
- Start over with cleaner approach?

**Recommendation:** Add comprehensive logging, rebuild on ARM64, and trace the ACTUAL code path. The "bytecode truncation" conclusion may be premature without concrete evidence of what's actually in the bytecode or where the runtime truncation occurs.

---

**Status:** Blocked pending:
1. jit.c cleanup/rebuild, OR
2. Access to working ARM64 build with logging, OR
3. Decision to report upstream with current findings

**Machine:** x86_64 (investigation was on ARM64 Raspberry Pi 5)
**Last Test:** Exit 139 (SIGSEGV) with x0=0xf5c433ed on ARM64
