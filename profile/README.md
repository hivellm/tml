# TML Performance Profile Suite

This directory contains comprehensive benchmarks comparing TML performance against equivalent C++ implementations.

## Directory Structure

```
profile/
├── common/           # Shared benchmark utilities
│   ├── bench.hpp     # C++ benchmark framework
│   └── bench.tml     # TML benchmark framework
├── cpp/              # C++ benchmark implementations
│   ├── math_bench.cpp
│   ├── string_bench.cpp
│   ├── json_bench.cpp
│   ├── collections_bench.cpp
│   ├── memory_bench.cpp
│   ├── text_bench.cpp
│   └── CMakeLists.txt
├── tml/              # TML benchmark implementations
│   ├── math_bench.tml
│   ├── string_bench.tml
│   ├── json_bench.tml
│   ├── collections_bench.tml
│   ├── memory_bench.tml
│   └── text_bench.tml
├── results/          # Benchmark results (JSON + reports)
├── build.bat         # Build all benchmarks
├── run.bat           # Run all benchmarks
└── report.py         # Generate comparison report
```

## Quick Start

1. **Build the TML compiler first:**
   ```batch
   scripts\build.bat release
   ```

2. **Build all benchmarks:**
   ```batch
   cd profile
   build.bat
   ```

3. **Run all benchmarks:**
   ```batch
   run.bat
   ```

4. **Generate comparison report:**
   ```batch
   python report.py
   ```

## Benchmark Categories

### Math (`math_bench`)
- Integer arithmetic (add, mul, div, mod)
- Float arithmetic
- Bitwise operations
- Fibonacci (recursive and iterative)
- Loop overhead

### String (`string_bench`)
- String concatenation
- String comparison
- String length
- Integer to string conversion
- Log building patterns

### JSON (`json_bench`)
- Parse small/medium JSON
- Field access
- Array iteration
- Nested object access
- Type validation

### Collections (`collections_bench`)
- List/Vector push/pop
- Random access
- Iteration
- HashMap insert/lookup/remove

### Memory (`memory_bench`)
- Heap allocation patterns
- Stack struct creation
- Sequential vs random access
- Buffer I/O

### Text (`text_bench`)
- StringBuilder patterns
- JSON/HTML/CSV building
- Log message construction
- Path building

## Adding New Benchmarks

1. Create `category_bench.cpp` in `cpp/`
2. Create `category_bench.tml` in `tml/`
3. Add to `CMakeLists.txt` if C++
4. Follow the existing pattern for timing and output

## Performance Targets

- **Excellent**: TML within 10% of C++ (ratio ≤ 1.1)
- **Good**: TML within 50% of C++ (ratio ≤ 1.5)
- **Needs Attention**: TML 2x slower (ratio ≤ 2.0)
- **Critical**: TML more than 2x slower (ratio > 2.0)

## Notes

- All benchmarks use warmup iterations to ensure JIT/cache effects are minimal
- C++ is compiled with `-O3` optimizations
- TML uses `--release` flag for optimizations
- Results are saved as JSON for automated comparison
