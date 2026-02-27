# LLVM IR Optimization Blockers

## Problem Statement

Deep analysis of generated LLVM IR reveals TML is 10-30x slower than Rust and C++ due to **IR patterns that block LLVM's optimization passes**, not lack of optimizations.

### Benchmark Comparison (100K iterations)

| Benchmark | TML | Rust | C++ | TML vs C++ |
|-----------|-----|------|-----|------------|
| Virtual Dispatch | 804 µs | 286 µs | 63 µs | **12.7x slower** |
| Object Creation | 1175 µs | 0 µs (DCE) | 177 µs | **6.6x slower** |
| HTTP Handler | 592 µs | 150 µs | 20 µs | **29.6x slower** |
| Game Loop | 1912 µs | 381 µs | 1090 µs | **1.8x slower** |
| Deep Inheritance | 310 µs | 0 µs (DCE) | 40 µs | **7.8x slower** |
| Method Chaining | 2377 µs | 0 µs (DCE) | 0 µs (DCE) | **∞ slower** |

## Root Cause Analysis

### 1. Phi Nodes with Aggregate Types (CRITICAL)

**TML generates**:
```llvm
loop.header1:
    %v15 = phi %struct.Rectangle [ %v6, %entry0 ], [ %v15, %if.merge9 ]
    %v17 = phi %struct.Circle [ %v3, %entry0 ], [ %v17, %if.merge9 ]
    %v18 = phi %struct.Triangle [ %v11, %entry0 ], [ %v18, %if.merge9 ]
```

**Rust generates**:
```llvm
bb5.i.i12.i:
    %data_ptr = phi ptr [ %rectangle, %bb3 ], [ %circle, %bb4 ], [ %triangle, %default ]
    %vtable_ptr = phi ptr [ @vtable.2, %bb3 ], [ @vtable.1, %bb4 ], [ @vtable.3, %default ]
```

**Impact**: LLVM's SROA (Scalar Replacement of Aggregates) cannot break down structs that flow through phi nodes as aggregate values. This blocks constant propagation, dead code elimination, and register promotion.

### 2. Objects Passed by Value Instead of Pointer

**TML generates**:
```llvm
define double @Circle__area(%struct.Circle %this) {
    %radius = extractvalue %struct.Circle %this, 1
    ...
}
```

**Rust generates**:
```llvm
define double @Circle_area(ptr %self) {
    %radius = load double, ptr %self
    ...
}
```

**Impact**: Passing structs by value copies entire struct contents in SSA. Pointer-based access allows SROA to work and enables getelementptr optimizations.

### 3. Drop Calls for Trivial Types

**TML generates**:
```llvm
call void @drop_Circle(%struct.Circle %v3)  ; 16-byte struct passed by value!

define void @drop_Circle(%struct.Circle %v) alwaysinline {
entry:
    ret void  ; Empty!
}
```

**Rust**: No drop call emitted for POD types.

**Impact**: Even with empty drops marked `alwaysinline`, passing full structs has ABI overhead and bloats IR.

### 4. Complex CFG with Continuation Blocks

**TML generates**:
```
entry0 → entry0_cont → entry0_cont_cont → entry0_cont_cont_cont → loop.header
```

**Rust/C++ generate**:
```
entry → loop.header → loop.body → loop.latch → exit
```

**Impact**: Complex CFG prevents loop optimizations (LICM, unrolling, vectorization) and increases compile time.

### 5. extractvalue in Hot Loops

**TML**:
```llvm
loop.header1:
    %v17 = phi %struct.Circle [ %v3, %entry0 ], [ %v17, %if.merge9 ]
    ...
if.then7:
    %v60 = extractvalue %struct.Circle %v17, 1   ; Extract in every iteration!
    %v61 = fmul double %v60, %v59
```

**Rust**:
```llvm
; Object stored in alloca once, accessed via getelementptr
%circle_ptr = alloca %struct.Circle
store double 5.0, ptr %circle_ptr
...
%radius = load double, ptr %circle_ptr  ; SROA can hoist this
```

## Proposed Solution

### Phase 1: Pointer-Based Object Representation

Store class instances in stack allocations (alloca) and pass pointers instead of values:

```llvm
; BEFORE (value semantics)
%v3 = insertvalue %struct.Circle undef, i32 1, 0
%v4 = insertvalue %struct.Circle %v3, double 5.0, 1
%v17 = phi %struct.Circle [ %v4, %entry0 ], [ %v17, %loop ]

; AFTER (pointer semantics)
%circle_ptr = alloca %struct.Circle
store i32 1, ptr %circle_ptr
%radius_ptr = getelementptr %struct.Circle, ptr %circle_ptr, i32 0, i32 1
store double 5.0, ptr %radius_ptr
; No phi needed - object lives at fixed address
```

### Phase 2: Trivial Drop Elimination

Detect types with no resources (only primitives, no heap allocations):
- Skip drop call emission entirely for trivially destructible types
- Mark types during type checking with `is_trivially_destructible` flag

### Phase 3: Standard Loop Form

Generate canonical loop structure:
```
preheader:
    ; Object allocations and initialization
    br label %loop.header

loop.header:
    %i = phi i32 [ 0, %preheader ], [ %i.next, %loop.latch ]
    %cond = icmp slt i32 %i, %n
    br i1 %cond, label %loop.body, label %exit

loop.body:
    ; Loop body
    br label %loop.latch

loop.latch:
    %i.next = add i32 %i, 1
    br label %loop.header

exit:
    ret
```

### Phase 4: Lifetime Intrinsics

Add LLVM lifetime markers for stack allocations:
```llvm
call void @llvm.lifetime.start.p0(i64 16, ptr %circle_ptr)
; ... use circle ...
call void @llvm.lifetime.end.p0(i64 16, ptr %circle_ptr)
```

## Expected Outcomes

| Benchmark | Current | After Fix | Target (Rust parity) |
|-----------|---------|-----------|----------------------|
| Virtual Dispatch | 804 µs | ~300 µs | 286 µs |
| Object Creation | 1175 µs | ~100 µs | 0 µs (DCE) |
| HTTP Handler | 592 µs | ~150 µs | 150 µs |
| Method Chaining | 2377 µs | ~50 µs | 0 µs (DCE) |

## Technical Complexity

- **Phase 1** (Pointer representation): HIGH - Requires changes to MIR builder and codegen
- **Phase 2** (Trivial drops): LOW - Add flag during type checking, check in codegen
- **Phase 3** (Loop form): MEDIUM - Refactor loop codegen in MIR builder
- **Phase 4** (Lifetime): LOW - Add intrinsic calls in codegen

## Files Affected

### MIR
- `compiler/src/mir/mir_builder.cpp` - Object representation
- `compiler/include/mir/mir.hpp` - New instruction types

### Codegen
- `compiler/src/codegen/llvm_ir_gen.cpp` - IR generation patterns
- `compiler/src/codegen/core/class_codegen.cpp` - Class representation
- `compiler/src/codegen/core/drop.cpp` - Drop emission

### Type System
- `compiler/src/types/env_lookups.cpp` - Trivially destructible detection
- `compiler/include/types/env.hpp` - Type flags

## Success Criteria

1. Virtual Dispatch benchmark within 2x of Rust (< 600 µs)
2. Method Chaining benchmark becomes measurable (< 100 µs)
3. LLVM's SROA pass successfully breaks down stack objects
4. All 1577+ existing tests pass
5. No increase in compile time > 10%
