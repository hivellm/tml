# TML Auto-Parallelization Specification

## 1. Overview

TML Auto-Parallel is a compiler feature that automatically detects and parallelizes suitable code patterns without requiring explicit annotations from the programmer. The goal is to achieve near-optimal parallel performance while maintaining correctness and predictable behavior.

## 2. Design Philosophy

Unlike Bend's pure functional approach with interaction combinators, TML takes a pragmatic imperative approach:

1. **Conservative by default** - Only parallelize when provably safe
2. **Opt-out, not opt-in** - Parallelization happens automatically unless disabled
3. **Bounded resources** - Never exceed available CPU cores
4. **Graceful fallback** - Small workloads run sequentially to avoid overhead

## 3. Parallelizable Patterns

### 3.1 Parallel For Loops

```tml
// Automatically parallelized - no loop-carried dependencies
for i in 0 to 1000000 {
    result[i] = compute(data[i])
}
```

**Detection criteria:**
- Loop variable only used for indexing
- No writes to variables read in other iterations
- No function calls with side effects

### 3.2 Map Operations

```tml
// Parallel map over collection
let results = items.map(do(x) expensive_computation(x))
```

**Detection criteria:**
- Closure is pure (no captured mutable state)
- No I/O operations inside

### 3.3 Reduce with Associative Operators

```tml
// Parallel reduction
let sum = numbers.reduce(0, do(acc, x) acc + x)
```

**Detection criteria:**
- Operator is associative (+, *, min, max, and, or)
- Initial value is identity element

### 3.4 Independent Statements

```tml
// These can run in parallel
let a = expensive_compute_a()
let b = expensive_compute_b()
let c = expensive_compute_c()
// Implicit join before using a, b, c
let result = combine(a, b, c)
```

## 4. Purity Analysis

A function is considered **pure** if:
- No mutable global state access
- No I/O operations (print, file, network)
- No channel operations
- All called functions are also pure

### 4.1 Effect System

```tml
// Pure function - can be parallelized
func square(x: I32) -> I32 {
    return x * x
}

// Impure - has IO effect
func log_and_square(x: I32) -> I32 {
    println("computing...")  // IO effect
    return x * x
}
```

### 4.2 Purity Annotations

```tml
@pure
func my_computation(x: I32) -> I32 {
    // Compiler trusts this is pure
    return complex_math(x)
}
```

## 5. Dependency Analysis

### 5.1 Loop Dependency Types

| Type | Example | Parallelizable |
|------|---------|----------------|
| None | `a[i] = f(b[i])` | Yes |
| Flow (RAW) | `a[i] = a[i-1] + 1` | No |
| Anti (WAR) | `a[i] = b[i+1]` | Yes (with copy) |
| Output (WAW) | `a[i%10] = i` | No |

### 5.2 Alias Analysis

```tml
func process(a: ref [I32], b: ref [I32]) {
    // Must prove a and b don't alias
    for i in 0 to len(a) {
        a[i] = b[i] * 2
    }
}
```

## 6. Code Generation

### 6.1 Parallel Loop IR

```llvm
; Original loop
; for i in 0 to n { body }

; Becomes parallel:
define void @parallel_loop(i32 %n) {
entry:
  %nthreads = call i32 @tml_get_num_threads()
  %chunk = udiv i32 %n, %nthreads
  call void @tml_parallel_for(ptr @loop_body, i32 0, i32 %n, i32 %chunk)
  ret void
}

define void @loop_body(i32 %start, i32 %end) {
  ; Sequential execution of chunk
  ...
}
```

### 6.2 Work Stealing Runtime

```c
typedef struct {
    atomic_int head;
    atomic_int tail;
    Task* tasks[DEQUE_SIZE];
} WorkDeque;

void worker_thread(int id) {
    while (running) {
        Task* task = steal_or_pop(id);
        if (task) {
            execute(task);
        } else {
            yield();
        }
    }
}
```

## 7. Runtime Configuration

### 7.1 Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `TML_THREADS` | CPU cores | Max parallel threads |
| `TML_CHUNK_SIZE` | auto | Minimum work per thread |
| `TML_PARALLEL` | 1 | Enable/disable (0/1) |

### 7.2 Programmatic Control

```tml
use std::parallel

func main() {
    // Set thread count
    parallel::set_threads(4)

    // Get optimal chunk size
    let chunk = parallel::optimal_chunk(workload_size)

    // Parallel region
    parallel::scope(do {
        // Parallel work here
    })
}
```

## 8. Annotations

### 8.1 Loop Annotations

```tml
// Force parallel execution
@parallel
for i in 0 to n {
    process(i)
}

// Prevent parallelization
@sequential
for i in 0 to n {
    accumulate(i)  // Has dependencies
}

// Limit parallelism
@parallel(threads: 2)
for i in 0 to n {
    io_bound_work(i)
}
```

### 8.2 Function Annotations

```tml
// Mark as parallelizable entry point
@parallel_entry
func process_batch(items: [Item]) {
    for item in items {
        process(item)
    }
}
```

## 9. Heuristics

### 9.1 Minimum Work Threshold

```
parallel_if:
  iterations >= 1000 OR
  estimated_cycles_per_iteration >= 10000
```

### 9.2 Chunk Size Calculation

```
chunk_size = max(
    min_chunk,
    iterations / (num_threads * oversubscription_factor)
)
```

### 9.3 Nested Parallelism

```tml
// Outer loop parallelized, inner sequential
@parallel
for i in 0 to n {
    @sequential  // Implicit - avoid nested parallelism overhead
    for j in 0 to m {
        work(i, j)
    }
}
```

## 10. Implementation Phases

### Phase 1: Foundation (MVP)
- Basic loop parallelization with `@parallel` annotation
- Simple thread pool (fixed size)
- No automatic detection

### Phase 2: Analysis
- Purity analysis pass
- Loop dependency detection
- Automatic parallelization of simple loops

### Phase 3: Optimization
- Work stealing scheduler
- Adaptive chunk sizing
- SIMD integration

### Phase 4: Advanced
- Pipeline parallelism
- Speculative execution
- NUMA awareness

## 11. Safety Guarantees

1. **Determinism** - Same input produces same output (order may vary for reductions)
2. **No data races** - Compiler proves non-interference
3. **Bounded resources** - Never spawn more threads than cores
4. **Deadlock-free** - No locks in generated parallel code

## 12. Comparison with Other Approaches

| Feature | TML | Bend | OpenMP | Rayon |
|---------|-----|------|--------|-------|
| Auto-detect | Yes | Yes | No | Partial |
| Annotations | Optional | None | Required | Required |
| Purity check | Yes | Implicit | No | Lifetime |
| Work stealing | Yes | Yes | Optional | Yes |
| SIMD | Integrated | No | Yes | No |
