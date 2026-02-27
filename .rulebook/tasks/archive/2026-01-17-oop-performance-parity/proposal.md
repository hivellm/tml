# OOP Performance Parity with C++

## Problem Statement

TML's OOP implementation is currently 7-64x slower than equivalent C++ code due to:
1. **Heap allocation by default** - C++ allocates objects on stack by default
2. **Insufficient devirtualization** - C++ devirtualizes when type is known at compile time
3. **Missing SROA (Scalar Replacement of Aggregates)** - C++ breaks down structs into scalar values
4. **Incomplete inlining** - C++ aggressively inlines small methods

## Benchmark Comparison (TML vs C++ with -O3)

| Benchmark | TML | C++ | Ratio | Root Cause |
|-----------|-----|-----|-------|------------|
| Object Creation | 10,554 µs | 164 µs | **64x slower** | Heap allocation |
| HTTP Handler | 567 µs | 18 µs | **31x slower** | Virtual dispatch + heap |
| Virtual Dispatch | 653 µs | 58 µs | **11x slower** | No devirtualization |
| Deep Inheritance | 279 µs | 37 µs | **7.5x slower** | Chain of super calls |
| Game Loop | 7,563 µs | 1,022 µs | **7x slower** | Heap + sqrt overhead |
| Method Chaining | 2,326 µs | ~0 µs | **∞ slower** | C++ eliminated entirely |

## Proposed Solution

### Phase 1: Stack Promotion for Non-Escaping Objects
- Implement escape analysis to identify objects that don't escape function scope
- Promote heap allocations to stack allocations (alloca) for non-escaping objects
- Add SROA pass to break down stack objects into scalar values

### Phase 2: Aggressive Devirtualization
- Devirtualize calls when concrete type is known at compile time
- Specialize virtual calls for common types
- Inline devirtualized methods aggressively

### Phase 3: Method Chaining Optimization
- Detect builder patterns and optimize away intermediate objects
- Apply copy elision for returned-by-value objects
- Enable return value optimization (RVO/NRVO)

### Phase 4: Constructor/Destructor Optimization
- Fuse chained constructors into single initialization
- Batch destructor calls for multiple objects
- Eliminate unnecessary zeroing of memory

## Expected Outcomes

| Benchmark | Current | Target | Improvement |
|-----------|---------|--------|-------------|
| Object Creation | 64x slower | 2-3x slower | 20-30x faster |
| HTTP Handler | 31x slower | 2-3x slower | 10-15x faster |
| Virtual Dispatch | 11x slower | 1-2x slower | 5-10x faster |
| Method Chaining | ∞ slower | 1-2x slower | Finite |

## Technical Approach

### Stack Promotion
```
BEFORE (heap allocation):
  %obj = call ptr @tml_alloc(i64 16)
  call void @Circle_init(ptr %obj, double 5.0)
  %area = call double @Circle_area(ptr %obj)
  call void @tml_free(ptr %obj)

AFTER (stack promotion):
  %obj = alloca %Circle
  store double 5.0, ptr %obj  ; direct field init
  %area = fmul double 3.14159, 25.0  ; inlined + constant folded
```

### Devirtualization
```
BEFORE (virtual dispatch):
  %vtable = load ptr, ptr %obj
  %method = getelementptr ptr, ptr %vtable, i32 0
  %fn = load ptr, ptr %method
  %result = call double %fn(ptr %obj)

AFTER (devirtualized + inlined):
  %radius = load double, ptr %obj
  %r2 = fmul double %radius, %radius
  %area = fmul double 3.14159, %r2
```

## Dependencies

- MIR escape analysis pass (exists, needs enhancement)
- MIR devirtualization pass (exists, needs enhancement)
- LLVM SROA pass (need to enable)
- LLVM aggressive inlining (need to tune thresholds)

## Risk Assessment

- **Low risk**: Enabling LLVM passes (SROA, inlining)
- **Medium risk**: Enhancing escape analysis
- **Medium risk**: Stack promotion in codegen
- **High risk**: Changing default allocation strategy (may break existing code)

## Success Criteria

1. Object Creation benchmark < 500 µs (20x improvement)
2. Method Chaining benchmark < 100 µs (measurable)
3. All existing tests pass
4. No increase in memory usage for long-running programs
