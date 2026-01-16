# TML OOP Benchmark Results

Generated: 2026-01-16 (Updated with sealed class optimizations)

## System Configuration
- Platform: Windows
- Compiler: TML with LLVM backend
- Value Class Optimization: Enabled for sealed classes

## Benchmark Description

Each benchmark runs 100,000 iterations after a 10,000 iteration warmup phase.

### Benchmarks

1. **Virtual Dispatch** - Tests polymorphic method calls on Shape hierarchy (Circle, Rectangle, Triangle)
2. **Object Creation** - Tests creating sealed Point objects and computing distances
3. **HTTP Handler** - Simulates HTTP request routing with different handler types
4. **Game Loop** - Simulates game entity updates (Player, Enemy, Projectile)
5. **Deep Inheritance** - Tests method calls through 5-level inheritance chain
6. **Method Chaining** - Tests builder pattern with sealed Builder class

## Results Summary (100,000 iterations)

### With Sealed Class Optimizations (Stack Allocation)

| Benchmark | Time | Per-Call |
|-----------|------|----------|
| Virtual Dispatch | 670 us | 6.7 ns |
| Object Creation | 10,559 us | 105.6 ns |
| HTTP Handler | 569 us | 5.7 ns |
| Game Loop | 7,516 us | 25.1 ns |
| Deep Inheritance | 280 us | 2.8 ns |
| Method Chaining | 2,294 us | 4.6 ns/step |

### Before vs After Sealed Class Optimization

| Benchmark | Before (heap) | After (stack) | Improvement |
|-----------|---------------|---------------|-------------|
| Object Creation | 12,660 us | 10,559 us | **17% faster** |
| Method Chaining | 12,359 us | 2,294 us | **81% faster (5.4x)** |

## Performance Analysis

### Key Improvements

1. **Sealed Class Value Semantics**: Classes marked `sealed` with no virtual methods are automatically treated as value types
2. **Stack Allocation**: Value classes use `alloca` instead of `malloc`, eliminating heap allocation overhead
3. **No Vtable**: Sealed value classes don't need vtable pointers, reducing memory footprint

### Per-Call Timings (Optimized)

| Benchmark | Time per Call | Notes |
|-----------|---------------|-------|
| Virtual Dispatch | 6.7 ns | Polymorphic Shape calls |
| Object Creation | 105.6 ns | Includes sqrt computation |
| HTTP Handler | 5.7 ns | Handler dispatch |
| Game Loop | 25.1 ns/entity | 3 entities updated |
| Deep Inheritance | 2.8 ns | Devirtualized |
| Method Chaining | 4.6 ns/step | Stack-allocated Builder |

### Fastest Operations
1. **Deep Inheritance** (~280 us) - Devirtualization pass optimizes inheritance chains
2. **Method Chaining** (~2,294 us) - Sealed Builder enables stack allocation
3. **HTTP Handler** (~569 us) - Handler dispatch is efficient

## Technical Details

### What Changed

The compiler now properly checks `is_value_class_candidate()` when generating struct literals for class instances. Sealed classes that meet these criteria:

1. Marked with `sealed` keyword
2. No virtual methods
3. No subclasses possible
4. Base class (if any) is also a value class candidate

Are automatically treated as value types and:
- Use `alloca` instead of `malloc`
- Don't store vtable pointers
- Have no heap allocation overhead

### Code Example

```tml
// Before: heap-allocated (malloc)
class Point {
    x: F64
    y: F64
}

// After: stack-allocated (alloca)
sealed class Point {
    x: F64
    y: F64
}
```

## Conclusion

TML's OOP implementation shows excellent performance with sealed class optimizations:

- **Virtual dispatch**: 6.7 ns (competitive with C++)
- **Deep inheritance**: 2.8 ns (effective devirtualization)
- **Sealed classes**: 5.4x speedup for builder pattern

The `sealed` keyword is now a powerful optimization hint that enables stack allocation for class instances.
