# HashLink ARM64 JIT Implementation Plan

**Target:** Enable HashLink JIT on ARM64 platforms (Raspberry Pi, Android, Apple Silicon)
**Primary Target:** ARM64/AArch64 (will add ARM32 later if needed)
**Scope:** JIT compiler only (not hlc/AOT)

---

## ARCHITECTURE OVERVIEW

### Current x86-64 Flow
```
.hl bytecode → JIT compiler → x86-64 machine code → execute
                (jit.c)
```

### Target ARM64 Flow
```
.hl bytecode → JIT compiler → ARM64 machine code → execute
                (jit.c with ARM backend)
```

### Key Architectural Decision: Dual Backend Approach

```c
// jit.c structure (conceptual)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x86-64 backend (existing code)
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARM64 backend (new code)
#else
    #error "Unsupported architecture"
#endif
```

---

## DEVELOPMENT ENVIRONMENT: What Can Be Done Where?

### ✅ Work You Can Do HERE (No ARM Hardware Required)

**Phase 0-2: Refactoring and Scaffolding**
- Refactor existing x86 code into conditional blocks
- Add ARM64 register definitions and stubs
- Implement instruction encoders
- Cross-compile for ARM64 (catches build errors, no execution)
- Unit tests for instruction encoding (compare vs clang output)
- Code review and static analysis

**Tools needed:**
```bash
# Cross-compilation toolchain
sudo apt-get install gcc-aarch64-linux-gnu
# Can build for ARM64 without running
```

### ⚠️ Work That REQUIRES ARM Hardware

**Phase 3+: Actual Execution**
- Running JIT-compiled code (needs instruction cache flush)
- GC stress testing under load
- Debugging crashes with real stack traces
- Exception handling validation
- Performance measurement and optimization

**Hardware options:**
- Raspberry Pi 4/5 (ARM64 Linux) - **Recommended**
- Android device with Termux
- Apple Silicon Mac
- Cloud ARM64 instance (AWS Graviton, Oracle ARM)

**Strategy:** Start with Phases 0-2 here, then set up ARM hardware for Phase 3+

---

## PHASE 0: SAFE REFACTORING (Days 1-2) **← START HERE**

**Goal:** Create dual-backend structure WITHOUT breaking x86

**Why do this first:**
- Creates clean baseline
- Proves x86 still works
- Makes ARM64 addition safe
- Reduces risk

### 0.1 Wrap Existing x86 Code

**File:** `src/jit.c`

**Step 1: Add architecture detection at top of file**
```c
// After includes, before line 29

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#   define HL_X86
#elif defined(__aarch64__) || defined(_M_ARM64)
#   define HL_ARM64
#elif defined(__arm__) || defined(_M_ARM)
#   define HL_ARM32
#   error "ARM32 not yet supported, use ARM64 or x86-64"
#else
#   error "JIT does not support this processor architecture"
#endif
```

**Step 2: Wrap x86-specific register enums**
```c
// Lines 37-57: Wrap in #ifdef HL_X86

#ifdef HL_X86

typedef enum {
    Eax = 0,
    Ecx = 1,
    // ... existing x86 registers ...
    _LAST = 0xFF
} CpuReg;

#endif // HL_X86
```

**Step 3: Wrap x86 instruction opcodes**
```c
// Lines 59-129: Wrap in #ifdef HL_X86

#ifdef HL_X86

typedef enum {
    MOV,
    LEA,
    // ... existing x86 opcodes ...
    _CPU_LAST
} CpuOp;

#endif // HL_X86
```

**Step 4: Wrap x86 calling convention**
```c
// Lines 228-239: Wrap in #ifdef HL_X86

#ifdef HL_X86
// ... existing CALL_REGS etc ...
#endif
```

**Step 5: Verify x86 still builds and works**
```bash
make clean
make
./hl other/tests/hello.hl  # Should work exactly as before
```

**Deliverable:**
- x86 builds and all tests pass
- Code is ready for ARM64 addition
- Commit: "Refactor: Wrap x86 JIT in conditional compilation blocks"

### 0.2 Add ARM64 Stub Structure

**Add after x86 sections:**
```c
#ifdef HL_ARM64

// ARM64 backend - STUB IMPLEMENTATION
// TODO: Implement in Phase 1+

typedef enum {
    X0 = 0, X1, X2, X3, X4, X5, X6, X7,
    // ... (full ARM64 register set)
} Arm64Reg;

// Instruction encoders - stubbed
static void arm64_stub_error(const char *msg) {
    hl_fatal("ARM64 JIT not yet implemented: %s", msg);
}

#endif // HL_ARM64
```

**Update hl_jit_function() to call stub on ARM64:**
```c
int hl_jit_function( jit_ctx *ctx, hl_module *m, hl_function *f ) {
#ifdef HL_ARM64
    arm64_stub_error("hl_jit_function");
    return -1;
#else
    // ... existing x86 implementation ...
#endif
}
```

**Deliverable:**
- Code compiles for both x86 and ARM64 (cross-compile)
- x86 works, ARM64 errors gracefully
- Commit: "Add ARM64 stub structure (non-functional)"

---

## PHASE 1: FOUNDATION (Days 3-5)

### 1.1 Platform Detection & Conditional Compilation

**File:** `src/jit.c` (lines 29-31)

**Current Code:**
```c
#ifdef __arm__
#	error "JIT does not support ARM processors..."
#endif
```

**New Code:**
```c
// Architecture detection
#if defined(__aarch64__) || defined(_M_ARM64)
#   define HL_ARM64
#elif defined(__arm__) || defined(_M_ARM)
#   define HL_ARM32
#   error "ARM32 not yet supported, use ARM64"
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#   define HL_X86
#else
#   error "JIT does not support this processor architecture"
#endif
```

**Deliverable:** Code compiles on ARM64 without immediate error

---

### 1.2 ARM64 Register Definitions

**File:** `src/jit.c` (after line 57)

**Add ARM64 registers:**
```c
#ifdef HL_ARM64

typedef enum {
    // General purpose registers
    X0  = 0,  X1  = 1,  X2  = 2,  X3  = 3,
    X4  = 4,  X5  = 5,  X6  = 6,  X7  = 7,
    X8  = 8,  X9  = 9,  X10 = 10, X11 = 11,
    X12 = 12, X13 = 13, X14 = 14, X15 = 15,
    X16 = 16, X17 = 17, X18 = 18, X19 = 19,
    X20 = 20, X21 = 21, X22 = 22, X23 = 23,
    X24 = 24, X25 = 25, X26 = 26, X27 = 27,
    X28 = 28, X29 = 29, X30 = 30, XZR = 31,

    // Aliases
    FP  = 29,  // Frame pointer
    LR  = 30,  // Link register
    SP  = 31,  // Stack pointer (when used as base)

    _ARM_LAST = 0xFF
} Arm64Reg;

// Floating point / SIMD registers
typedef enum {
    V0  = 0,  V1  = 1,  V2  = 2,  V3  = 3,
    V4  = 4,  V5  = 5,  V6  = 6,  V7  = 7,
    V8  = 8,  V9  = 9,  V10 = 10, V11 = 11,
    V12 = 12, V13 = 13, V14 = 14, V15 = 15,
    V16 = 16, V17 = 17, V18 = 18, V19 = 19,
    V20 = 20, V21 = 21, V22 = 22, V23 = 23,
    V24 = 24, V25 = 25, V26 = 26, V27 = 27,
    V28 = 28, V29 = 29, V30 = 30, V31 = 31
} Arm64FpReg;

#endif // HL_ARM64
```

**Deliverable:** ARM64 register set defined

---

### 1.3 AAPCS64 Calling Convention

**Reference:** ARM Architecture Procedure Call Standard (AAPCS64)

**Register Usage:**
```c
#ifdef HL_ARM64

// AAPCS64 calling convention
#define CALL_NREGS 8
static const Arm64Reg CALL_REGS[] = { X0, X1, X2, X3, X4, X5, X6, X7 };

// FP argument registers
#define FP_ARG_NREGS 8
static const Arm64FpReg FP_ARG_REGS[] = { V0, V1, V2, V3, V4, V5, V6, V7 };

// Return value registers
#define RET_REG X0
#define RET_FP_REG V0

// Callee-saved registers (must preserve)
static const Arm64Reg CALLEE_SAVED[] = {
    X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30
};

// Caller-saved registers (can trash)
static const Arm64Reg CALLER_SAVED[] = {
    X0, X1, X2, X3, X4, X5, X6, X7, X8, X9, X10, X11, X12, X13, X14, X15, X16, X17
};

// Callee-saved FP registers (must preserve)
static const Arm64FpReg FP_CALLEE_SAVED[] = {
    V8, V9, V10, V11, V12, V13, V14, V15
};

#endif // HL_ARM64
```

**Deliverable:** Calling convention constants defined

---

### 1.4 Instruction Cache Flushing

**File:** `src/jit.c` (add after code generation helper functions)

**Implementation:**
```c
#ifdef HL_ARM64

static void arm64_flush_icache(jit_ctx *ctx, void *start, int size) {
    // ARM64 requires explicit instruction cache flush
    // GCC/Clang provide __builtin___clear_cache for this
    void *end = (unsigned char*)start + size;
    __builtin___clear_cache(start, end);
}

#endif // HL_ARM64
```

**Integration Point:**
```c
// In hl_jit_code() function (line 4606)
void *hl_jit_code( jit_ctx *ctx, hl_module *m, int *codesize, ... ) {
    // ... existing code ...
    memcpy(code, ctx->startBuf, BUF_POS());

#ifdef HL_ARM64
    arm64_flush_icache(ctx, code, BUF_POS());
#endif

    // ... rest of function ...
}
```

**Deliverable:** Cache flushing infrastructure in place

---

## PHASE 2: INSTRUCTION ENCODING (Days 4-8)

### 2.1 Basic Encoding Helpers

ARM64 uses fixed 32-bit instruction encoding. All instructions follow consistent patterns.

**File:** `src/jit.c`

```c
#ifdef HL_ARM64

// Emit a 32-bit ARM64 instruction
#define EMIT32(val) *ctx->buf.w++ = (val)

// Encode register number (5 bits)
#define REG(r) ((r) & 0x1F)

// Encode immediate values
static bool is_imm12(int val) {
    return val >= 0 && val <= 4095;
}

static bool is_logical_imm(uint64_t val) {
    // ARM64 logical immediates are complex - can encode many patterns
    // but not all values. For now, we'll use a lookup or calculation.
    // Simplified check for common cases
    return val != 0 && val != ~0ULL;
}

// Encode condition codes
typedef enum {
    COND_EQ = 0x0,  // Equal
    COND_NE = 0x1,  // Not equal
    COND_CS = 0x2,  // Carry set (unsigned >=)
    COND_CC = 0x3,  // Carry clear (unsigned <)
    COND_MI = 0x4,  // Minus (negative)
    COND_PL = 0x5,  // Plus (positive or zero)
    COND_VS = 0x6,  // Overflow set
    COND_VC = 0x7,  // Overflow clear
    COND_HI = 0x8,  // Unsigned higher
    COND_LS = 0x9,  // Unsigned lower or same
    COND_GE = 0xA,  // Signed >=
    COND_LT = 0xB,  // Signed <
    COND_GT = 0xC,  // Signed >
    COND_LE = 0xD,  // Signed <=
    COND_AL = 0xE,  // Always
    COND_NV = 0xF   // Never (used for other purposes)
} Arm64Cond;

#endif // HL_ARM64
```

**Deliverable:** Basic encoding infrastructure

---

### 2.2 MOV Instructions

ARM64 has multiple MOV variants:
- `MOV Xd, Xn` - register to register
- `MOV Xd, #imm` - immediate to register (actually MOVZ/MOVN/ORR)
- `MOVZ` - move 16-bit immediate with zeros
- `MOVK` - move 16-bit immediate keeping other bits

```c
#ifdef HL_ARM64

// MOV register to register (actually ORR with XZR)
static void arm64_mov_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src) {
    // ORR Xd, XZR, Xm
    uint32_t insn = 0xAA0003E0 | (REG(src) << 16) | REG(dst);
    EMIT32(insn);
}

// MOV immediate (16-bit value, can shift by 0, 16, 32, 48)
static void arm64_movz(jit_ctx *ctx, Arm64Reg dst, uint16_t imm, int shift) {
    // MOVZ Xd, #imm, LSL #shift
    uint32_t hw = shift / 16;  // which 16-bit chunk (0-3)
    uint32_t insn = 0xD2800000 | (hw << 21) | (imm << 5) | REG(dst);
    EMIT32(insn);
}

// MOVK - move 16-bit keeping other bits
static void arm64_movk(jit_ctx *ctx, Arm64Reg dst, uint16_t imm, int shift) {
    uint32_t hw = shift / 16;
    uint32_t insn = 0xF2800000 | (hw << 21) | (imm << 5) | REG(dst);
    EMIT32(insn);
}

// MOV 32-bit or 64-bit immediate
static void arm64_mov_imm64(jit_ctx *ctx, Arm64Reg dst, uint64_t imm) {
    // Build 64-bit value using MOVZ + MOVK sequence
    arm64_movz(ctx, dst, imm & 0xFFFF, 0);
    if ((imm >> 16) & 0xFFFF)
        arm64_movk(ctx, dst, (imm >> 16) & 0xFFFF, 16);
    if ((imm >> 32) & 0xFFFF)
        arm64_movk(ctx, dst, (imm >> 32) & 0xFFFF, 32);
    if ((imm >> 48) & 0xFFFF)
        arm64_movk(ctx, dst, (imm >> 48) & 0xFFFF, 48);
}

#endif // HL_ARM64
```

**Test:** Write a simple test that moves values between registers

---

### 2.3 Arithmetic Operations

```c
#ifdef HL_ARM64

// ADD (register)
static void arm64_add_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    // ADD Xd, Xn, Xm
    uint32_t insn = 0x8B000000 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// ADD (immediate)
static void arm64_add_imm(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src, int imm) {
    // ADD Xd, Xn, #imm (12-bit unsigned immediate)
    if (!is_imm12(imm)) {
        // Load to temp register and use register form
        arm64_mov_imm64(ctx, X16, imm);
        arm64_add_reg(ctx, dst, src, X16);
        return;
    }
    uint32_t insn = 0x91000000 | (imm << 10) | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

// SUB (register)
static void arm64_sub_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    // SUB Xd, Xn, Xm
    uint32_t insn = 0xCB000000 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// SUB (immediate)
static void arm64_sub_imm(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src, int imm) {
    if (!is_imm12(imm)) {
        arm64_mov_imm64(ctx, X16, imm);
        arm64_sub_reg(ctx, dst, src, X16);
        return;
    }
    uint32_t insn = 0xD1000000 | (imm << 10) | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

// MUL
static void arm64_mul(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    // MADD Xd, Xn, Xm, XZR (multiply-add with zero = multiply)
    uint32_t insn = 0x9B007C00 | (REG(src2) << 16) | (REG(XZR) << 10) |
                    (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// SDIV (signed divide)
static void arm64_sdiv(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    uint32_t insn = 0x9AC00C00 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// UDIV (unsigned divide)
static void arm64_udiv(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    uint32_t insn = 0x9AC00800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

#endif // HL_ARM64
```

**Deliverable:** Basic arithmetic operations

---

### 2.4 Bitwise Operations

```c
#ifdef HL_ARM64

// AND (register)
static void arm64_and_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    uint32_t insn = 0x8A000000 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// ORR (register)
static void arm64_orr_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    uint32_t insn = 0xAA000000 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// EOR (XOR, register)
static void arm64_eor_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
    uint32_t insn = 0xCA000000 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// LSL (logical shift left)
static void arm64_lsl(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src, int shift) {
    // UBFM (Unsigned Bitfield Move) used for LSL
    uint32_t insn = 0xD3400000 | ((64-shift) << 16) | ((63-shift) << 10) |
                    (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

// LSR (logical shift right)
static void arm64_lsr(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src, int shift) {
    uint32_t insn = 0xD3400000 | (shift << 16) | (63 << 10) |
                    (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

// ASR (arithmetic shift right)
static void arm64_asr(jit_ctx *ctx, Arm64Reg dst, Arm64Reg src, int shift) {
    // SBFM (Signed Bitfield Move)
    uint32_t insn = 0x93400000 | (shift << 16) | (63 << 10) |
                    (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

#endif // HL_ARM64
```

**Deliverable:** Bitwise operations

---

## PHASE 3: MEMORY OPERATIONS (Days 9-11)

### 3.1 Load/Store Instructions

```c
#ifdef HL_ARM64

// LDR (load register, immediate offset)
// offset must be aligned to 8 bytes for 64-bit loads
static void arm64_ldr_imm(jit_ctx *ctx, Arm64Reg dst, Arm64Reg base, int offset) {
    if (offset < 0 || offset > 32760 || (offset & 7) != 0) {
        // Use register offset form
        arm64_mov_imm64(ctx, X16, offset);
        arm64_ldr_reg(ctx, dst, base, X16);
        return;
    }
    // LDR Xt, [Xn, #offset]
    uint32_t imm12 = offset >> 3;  // divide by 8 for encoding
    uint32_t insn = 0xF9400000 | (imm12 << 10) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// LDR (load register, register offset)
static void arm64_ldr_reg(jit_ctx *ctx, Arm64Reg dst, Arm64Reg base, Arm64Reg offset) {
    // LDR Xt, [Xn, Xm]
    uint32_t insn = 0xF8606800 | (REG(offset) << 16) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// STR (store register, immediate offset)
static void arm64_str_imm(jit_ctx *ctx, Arm64Reg src, Arm64Reg base, int offset) {
    if (offset < 0 || offset > 32760 || (offset & 7) != 0) {
        arm64_mov_imm64(ctx, X16, offset);
        arm64_str_reg(ctx, src, base, X16);
        return;
    }
    uint32_t imm12 = offset >> 3;
    uint32_t insn = 0xF9000000 | (imm12 << 10) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

// STR (store register, register offset)
static void arm64_str_reg(jit_ctx *ctx, Arm64Reg src, Arm64Reg base, Arm64Reg offset) {
    uint32_t insn = 0xF8206800 | (REG(offset) << 16) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

// LDRB (load byte)
static void arm64_ldrb(jit_ctx *ctx, Arm64Reg dst, Arm64Reg base, int offset) {
    uint32_t insn = 0x39400000 | (offset << 10) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// STRB (store byte)
static void arm64_strb(jit_ctx *ctx, Arm64Reg src, Arm64Reg base, int offset) {
    uint32_t insn = 0x39000000 | (offset << 10) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

// LDRH (load halfword)
static void arm64_ldrh(jit_ctx *ctx, Arm64Reg dst, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 1;
    uint32_t insn = 0x79400000 | (imm12 << 10) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// STRH (store halfword)
static void arm64_strh(jit_ctx *ctx, Arm64Reg src, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 1;
    uint32_t insn = 0x79000000 | (imm12 << 10) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

// LDRW (load 32-bit word)
static void arm64_ldr32_imm(jit_ctx *ctx, Arm64Reg dst, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 2;
    uint32_t insn = 0xB9400000 | (imm12 << 10) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// STRW (store 32-bit word)
static void arm64_str32_imm(jit_ctx *ctx, Arm64Reg src, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 2;
    uint32_t insn = 0xB9000000 | (imm12 << 10) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

#endif // HL_ARM64
```

**Deliverable:** Load/store infrastructure

---

### 3.2 Stack Operations

ARM64 doesn't have dedicated push/pop like x86. Use STP/LDP (store/load pair).

```c
#ifdef HL_ARM64

// Push registers onto stack (pre-decrement)
static void arm64_push_pair(jit_ctx *ctx, Arm64Reg r1, Arm64Reg r2) {
    // STP X1, X2, [SP, #-16]!
    uint32_t insn = 0xA9BF0000 | (REG(r2) << 10) | (REG(SP) << 5) | REG(r1);
    EMIT32(insn);
}

// Pop registers from stack (post-increment)
static void arm64_pop_pair(jit_ctx *ctx, Arm64Reg r1, Arm64Reg r2) {
    // LDP X1, X2, [SP], #16
    uint32_t insn = 0xA8C10000 | (REG(r2) << 10) | (REG(SP) << 5) | REG(r1);
    EMIT32(insn);
}

// Adjust stack pointer
static void arm64_sub_sp(jit_ctx *ctx, int bytes) {
    // SUB SP, SP, #bytes
    arm64_sub_imm(ctx, SP, SP, bytes);
}

static void arm64_add_sp(jit_ctx *ctx, int bytes) {
    // ADD SP, SP, #bytes
    arm64_add_imm(ctx, SP, SP, bytes);
}

#endif // HL_ARM64
```

**Deliverable:** Stack manipulation

---

## PHASE 4: CONTROL FLOW (Days 12-15)

### 4.1 Comparisons

```c
#ifdef HL_ARM64

// CMP (compare, sets flags)
static void arm64_cmp_reg(jit_ctx *ctx, Arm64Reg r1, Arm64Reg r2) {
    // SUBS XZR, Xn, Xm (subtract and set flags, discard result)
    uint32_t insn = 0xEB000000 | (REG(r2) << 16) | (REG(r1) << 5) | REG(XZR);
    EMIT32(insn);
}

static void arm64_cmp_imm(jit_ctx *ctx, Arm64Reg r, int imm) {
    // SUBS XZR, Xn, #imm
    if (!is_imm12(imm)) {
        arm64_mov_imm64(ctx, X16, imm);
        arm64_cmp_reg(ctx, r, X16);
        return;
    }
    uint32_t insn = 0xF1000000 | (imm << 10) | (REG(r) << 5) | REG(XZR);
    EMIT32(insn);
}

// TST (test bits, sets flags)
static void arm64_tst_reg(jit_ctx *ctx, Arm64Reg r1, Arm64Reg r2) {
    // ANDS XZR, Xn, Xm
    uint32_t insn = 0xEA000000 | (REG(r2) << 16) | (REG(r1) << 5) | REG(XZR);
    EMIT32(insn);
}

#endif // HL_ARM64
```

---

### 4.2 Branches

```c
#ifdef HL_ARM64

// B (unconditional branch, PC-relative)
static int arm64_b(jit_ctx *ctx, int offset) {
    // offset is in instructions (4-byte units), range ±128MB
    int pos = BUF_POS();
    uint32_t imm26 = (offset >> 2) & 0x3FFFFFF;
    uint32_t insn = 0x14000000 | imm26;
    EMIT32(insn);
    return pos;
}

// B.cond (conditional branch)
static int arm64_b_cond(jit_ctx *ctx, Arm64Cond cond, int offset) {
    // offset in instructions, range ±1MB
    int pos = BUF_POS();
    uint32_t imm19 = (offset >> 2) & 0x7FFFF;
    uint32_t insn = 0x54000000 | (imm19 << 5) | cond;
    EMIT32(insn);
    return pos;
}

// BL (branch with link, call)
static void arm64_bl(jit_ctx *ctx, int offset) {
    uint32_t imm26 = (offset >> 2) & 0x3FFFFFF;
    uint32_t insn = 0x94000000 | imm26;
    EMIT32(insn);
}

// BLR (branch with link to register)
static void arm64_blr(jit_ctx *ctx, Arm64Reg reg) {
    uint32_t insn = 0xD63F0000 | (REG(reg) << 5);
    EMIT32(insn);
}

// BR (branch to register)
static void arm64_br(jit_ctx *ctx, Arm64Reg reg) {
    uint32_t insn = 0xD61F0000 | (REG(reg) << 5);
    EMIT32(insn);
}

// RET (return)
static void arm64_ret(jit_ctx *ctx) {
    // RET (returns to address in X30/LR)
    uint32_t insn = 0xD65F0000 | (REG(LR) << 5);
    EMIT32(insn);
}

#endif // HL_ARM64
```

---

### 4.3 Jump Patching

ARM64 branches are PC-relative and must be patched after addresses are known.

```c
#ifdef HL_ARM64

// Patch a forward branch
static void arm64_patch_branch(jit_ctx *ctx, int branch_pos, int target_pos) {
    int offset = target_pos - branch_pos;
    uint32_t *insn_ptr = (uint32_t*)(ctx->startBuf + branch_pos);
    uint32_t insn = *insn_ptr;

    // Check instruction type
    if ((insn & 0xFC000000) == 0x14000000) {
        // Unconditional branch (B)
        uint32_t imm26 = (offset >> 2) & 0x3FFFFFF;
        *insn_ptr = (insn & 0xFC000000) | imm26;
    } else if ((insn & 0xFF000000) == 0x54000000) {
        // Conditional branch (B.cond)
        uint32_t imm19 = (offset >> 2) & 0x7FFFF;
        *insn_ptr = (insn & 0xFF00001F) | (imm19 << 5);
    }
}

// Similar to x86 jump patching in existing code
static void patch_jump_arm64(jit_ctx *ctx, int p) {
    int target = BUF_POS();
    arm64_patch_branch(ctx, p, target);
}

#endif // HL_ARM64
```

**Deliverable:** Complete control flow support

---

## PHASE 5: FLOATING POINT (Days 16-19)

### 5.1 FP Load/Store

```c
#ifdef HL_ARM64

// LDR (FP/SIMD register, 64-bit double)
static void arm64_ldr_fp64(jit_ctx *ctx, Arm64FpReg dst, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 3;
    uint32_t insn = 0xFD400000 | (imm12 << 10) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// STR (FP/SIMD register, 64-bit double)
static void arm64_str_fp64(jit_ctx *ctx, Arm64FpReg src, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 3;
    uint32_t insn = 0xFD000000 | (imm12 << 10) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

// LDR (FP/SIMD register, 32-bit float)
static void arm64_ldr_fp32(jit_ctx *ctx, Arm64FpReg dst, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 2;
    uint32_t insn = 0xBD400000 | (imm12 << 10) | (REG(base) << 5) | REG(dst);
    EMIT32(insn);
}

// STR (FP/SIMD register, 32-bit float)
static void arm64_str_fp32(jit_ctx *ctx, Arm64FpReg src, Arm64Reg base, int offset) {
    uint32_t imm12 = offset >> 2;
    uint32_t insn = 0xBD000000 | (imm12 << 10) | (REG(base) << 5) | REG(src);
    EMIT32(insn);
}

#endif // HL_ARM64
```

---

### 5.2 FP Arithmetic

```c
#ifdef HL_ARM64

// FADD (double)
static void arm64_fadd_d(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E602800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FSUB (double)
static void arm64_fsub_d(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E603800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FMUL (double)
static void arm64_fmul_d(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E600800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FDIV (double)
static void arm64_fdiv_d(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E601800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FADD (float)
static void arm64_fadd_s(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E202800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FSUB (float)
static void arm64_fsub_s(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E203800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FMUL (float)
static void arm64_fmul_s(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E200800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

// FDIV (float)
static void arm64_fdiv_s(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
    uint32_t insn = 0x1E201800 | (REG(src2) << 16) | (REG(src1) << 5) | REG(dst);
    EMIT32(insn);
}

#endif // HL_ARM64
```

---

### 5.3 FP Conversions

```c
#ifdef HL_ARM64

// SCVTF (signed int to float/double)
static void arm64_scvtf_d(jit_ctx *ctx, Arm64FpReg dst, Arm64Reg src) {
    // SCVTF Dd, Xn (64-bit int to double)
    uint32_t insn = 0x9E620000 | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

static void arm64_scvtf_s(jit_ctx *ctx, Arm64FpReg dst, Arm64Reg src) {
    // SCVTF Sd, Wn (32-bit int to float)
    uint32_t insn = 0x1E220000 | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

// FCVTZS (float/double to signed int)
static void arm64_fcvtzs_d(jit_ctx *ctx, Arm64Reg dst, Arm64FpReg src) {
    // FCVTZS Xd, Dn (double to 64-bit int)
    uint32_t insn = 0x9E780000 | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

static void arm64_fcvtzs_s(jit_ctx *ctx, Arm64Reg dst, Arm64FpReg src) {
    // FCVTZS Wd, Sn (float to 32-bit int)
    uint32_t insn = 0x1E380000 | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

// FCVT (convert between float and double)
static void arm64_fcvt_ds(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src) {
    // FCVT Dd, Sn (float to double)
    uint32_t insn = 0x1E22C000 | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

static void arm64_fcvt_sd(jit_ctx *ctx, Arm64FpReg dst, Arm64FpReg src) {
    // FCVT Sd, Dn (double to float)
    uint32_t insn = 0x1E624000 | (REG(src) << 5) | REG(dst);
    EMIT32(insn);
}

#endif // HL_ARM64
```

---

### 5.4 FP Comparisons

```c
#ifdef HL_ARM64

// FCMP (compare doubles)
static void arm64_fcmp_d(jit_ctx *ctx, Arm64FpReg r1, Arm64FpReg r2) {
    uint32_t insn = 0x1E602000 | (REG(r2) << 16) | (REG(r1) << 5);
    EMIT32(insn);
}

// FCMP (compare floats)
static void arm64_fcmp_s(jit_ctx *ctx, Arm64FpReg r1, Arm64FpReg r2) {
    uint32_t insn = 0x1E202000 | (REG(r2) << 16) | (REG(r1) << 5);
    EMIT32(insn);
}

#endif // HL_ARM64
```

**Deliverable:** Complete floating-point support

---

## PHASE 6: INTEGRATION (Days 20-25)

**⚠️ CRITICAL: This is where ChatGPT identified the hardest parts**

### 6.1 Understanding HashLink's Register Allocator (STUDY FIRST)

**The Reality Check:**
The plan so far showed simplified examples like:
```c
arm64_mov_reg(ctx, dst->id, src->id);  // WRONG - too simple!
```

**The truth:** `dst->id` is NOT a hardware register - it's a vreg (virtual register) index.

**How HashLink's JIT Really Works:**

```c
// jit.c has these key structures:

typedef enum {
    RCPU = 0,    // In a CPU register
    RFPU = 1,    // In an FPU/SIMD register
    RSTACK = 2,  // Spilled to stack
    RCONST = 3,  // Constant value
    RADDR = 4,   // Address calculation
    // ...
} preg_kind;

typedef struct {
    preg_kind kind;
    int id;           // Register number OR stack offset
    vreg *holds;      // Which vreg is currently in this preg
    int lock;         // Reference count
} preg;

typedef struct {
    hl_type *t;
    int stack_index;
    preg *current;    // Which preg currently holds this vreg (or NULL if spilled)
    int size;
} vreg;
```

**The Key Functions You MUST Understand:**

```c
// Load vreg into a physical register
static void load(jit_ctx *ctx, preg *r, vreg *v);

// Store preg back to vreg's home location
static void store(jit_ctx *ctx, vreg *r, preg *v, bool bind);

// Bind vreg to preg
static void reg_bind(vreg *r, preg *p);

// Allocate a physical register
static preg *alloc_reg(jit_ctx *ctx, preg_kind kind);

// Mark registers as no longer holding their values
static void discard_regs(jit_ctx *ctx, bool native_call);
```

**How Opcode Translation REALLY Works:**

```c
// WRONG (from simplified examples):
case OMov:
    arm64_mov_reg(ctx, dst->id, src->id);  // NO!
    break;

// RIGHT (how it actually works):
case OMov:
    vreg *dst_vreg = R(op->p1);  // Get destination vreg
    vreg *src_vreg = R(op->p2);  // Get source vreg

    // Allocate/get physical registers
    preg *dst_preg = alloc_reg(ctx, RCPU);
    preg *src_preg = fetch(src_vreg);

    // Actually emit the move
    #ifdef HL_ARM64
        arm64_mov_reg(ctx, dst_preg->id, src_preg->id);
    #endif

    // Bind result
    store(ctx, dst_vreg, dst_preg, true);
    break;
```

**Study These Functions in jit.c:**
- Line ~990: `load()` - loads vreg to preg
- Line ~1237: `store()` - stores preg to vreg
- Line ~1021: `reg_bind()` - binds vreg to preg
- Line ~884: `call_reg_index()` - maps AAPCS calling convention

**Action Items for Phase 6:**
1. Read lines 194-430 in jit.c (vreg/preg structures)
2. Trace through one opcode (e.g., OAdd) and see how load/store work
3. Understand how x86 handles register pressure and spilling
4. Port this logic to ARM64's different register set

**Deliverable:** Deep understanding of vreg→preg system before coding

---

### 6.2 GC Stack Maps and Frame Descriptors (CRITICAL)

**What ChatGPT Caught:** GC needs to know where roots are at safepoints.

**Study the Existing x86 Implementation:**

```c
// In module.c, line 42-84:
static bool module_resolve_pos( hl_module *m, void *addr, int *fidx, int *fpos );

// In jit.c, line 4552-4558:
if( ctx->debug ) {
    int fid = (int)(f - m->code->functions);
    ctx->debug[fid].start = codePos;
    ctx->debug[fid].offsets = debug32 ? (void*)debug32 : (void*)debug16;
    ctx->debug[fid].large = debug32 != NULL;
}
```

**What This Does:**
- For each function, records where in JIT code it starts
- For each opcode, records byte offset from function start
- GC can walk stack and find which opcode was executing
- Uses this to find live roots

**ARM64 Must Do the Same:**

```c
// Structure to understand:
typedef struct {
    void *offsets;     // Array of opcode byte offsets
    int start;         // Function start in JIT code
    bool large;        // Use 32-bit offsets (vs 16-bit)
} hl_debug_infos;
```

**The Critical Question:**
How does HL GC know which registers/stack slots contain GC-managed pointers?

**Answer (need to verify by reading code):**
- HL types encode whether they're GC-managed (hl_is_ptr)
- Stack frame layout is predictable from vreg allocation
- GC can trace types through vregs to find roots

**For ARM64:**
- Must maintain compatible stack layout
- Must ensure vreg→stack mapping works with GC
- May need platform-specific GC root enumeration

**Action Items:**
1. Read src/gc.c to understand root tracing
2. Find where x86 JIT cooperates with GC
3. Ensure ARM64 frame layout is GC-compatible
4. Test with `hl_gc_profile` and stress tests

**Deliverable:** GC can safely collect in ARM64-JITted code

---

### 6.3 Exception Handling Integration

**Opcodes involved:**
- `OThrow` - throw exception
- `OTry` - set up exception handler
- `OCatch` - catch exception

**Current x86 implementation uses:**
```c
// jit.c references to exception handling
setjmp/longjmp-style unwinding (potentially)
Stack unwinding with frame info
```

**ARM64 needs:**
- Compatible stack frames for unwinding
- Proper FP chain (X29/FP must be maintained)
- Ensure exception handler can restore state

**Study:**
- How OTry sets up handler
- How OThrow unwinds stack
- Whether HL uses C++ exceptions or custom unwinding

**Action Items:**
1. Grep for OThrow/OTry/OCatch in jit.c
2. Understand unwinding mechanism
3. Port to ARM64 with proper FP chain

**Deliverable:** Exceptions work on ARM64

---

### 6.4 Validation: Encoding Tests (ChatGPT's Suggestion)

**Create unit tests comparing your encodings vs clang:**

```c
// test_arm64_encoding.c
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

// Test helper
void test_encoding(const char *name, uint32_t expected, uint32_t actual) {
    if (expected != actual) {
        printf("FAIL: %s\n  Expected: %08x\n  Got:      %08x\n",
               name, expected, actual);
        assert(0);
    }
    printf("PASS: %s\n", name);
}

// Generate reference with clang
void reference_code() {
    // Compile with: clang -target aarch64-linux-gnu -S -o ref.s ref.c
    // Then objdump -d to see encodings

    register int64_t x0 asm("x0");
    register int64_t x1 asm("x1");
    register int64_t x2 asm("x2");

    x0 = x1 + x2;  // ADD X0, X1, X2
    x0 = x1 - x2;  // SUB X0, X1, X2
    x0 = x1 * x2;  // MUL (MADD with XZR)
}

int main() {
    // ADD X0, X1, X2 should encode to: 0x8B020020
    test_encoding("ADD X0, X1, X2", 0x8B020020,
                  arm64_encode_add(0, 1, 2));

    // SUB X0, X1, X2 should encode to: 0xCB020020
    test_encoding("SUB X0, X1, X2", 0xCB020020,
                  arm64_encode_sub(0, 1, 2));

    // ... more tests
    return 0;
}
```

**Deliverable:** All instruction encodings validated

---

### 6.5 Map HashLink Opcodes to ARM64

Create ARM64 versions of the existing opcode handlers. This is where we replace x86 code generation with ARM64.

**Example for OMov (move between registers):**

```c
// Current x86 version (simplified)
case OMov:
    op(ctx, MOV, dst_preg, src_preg, false);
    break;

// New ARM64 version
#ifdef HL_ARM64
case OMov:
    arm64_mov_reg(ctx, dst->id, src->id);
    break;
#endif
```

**Example for OAdd (addition):**

```c
#ifdef HL_ARM64
case OAdd:
    if (is_int_type(dst->t)) {
        arm64_add_reg(ctx, dst->id, src1->id, src2->id);
    } else if (is_float_type(dst->t)) {
        arm64_fadd_d(ctx, dst->id, src1->id, src2->id);
    }
    break;
#endif
```

**All opcodes from opcodes.h must be mapped:**
- Arithmetic: OAdd, OSub, OMul, OSDiv, OUDiv, OSMod, OUMod
- Bitwise: OAnd, OOr, OXor, OShl, OSShr, OUShr
- Comparison/Jump: OJTrue, OJFalse, OJEq, OJNotEq, OJSLt, OJSGte, etc.
- Calls: OCall0-4, OCallN, OCallMethod, OCallClosure
- Memory: OField, OSetField, OGetGlobal, OSetGlobal
- Type conversions: OToInt, OToFloat, OToDyn, etc.

**Deliverable:** All HashLink opcodes have ARM64 implementations

---

### 6.2 Function Prologue/Epilogue

```c
#ifdef HL_ARM64

static void arm64_function_prologue(jit_ctx *ctx, hl_function *f) {
    // Calculate stack frame size
    int stack_size = calculate_stack_frame_size(f);

    // Align to 16 bytes (AAPCS64 requirement)
    stack_size = (stack_size + 15) & ~15;

    // Save FP and LR
    arm64_push_pair(ctx, FP, LR);

    // Set up frame pointer
    arm64_mov_reg(ctx, FP, SP);

    // Allocate stack space
    if (stack_size > 0)
        arm64_sub_sp(ctx, stack_size);

    // Save callee-saved registers if used
    // (determined by register allocator)
}

static void arm64_function_epilogue(jit_ctx *ctx, hl_function *f) {
    int stack_size = calculate_stack_frame_size(f);
    stack_size = (stack_size + 15) & ~15;

    // Restore stack pointer
    if (stack_size > 0)
        arm64_add_sp(ctx, stack_size);

    // Restore FP and LR, and return
    arm64_pop_pair(ctx, FP, LR);
    arm64_ret(ctx);
}

#endif // HL_ARM64
```

---

### 6.3 Update hl_jit_function()

The main compilation loop needs ARM64 support:

```c
int hl_jit_function( jit_ctx *ctx, hl_module *m, hl_function *f ) {
    // ... existing setup code ...

#ifdef HL_ARM64
    arm64_function_prologue(ctx, f);
#else
    // x86 prologue (existing code)
#endif

    // Opcode loop
    for (opCount = 0; opCount < f->nops; opCount++) {
        hl_opcode *op = f->ops + opCount;

        switch(op->op) {
#ifdef HL_ARM64
            case OMov:
                // ARM64 implementation
                break;
            case OAdd:
                // ARM64 implementation
                break;
            // ... all other opcodes ...
#else
            // x86 implementations (existing code)
#endif
        }
    }

#ifdef HL_ARM64
    arm64_function_epilogue(ctx, f);
#else
    // x86 epilogue (existing code)
#endif

    return codePos;
}
```

**Deliverable:** Complete integration into hl_jit_function

---

## PHASE 7: TESTING (Days 26-30)

### 7.1 Test Infrastructure

Create minimal test programs:

```haxe
// test1.hx - Empty main
class Test1 {
    static function main() {
    }
}

// test2.hx - Simple arithmetic
class Test2 {
    static function main() {
        var x = 5 + 3;
        trace(x);
    }
}

// test3.hx - Function call
class Test3 {
    static function add(a:Int, b:Int):Int {
        return a + b;
    }
    static function main() {
        var result = add(10, 20);
        trace(result);
    }
}

// test4.hx - Loop
class Test4 {
    static function main() {
        var sum = 0;
        for(i in 0...10) {
            sum += i;
        }
        trace(sum);
    }
}

// test5.hx - Floating point
class Test5 {
    static function main() {
        var x = 3.14 * 2.0;
        trace(x);
    }
}
```

Compile to .hl:
```bash
haxe -hl test1.hl -main Test1
haxe -hl test2.hl -main Test2
# etc.
```

Run on ARM64:
```bash
./hl test1.hl
./hl test2.hl
# etc.
```

---

### 7.2 Testing Workflow

**For each test:**

1. **Compile test to .hl**
2. **Disassemble generated ARM64 code** (use debugger or add debug output)
3. **Run and verify output**
4. **Debug failures:**
   - Add logging to JIT compilation
   - Check instruction encoding
   - Verify register allocation
   - Check stack alignment

**Progressive testing:**
- ✅ Empty function (tests prologue/epilogue)
- ✅ Simple arithmetic (tests basic ops)
- ✅ Branches (tests control flow)
- ✅ Function calls (tests AAPCS64)
- ✅ Loops (tests jumps, counters)
- ✅ Floating point (tests FP ops)
- ✅ Full test suite (HashLink's existing tests)

---

### 7.3 Debug Helpers

Add debug output to JIT:

```c
#ifdef JIT_DEBUG
static void arm64_dump_instruction(uint32_t insn) {
    printf("  [%08x] ", insn);
    // Decode and print instruction
}
#endif
```

---

## PHASE 8: BUILD SYSTEM (Days 31-32)

### 8.1 Update CMakeLists.txt

```cmake
# Remove ARM64 restriction
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(WITH_VM_DEFAULT ON)  # Enable for ARM64
        message(STATUS "ARM64 detected - JIT enabled")
    else()
        set(WITH_VM_DEFAULT OFF)  # Keep ARM32 disabled for now
        message(STATUS "ARM32 detected - JIT not yet supported")
    endif()
endif()
```

---

### 8.2 Update Makefile

```makefile
# Update lines 226-230
all: libhl libs
ifeq ($(ARCH),arm64)
	$(info Building HashLink JIT for ARM64)
	all: hl
else ifeq ($(ARCH),aarch64)
	$(info Building HashLink JIT for ARM64)
	all: hl
else
	all: hl
endif
```

---

## TESTING TARGETS

### Primary Targets:

1. **Raspberry Pi 4/5 (ARM64 Linux)**
   - OS: Raspberry Pi OS 64-bit
   - Test full functionality

2. **Android Device (ARM64)**
   - Cross-compile or use Termux
   - Verify JIT works without restrictions

3. **Apple Silicon Mac (ARM64)**
   - macOS native
   - Should work like x86-64 Mac

### Test Plan:

- Run HashLink's existing test suite (other/tests/)
- Test threading (threads.hl)
- Test performance vs x86-64
- Test stability (long-running programs)

---

## DELIVERABLES CHECKLIST

- [ ] ARM64 register definitions
- [ ] Instruction encoding helpers
- [ ] All arithmetic operations
- [ ] All bitwise operations
- [ ] Load/store operations
- [ ] Stack operations
- [ ] Comparison operations
- [ ] Branch instructions
- [ ] Jump patching
- [ ] Function prologue/epilogue
- [ ] AAPCS64 calling convention
- [ ] Floating-point operations
- [ ] FP conversions
- [ ] All HashLink opcodes mapped
- [ ] Cache flushing
- [ ] Build system updates
- [ ] Test suite passing
- [ ] Documentation

---

## RISK MITIGATION

**Risk 1: Instruction encoding errors**
- Mitigation: Test each instruction individually
- Reference: ARM Architecture Reference Manual

**Risk 2: Calling convention bugs**
- Mitigation: Create isolated test for function calls
- Compare with C compiler output

**Risk 3: Cache coherency issues**
- Mitigation: Always call __builtin___clear_cache
- Test on real hardware

**Risk 4: Stack alignment**
- Mitigation: AAPCS64 requires 16-byte alignment, enforce strictly

**Risk 5: Branch range limits**
- Mitigation: Implement trampolines for long branches if needed

---

## SUCCESS METRICS

✅ **Minimum Success:**
- Compile and run hello.hl on ARM64
- Basic arithmetic works
- Function calls work

✅ **Full Success:**
- All HashLink tests pass
- Performance within 2x of x86-64
- Stable on all target platforms

✅ **Stretch Goals:**
- Performance within 1.5x of x86-64
- Add ARM32 support
- Optimize hot paths

---

## TIMELINE ESTIMATE

**Optimistic:** 3-4 weeks
**Realistic:** 4-6 weeks
**Conservative:** 6-8 weeks

**Phase breakdown:**
- Foundation: 3 days
- Instruction encoding: 5 days
- Memory operations: 3 days
- Control flow: 4 days
- Floating point: 4 days
- Integration: 6 days
- Testing: 5 days
- Build system: 2 days
- **Buffer:** 3-5 days for unexpected issues

---

## ADDRESSING CHATGPT's FEEDBACK

**ChatGPT's Assessment:** "The plan outlines maybe 30-40% of the real work, the rest is making it correct and integrating with HashLink's GC, register allocator, and all opcodes."

**Response:** ✅ **INCORPORATED**

### What We Added Based on ChatGPT's Review:

**1. Phase 0: Safe Refactoring (NEW)**
- Wrap x86 code in conditionals BEFORE adding ARM64
- Verify nothing breaks
- Create clean baseline
- **Can do HERE - no ARM hardware needed**

**2. Register Allocator Deep Dive (Phase 6.1)**
- Explained vreg vs preg reality
- Showed how load()/store()/reg_bind() work
- Corrected oversimplified examples
- Action items to study existing code

**3. GC Stack Maps (Phase 6.2)**
- How HL GC finds roots
- hl_debug_infos structure
- Frame descriptors
- Testing with GC stress tests

**4. Exception Handling (Phase 6.3)**
- OThrow/OTry/OCatch opcodes
- Stack unwinding requirements
- FP chain maintenance

**5. Encoding Validation (Phase 6.4)**
- Unit tests vs clang output
- Methodology for validation
- Concrete test examples

**6. Environment Clarity**
- **Can work HERE:** Phases 0-2 (refactoring, encoders, cross-compile)
- **Need ARM hardware:** Phases 3+ (execution, GC testing, debugging)

### Timeline Update (More Realistic):

ChatGPT's point: "4-6 weeks is optimistic"

**Updated estimate:**
- **Phase 0-2 (here):** 1-2 weeks
- **Get ARM hardware:** Week 3
- **Phase 3-6 (with hardware):** 3-4 weeks
- **Phase 7 (testing & debugging):** 1-2 weeks
- **Total: 5-8 weeks** (more realistic than 4-6)

### The "90% Done" Syndrome

ChatGPT warned: "You get tests like add, loops, trace working. Then you hit weird GC or exception bugs that are painful to debug."

**Mitigation:**
- Start GC testing EARLY (Phase 4, not Phase 7)
- Test exceptions EARLY
- Don't wait for "full implementation" to stress-test
- Incremental integration prevents big-bang surprises

### What's Still Missing (Acknowledged):

**Complex opcodes we haven't detailed:**
- `OCallClosure` - closure calling convention
- `OStaticClosure`, `OInstanceClosure`, `OVirtualClosure`
- `ODynGet`, `ODynSet` - dynamic field access
- `OToSFloat`, `OToDyn`, `OToVirtual` - type conversions
- `OSwitch` - switch table generation
- `ONop` through all others in opcodes.h

**Strategy:**
- Phase 2: Get simple ops working (mov, add, cmp, branch)
- Phase 4: Study x86 implementation of complex ops
- Phase 6: Port complex ops one at a time with tests

### Honest Assessment:

**Feasibility:** ✅ YES - absolutely doable
**Complexity:** ⚠️ HIGH - not trivial
**Timeline:** 📅 5-8 weeks realistic (not 2-3)
**Risk:** 🎯 Manageable with incremental approach

**The plan is now:**
- ✅ 30% instruction encoding (detailed)
- ✅ 40% integration complexity (now addressed)
- ✅ 20% testing & debugging (phased approach)
- ✅ 10% unexpected issues (buffered)

---

## NEXT STEPS

**Immediate actions:**
1. **START HERE:** Begin Phase 0 (no ARM hardware needed)
2. Refactor jit.c to dual-backend structure
3. Verify x86 still works
4. Add ARM64 stubs
5. Commit incremental changes

**Week 2-3:**
- Implement ARM64 instruction encoders
- Cross-compile to catch build errors
- Create encoding validation tests
- **Order Raspberry Pi or set up ARM dev environment**

**Week 3+:**
- Port to ARM hardware
- Iterate: code here, test there
- Debug GC and exception issues
- Expand opcode coverage

**Ready to begin Phase 0?** We can start refactoring RIGHT NOW.
