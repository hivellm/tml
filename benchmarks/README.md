# TML Language Benchmarks

Benchmark comparisons between TML, Rust, C++, and Go. Includes optimization effectiveness benchmarks.

## Structure

```
benchmarks/
├── README.md                  # This file
├── run_all.bat                # Windows script to run all benchmarks
├── tml/                       # TML benchmark implementations
│   ├── fibonacci.tml          # Fibonacci sequences
│   ├── algorithms.tml         # Classic algorithms (factorial, GCD, etc.)
│   ├── data_structures.tml    # Data structure operations
│   ├── math.tml               # Mathematical computations
│   ├── optimization_bench.tml # HIR/MIR optimization benchmarks
│   └── oop_bench.tml          # OOP performance benchmarks
├── rust/                      # Rust benchmark implementations
│   ├── Cargo.toml
│   └── benches/
│       └── algorithms.rs      # Criterion benchmarks
├── cpp/                       # C++ benchmark implementations
│   ├── CMakeLists.txt
│   └── algorithms.cpp         # Standalone benchmark program
├── go/                        # Go benchmark implementations
│   ├── go.mod
│   ├── main.go                # Standalone program
│   └── algorithms_test.go     # Go testing benchmarks
├── scripts/                   # Benchmark runner scripts
│   ├── run_benchmarks.py      # Cross-language benchmark runner
│   └── run_optimization_bench.py  # Optimization effectiveness benchmark
└── results/                   # Benchmark results and comparisons
    └── comparison.md          # Cross-language comparison
```

## Benchmark Categories

### 1. Algorithm Benchmarks
- **Fibonacci**: Recursive and iterative implementations
- **Factorial**: Recursive and iterative
- **GCD**: Euclidean algorithm (recursive & iterative)
- **Power**: Naive and fast exponentiation
- **Prime Check**: Trial division with 6k±1 optimization
- **Collatz Conjecture**: Steps to reach 1

### 2. Data Structure Benchmarks
- Stack push/pop cycles
- Binary search steps
- Bubble sort comparisons
- Matrix multiplication operations
- Hash function computation
- Linked list traversal

### 3. Mathematical Computations
- Integer square root (Newton's method)
- Modular exponentiation
- Ackermann function
- Catalan numbers
- Binomial coefficients
- Euler's totient function
- Sum of divisors

### 4. OOP Benchmarks
- Virtual dispatch (Shape hierarchy)
- Object creation (Point distance)
- HTTP handler simulation
- Game loop simulation (Entity updates)
- Deep inheritance chains (5 levels)
- Method chaining (Builder pattern)

## Running Benchmarks

### TML
```bash
# From project root
build\debug\tml.exe build benchmarks\tml\fibonacci.tml -o benchmarks\tml\fibonacci
benchmarks\tml\fibonacci.exe

# With optimizations (MIR)
build\debug\tml.exe build benchmarks\tml\algorithms.tml -O3 --emit-mir
```

### Rust
```bash
cd benchmarks/rust
cargo bench
```

### C++
```bash
cd benchmarks/cpp
# MSVC
cl /O2 /EHsc algorithms.cpp /Fe:algorithms.exe
algorithms.exe

# GCC/Clang
g++ -O3 -o algorithms algorithms.cpp
./algorithms
```

### Go
```bash
cd benchmarks/go
go run main.go           # Quick test
go test -bench=. -benchmem  # Full benchmarks
```

## Expected Results

All implementations should produce identical results:

| Benchmark | Expected Value |
|-----------|----------------|
| Factorial(10) | 3628800 |
| Fibonacci(20) | 6765 |
| GCD(48, 18) | 6 |
| Power(2, 10) | 1024 |
| Primes ≤ 100 | 25 |
| Sum(1..100) | 5050 |
| Collatz(27) | 111 steps |

## Language Comparison Notes

| Language | Strengths | Weaknesses |
|----------|-----------|------------|
| **TML**  | Clean syntax, LLM-friendly, explicit keywords | Young ecosystem |
| **Rust** | Zero-cost abstractions, memory safety | Complex syntax for LLMs |
| **C++**  | Maximum performance, mature ecosystem | Manual memory management |
| **Go**   | Simple, fast compilation, built-in GC | Less control over memory |

## Optimization Levels

| Language | Levels | Description |
|----------|--------|-------------|
| **TML** | `-O0` to `-O3` | None, basic, standard, aggressive |
| **Rust** | debug/release | Unoptimized vs full optimization |
| **C++** | `-O0` to `-O3` | None to aggressive |
| **Go** | N/A | Built-in optimizations |

## Syntax Comparison

### Factorial - TML
```tml
func factorial(n: I32) -> I32 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

### Factorial - Rust
```rust
fn factorial(n: i32) -> i32 {
    if n <= 1 { 1 } else { n * factorial(n - 1) }
}
```

### Factorial - C++
```cpp
int32_t factorial(int32_t n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
```

### Factorial - Go
```go
func factorial(n int32) int32 {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
```

## Key Observations

1. **TML vs Rust**: TML uses more explicit keywords (`func` vs `fn`, `and`/`or` vs `&&`/`||`)
2. **TML vs C++**: Similar control flow, but TML has no header files and cleaner type syntax
3. **TML vs Go**: Very similar syntax, but TML has explicit return types and no GC

## Optimization Benchmarks

The `optimization_bench.tml` file contains code patterns designed to test specific optimization passes:

### HIR Optimizations
- Constant folding (integers, floats, booleans, bitwise)
- Dead code elimination (constant if conditions)
- Short-circuit evaluation

### MIR Optimizations
- Constant folding
- Constant propagation
- Copy propagation
- Common subexpression elimination (CSE)
- Dead code elimination
- Unreachable code elimination

### Running Optimization Benchmarks

```bash
# Run the optimization benchmark script
python benchmarks/scripts/run_optimization_bench.py

# With verbose output
python benchmarks/scripts/run_optimization_bench.py --verbose

# Custom output file
python benchmarks/scripts/run_optimization_bench.py --output my_report.md
```

### Expected Results

| Benchmark | O0 | O2 | Reduction |
|-----------|-----|-----|-----------|
| optimization_bench.tml | ~650 instrs | ~373 instrs | ~43% |
| algorithms.tml | ~666 instrs | ~407 instrs | ~39% |

### Manual MIR Inspection

```bash
# Compare MIR output at different optimization levels
tml build benchmarks/tml/optimization_bench.tml -O0 --emit-mir
cat build/debug/optimization_bench.mir > mir_O0.txt

tml build benchmarks/tml/optimization_bench.tml -O2 --emit-mir
cat build/debug/optimization_bench.mir > mir_O2.txt

# Compare the files to see optimizations applied
diff mir_O0.txt mir_O2.txt
```
