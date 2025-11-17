# ARM64 JIT Status Report

## Current State: Phase 1 - vclosure Offset Bugs Fixed, Runtime Issues Remain

### ✅ What Works
- JIT Compilation: Successfully compiles HashLink bytecode to ARM64 machine code
- Operation Coverage: 50+ operations implemented
- Jump/Branch Handling: opsPos tracking fixed, jump patching works
- Stack-Based System: All vregs stored on stack, LOAD/STORE macros functional
- C-Level Tests: Runtime initializes, JIT backend detected
- **MOVZ/MOVK Encoding: Fixed critical instruction encoding bugs**
- **vclosure Offset Fixes: Fixed closure->fun, closure->value, closure->hasValue loading**

### ⚠️ Current Status
**Build:** ✅ Compiles successfully (no errors)
**Runtime:** ❌ Test programs still crash at PC=0x0 (SIGSEGV)

### 🐛 Recent Bug Fixes (2025-11-17)

**Critical vclosure Structure Offset Bugs Fixed:**
1. ✅ `closure->fun` now loads from offset 1 (8 bytes), not 0
2. ✅ `closure->value` now loads from offset 3 (24 bytes), not 1
3. ✅ `closure->hasValue` now uses 32-bit load with correct offset
4. ✅ vclosure initialization in main.c completed (stackCount, value fields)

**Previous Fixes:**
- ✅ MOVZ instruction encoding (0x52 → 0xA5)
- ✅ MOVK instruction encoding (0x72 → 0xE5)
- ✅ All GET_REG bugs eliminated (38 operations)
- ✅ Instruction cache flush added

**Still Crashing:** Despite fixes, programs crash with SIGSEGV at PC=0x0 when calling entry point function. Issue is inside JIT-compiled code, not setup. See ARM64_VCLOSURE_INVESTIGATION.md for detailed analysis.

### ✅ All GET_REG Operations Fixed (38 total)

**Tier 1: Call Operations (9 uses)**
1. OCallMethod - Method calls on objects ✓
2. OCallThis - Calls on "this" object ✓
3. OCallClosure - Closure invocation (2 cases) ✓

**Tier 2: Dynamic Operations (2 uses)**
4. ODynGet - Dynamic field getter ✓

**Tier 3: Type Conversions (8 uses)**
5. OFloat - Float constant loading ✓
6. OToSFloat - Signed int → float ✓
7. OToUFloat - Unsigned int → float ✓
8. OToVirtual - Type → virtual ✓

**Tier 4: Control & Exception Handling (4 uses)**
9. OThrow - Throw exception ✓
10. ORethrow - Rethrow exception ✓
11. OSwitch - Switch statement ✓
12. ONullCheck - Null pointer check ✓

**Tier 5: Object Operations (3 uses)**
13. ONew - Object allocation ✓
14. OSetEnumField - Enum field setter ✓

**Previously Fixed:**
- OStaticClosure - Static closure allocation ✓
- OInstanceClosure - Instance closure allocation ✓
- OVirtualClosure - Virtual closure allocation ✓

### Operations Completed (50+)
See full list in SESSION_PROGRESS.md

### Critical Fixes This Session
- **All GET_REG bugs fixed:** 38 operations now use LOAD_VREG/STORE_VREG
- opsPos tracking: Fixed jump patching
- Removed invalid operations (OGetI32/OSetI32)
- Added arm_lsl_imm forward declaration

### Next Steps (See ARM64_VCLOSURE_INVESTIGATION.md for Details)

**Priority 1: Identify Failing BLR**
- Disassemble JIT code at entry point offset (0x1a03c)
- Find which BLR instruction jumps to NULL
- Trace back to generating operation

**Priority 2: Check Global Function Pointers**
- Verify OGetGlobal loads valid function pointers
- Check if `null_function` is properly accessible from JIT
- Add runtime logging before function calls

**Priority 3: Verify Remaining Operations**
- Check OCall, OCallMethod, OCallThis implementations
- Verify all indirect call operations
- Test with minimal JIT-only program

**Current Focus:** The crash happens inside the JIT-compiled entry point function when it tries to call another function. Even absolutely empty Haxe programs crash, suggesting the issue is in runtime initialization code that runs before/during main().
