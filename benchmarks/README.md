# TML Benchmarks

Comparative benchmarks between **TML**, **C++**, and **Rust**.

## Philosophy

These benchmarks measure:
1. **Runtime Performance** - Execution speed of equivalent algorithms
2. **Build Time** - Compilation speed
3. **Binary Size** - Output executable size

## Categories

| Category | Description |
|----------|-------------|
| `arithmetic` | Basic math operations without allocation |
| `loops` | Loop performance (for, while) |
| `functions` | Function call overhead |
| `collections` | List/Vec operations |
| `strings` | String manipulation |
| `io` | File and console I/O |

## Running Benchmarks

```bash
# Run all benchmarks
python benchmarks/scripts/run_benchmarks.py

# Run specific category
python benchmarks/scripts/run_benchmarks.py --category arithmetic

# Generate report
python benchmarks/scripts/generate_report.py
```

## Results

Results are saved in `benchmarks/results/` as JSON and Markdown reports.

## Adding New Benchmarks

1. Create equivalent code in `cpp/`, `rust/`, `tml/`
2. Add benchmark metadata to `scripts/benchmarks.json`
3. Run the benchmark suite

## Requirements

- **C++**: clang++ or g++ with C++20
- **Rust**: rustc (stable)
- **TML**: tml compiler (this repo)
- **Python**: 3.10+ for scripts
