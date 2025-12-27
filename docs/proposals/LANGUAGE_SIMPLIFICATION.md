# TML Language Simplification Proposal

## Problem

TML code is ~2x more verbose than equivalent Rust/C++ code.

Benchmark comparison:
- TML: 229 lines
- Rust: 103 lines
- C++: 102 lines

## Root Causes

### 1. No Range-Based For Loops (Priority: HIGH)

**Current:** Manual loop with counter
```tml
let mut i: I64 = 0
loop {
    if i >= n { break }
    sum = sum + i
    i = i + 1
}
```

**Proposed:** Range syntax (already in spec, needs implementation)
```tml
for i in 0 to n {
    sum = sum + i
}

// Inclusive range
for i in 1 through n {
    product = product * i
}
```

**Impact:** Reduces 8 lines → 3 lines per loop

### 2. No Compound Assignment Operators (Priority: HIGH)

**Current:**
```tml
sum = sum + i
count = count - 1
product = product * factor
```

**Proposed:**
```tml
sum += i
count -= 1
product *= factor
```

**Operators needed:** `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`

**Impact:** More readable, fewer typos

### 3. No First-Class Functions (Priority: CRITICAL)

This causes the most code duplication.

**Current:** Must duplicate wrapper functions
```tml
func run_benchmark_add(name: Str, arg: I64, runs: I32) {
    let _ = black_box_i64(bench_int_add(arg))
    // ... timing code ...
}

func run_benchmark_mul(name: Str, arg: I64, runs: I32) {
    let _ = black_box_i64(bench_int_mul(arg))  // Only this line differs!
    // ... same timing code ...
}
```

**Proposed:** Function types and closures
```tml
// Function type syntax
type BenchFn = Fn(I64) -> I64

func run_benchmark(name: Str, func: BenchFn, arg: I64, runs: I32) {
    let _ = black_box_i64(func(arg))
    // ... timing code ...
}

func main() -> I32 {
    run_benchmark("int_add", bench_int_add, 1000000, 3)
    run_benchmark("int_mul", bench_int_mul, 100000, 3)
}
```

**Impact:** Eliminates ~100 lines of duplication in benchmark

### 4. No Increment/Decrement Operators (Priority: MEDIUM)

**Current:**
```tml
i = i + 1
count = count - 1
```

**Proposed:** (optional, compound assignment covers most cases)
```tml
i++    // or ++i for pre-increment
count--
```

### 5. No Multiple Variable Declaration (Priority: LOW)

**Current:**
```tml
let a: I64 = 1
let b: I64 = 2
let c: I64 = 3
```

**Proposed:** Tuple destructuring
```tml
let (a, b, c): (I64, I64, I64) = (1, 2, 3)
// or with inference
let (a, b, c) = (1i64, 2i64, 3i64)
```

### 6. Single-Expression If (Priority: LOW)

**Current:**
```tml
if n <= 1 {
    return n
}
```

**Proposed:** Allow single-line form
```tml
if n <= 1 then return n
```

## Implementation Priority

1. **Range for loops** - Already in spec, needs codegen
2. **Compound assignment** - Simple lexer/parser change
3. **First-class functions** - Major feature, critical for reducing duplication
4. **Increment operators** - Nice to have
5. **Tuple destructuring** - Nice to have
6. **Single-line if** - Already supported via `if expr then expr else expr`

## Expected Impact

With proposals 1-3 implemented:
- TML benchmark: ~110-120 lines (from 229)
- Matches Rust/C++ verbosity
- Significantly better developer experience

## Code Comparison After Improvements

### Before (229 lines)
```tml
func bench_int_add(iterations: I64) -> I64 {
    let mut sum: I64 = 0
    let mut i: I64 = 0
    loop {
        if i >= iterations { break }
        sum = sum + i
        i = i + 1
    }
    return sum
}

// Plus 5 duplicate run_benchmark_* functions...
```

### After (~110 lines)
```tml
func bench_int_add(iterations: I64) -> I64 {
    let mut sum: I64 = 0
    for i in 0 to iterations {
        sum += i
    }
    sum  // implicit return
}

func run_benchmark(name: Str, func: Fn(I64) -> I64, arg: I64, runs: I32) {
    // Single generic function handles all benchmarks
}
```
