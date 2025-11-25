# Plan: Finding the ARM64 JIT Pointer Truncation Bug

## The Problem

Every test crashes with the same symptom: `x0=0xf5c433ed` (truncated 64-bit pointer) passed to native functions. We've been guessing at causes for 10+ commits with no progress.

## Why We're Stuck

1. We don't know WHICH JIT-generated instruction produces the truncated value
2. We don't know WHICH bytecode opcode is responsible
3. We don't know if it's a store bug, load bug, or register width bug
4. We keep making speculative fixes without evidence

## Radical New Approach: Binary Search the Problem

### Phase 1: Isolate the Simplest Crashing Case

Create the absolute minimal Haxe program that crashes:

```haxe
class Test {
    static function main() {
        trace("hello");
    }
}
```

If this crashes, we have ~50 bytecode ops to examine.
If it works, add complexity until it crashes - then we know exactly what triggers it.

### Phase 2: Dump Everything at Crash Point

Modify the native function that crashes (`hl_hbset_impl`) to dump its inputs BEFORE dereferencing:

```c
void hl_hbset_impl(...) {
    fprintf(stderr, "hbset called with: %p\n", arg0);
    if ((uint64_t)arg0 < 0x100000000ULL) {
        fprintf(stderr, "TRUNCATED POINTER DETECTED!\n");
        abort(); // Get clean stack trace
    }
    // ... rest of function
}
```

This tells us the exact call site.

### Phase 3: Trace Backwards from Crash

Once we know which JIT code calls the crashing native function:

1. Disassemble the JIT-generated code around that call
2. Find which instruction loaded X0
3. Trace back to where that value was stored

### Phase 4: Instrument JIT Code Generation

Add a debug mode that logs EVERY load/store the JIT generates:

```
[JIT F23 op5] STORE X10 -> [X29-24] size=3 (64-bit)
[JIT F23 op7] LOAD [X29-24] -> X0 size=3 (64-bit)
[JIT F23 op8] CALL native hl_hbset
```

If we see `size=2` (32-bit) anywhere a pointer should be, that's our bug.

### Phase 5: Compare with x86 JIT

For the exact bytecode sequence that crashes:

1. Print what the x86 JIT generates (on x86 machine or via code inspection)
2. Print what ARM64 JIT generates
3. Diff them - the bug is in the difference

## Concrete First Steps

1. **Create minimal test case** - Start with `trace("hello")` and see if it crashes
2. **Add truncation detector** - Modify libhl.so to catch truncated pointers at entry to native functions
3. **Get exact crash location** - Which function, which bytecode op, which register

## What NOT To Do

- Do NOT make more speculative fixes
- Do NOT change code without knowing why
- Do NOT assume we know where the bug is

## Success Criteria

We'll know we've found it when we can say:

> "Bytecode opcode X in function Y generates ARM64 instruction Z which uses W0 instead of X0 (or similar concrete bug)"

## Current State (2024-11-25)

- Crash: `hl_hbset_impl` receives `x0=0xf5c433ed` (clearly truncated from `0x7ffff5c433ed`)
- Backtrace shows: JIT code -> call_jit_c2hl -> hl_hbset_impl
- The truncation happens somewhere in JIT-generated code before the native call

## Bugs Fixed Along the Way (but didn't fix the crash)

1. OSetThis was using `o->p2` instead of `o->p1` for field_index
2. OField/OSetField now correctly use `rt->fields_indexes[field_index]`

These were real bugs but not the root cause of the pointer truncation.
