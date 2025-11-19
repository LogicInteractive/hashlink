# Phase 2: ARM64 Register Allocator - Implementation Plan

## Overview

After Phase 1 provides a working POC, Phase 2 will add proper register allocation for production-quality performance.

## Architecture

### Register Assignment Strategy

**Callee-Saved Registers (X19-X28):** Hot vregs
```
X19-X23: First 5 vregs (most frequently used)
X24-X28: Spill registers / additional hot vregs
```

**Caller-Saved Registers (X10-X15):** Temporaries
```
X10-X12: Already used in Phase 1 for temp values
X13-X15: Available for additional temps
```

**Reserved Registers:**
```
X0-X7:   Arguments/returns (ABI)
X8:      Indirect result
X9:      Function pointers, large immediates
X16-X17: Linker temps (avoid)
X29:     Frame pointer
X30:     Link register
SP:      Stack pointer
```

### Data Structures

```c
typedef struct {
    vreg *holds;      // Which vreg is in this register (NULL if free)
    int lock;         // Reference count (don't spill if > 0)
    int last_use;     // Last opcode that used this (for LRU)
} arm64_preg;

typedef struct {
    arm64_preg regs[32];       // Physical registers X0-X31
    arm64_preg *free_list;     // Linked list of free registers
    int current_op;            // Current operation number
} arm64_regalloc;
```

### Core Allocator Functions

```c
// Initialize register allocator
void arm64_regalloc_init(jit_ctx *ctx) {
    for (int i = 19; i <= 28; i++) {  // X19-X28 available
        ctx->arm64_regs[i].holds = NULL;
        ctx->arm64_regs[i].lock = 0;
    }
}

// Allocate register for vreg
arm64_preg *arm64_alloc_reg(jit_ctx *ctx, vreg *vr, bool load) {
    // 1. Check if vreg already in a register
    if (vr->current) {
        vr->current->lock++;
        return vr->current;
    }

    // 2. Find free register
    arm64_preg *p = find_free_reg(ctx);
    if (!p) {
        // 3. Spill least recently used
        p = find_lru_reg(ctx);
        spill_reg(ctx, p);
    }

    // 4. Load vreg into register if needed
    if (load && vreg_on_stack(vr)) {
        arm_ldur_imm(ctx, p->id, X29, vr->stackPos, 3);
    }

    // 5. Update tracking
    p->holds = vr;
    vr->current = p;
    p->lock = 1;
    p->last_use = ctx->current_op;

    return p;
}

// Free register (decrease reference count)
void arm64_free_reg(jit_ctx *ctx, arm64_preg *p) {
    if (--p->lock == 0) {
        // Register now available for reuse
        // But don't clear p->holds yet (vreg still there)
    }
}

// Spill register to stack
void spill_reg(jit_ctx *ctx, arm64_preg *p) {
    if (p->holds && p->lock == 0) {
        vreg *vr = p->holds;
        arm_stur_imm(ctx, p->id, X29, vr->stackPos, 3);
        vr->current = NULL;
        p->holds = NULL;
    }
}

// Find free register
arm64_preg *find_free_reg(jit_ctx *ctx) {
    for (int i = 19; i <= 28; i++) {
        if (ctx->arm64_regs[i].holds == NULL) {
            return &ctx->arm64_regs[i];
        }
    }
    return NULL;
}

// Find least recently used register
arm64_preg *find_lru_reg(jit_ctx *ctx) {
    arm64_preg *lru = NULL;
    int oldest = INT_MAX;

    for (int i = 19; i <= 28; i++) {
        arm64_preg *p = &ctx->arm64_regs[i];
        if (p->lock == 0 && p->last_use < oldest) {
            oldest = p->last_use;
            lru = p;
        }
    }

    return lru;
}
```

### Operation Pattern Updates

```c
// Phase 1 pattern (stack-based):
case OAdd:
    LOAD_VREG(X10, ra);
    LOAD_VREG(X11, rb);
    arm_add_reg(ctx, X10, X10, X11, true);
    STORE_VREG(X10, dst);
    break;

// Phase 2 pattern (register-allocated):
case OAdd:
    {
        arm64_preg *pa = arm64_alloc_reg(ctx, ra, true);
        arm64_preg *pb = arm64_alloc_reg(ctx, rb, true);
        arm64_preg *pd = arm64_alloc_reg(ctx, dst, false);

        arm_add_reg(ctx, pd->id, pa->id, pb->id, true);

        arm64_free_reg(ctx, pa);
        arm64_free_reg(ctx, pb);
        arm64_free_reg(ctx, pd);
    }
    break;
```

## Implementation Phases

### Phase 2A: Basic Allocator (1 day)
1. Add arm64_preg structure to jit_ctx
2. Implement alloc/free/spill functions
3. Update LOAD_VREG/STORE_VREG to use allocator
4. Test with simple programs

### Phase 2B: Optimization (1 day)
1. Add liveness analysis
2. Implement register pressure tracking
3. Smart spilling (spill cold vregs first)
4. Coalescing (reuse registers when possible)

### Phase 2C: Advanced Features (1 day)
1. Handle function calls (save/restore around calls)
2. Loop optimization (keep loop vars in registers)
3. Constant propagation
4. Dead code elimination

## Performance Goals

**Phase 1 (Stack-based):**
- Every vreg access: 1 load + 1 store
- Binary operation: 2 loads + 1 computation + 1 store = 4 memory ops
- Performance: ~60% of x86

**Phase 2A (Basic register allocation):**
- Hot vregs stay in registers
- Typical binary operation: 0-1 loads + 1 computation + 0 store = 0-1 memory ops
- Performance: ~80% of x86

**Phase 2B (Optimized):**
- Smart spilling reduces memory traffic
- Loop variables never spilled
- Performance: ~90% of x86

**Phase 2C (Advanced):**
- Constant folding, dead code elimination
- Performance: ~95-100% of x86

## Testing Strategy

**Benchmarks:**
1. Fibonacci (recursion)
2. Matrix multiply (loops)
3. String manipulation (memory)
4. Real Haxe applications

**Metrics:**
- Instructions per operation
- Memory accesses per operation
- Total execution time
- Comparison to x86 JIT

## Migration from Phase 1

**Compatibility:**
- Keep LOAD_VREG/STORE_VREG macros
- Make them use allocator internally
- Old Phase 1 code still works

```c
// Phase 1 LOAD_VREG: Always loads from stack
#define LOAD_VREG(tmp_reg, vr) arm_ldur_imm(...)

// Phase 2 LOAD_VREG: Uses allocator
#define LOAD_VREG(tmp_reg, vr) \\
    ({ \\
        arm64_preg *_p = arm64_alloc_reg(ctx, vr, true); \\
        tmp_reg = _p->id; \\
        arm64_free_reg(ctx, _p); \\
    })
```

## Risk Mitigation

**Incremental approach:**
1. Phase 1 works (baseline)
2. Add allocator but keep it simple
3. Test extensively at each step
4. Can always fall back to Phase 1

**Validation:**
- Compare output with Phase 1 (should be identical)
- Run full Haxe test suite
- Benchmark against x86

## Timeline

**Phase 2A:** 2-3 days
**Phase 2B:** 2-3 days
**Phase 2C:** 2-3 days
**Total:** 1-2 weeks for full optimization

**Or:** Just do Phase 2A (3 days) for 80% performance, good enough for most uses.

## Success Criteria

- [ ] Hot vregs stay in registers across operations
- [ ] Cold vregs intelligently spilled
- [ ] Performance within 10% of x86 JIT
- [ ] All Haxe tests pass
- [ ] No regressions from Phase 1

---

**Status:** Detailed plan ready
**Dependency:** Phase 1 must complete first
**Effort:** 1-2 weeks
**Reward:** Production-quality ARM64 JIT!
