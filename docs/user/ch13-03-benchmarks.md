# Benchmarks

TML includes a built-in benchmarking framework that measures function performance with configurable iteration counts.

## Writing Benchmarks

Mark a function with the `@bench` decorator to make it a benchmark:

```tml
use test

@bench
func bench_string_concat() {
    var result = ""
    loop i in 0 to 100 {
        result = result + "x"
    }
}
```

The benchmark runner executes the function multiple times and reports the average duration.

## Controlling Iterations

By default, the framework chooses an iteration count. You can specify it explicitly:

```tml
@bench(iterations = 10000)
func bench_hash() {
    let _ = "hello world".hash()
}

@bench(iterations = 1000)
func bench_sort() {
    var data = [5, 3, 8, 1, 9, 2, 7, 4, 6, 0]
    data.sort()
}
```

Higher iteration counts produce more stable measurements but take longer to run.

## Running Benchmarks

Benchmarks run alongside tests when you use the `--bench` flag:

```bash
# Run all tests and benchmarks
tml test --bench

# Run benchmarks in a specific suite
tml test --suite core/str --bench

# Run only benchmarks matching a pattern
tml test --filter "bench_" --bench
```

## Benchmark Best Practices

**Measure what matters.** Put only the code you want to measure inside the benchmark function. Move setup code outside the measured path when possible:

```tml
@bench(iterations = 5000)
func bench_lookup() {
    // Setup happens each iteration here - acceptable for small setup
    var map = HashMap[Str, I32].new()
    map.insert("key", 42)

    // This is what we're actually measuring
    let _ = map.get("key")
}
```

**Use realistic data.** Benchmark with data that resembles production workloads. Sorting ten elements produces different results than sorting ten thousand.

**Compare before and after.** Run benchmarks before making a change, save the output, make the change, and run again. This gives you a clear picture of performance impact.

**Avoid dead code elimination.** The compiler may optimize away computations whose results are unused. Assign results to variables or use them in assertions to prevent this:

```tml
@bench(iterations = 10000)
func bench_parse() {
    let result = parse_number("12345")
    assert(result.is_ok())
}
```

## Example: Comparing Algorithms

```tml
use test

@bench(iterations = 1000)
func bench_linear_search() {
    let data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    loop i in 0 to 10 {
        let _ = data.contains(i)
    }
}

@bench(iterations = 1000)
func bench_hash_lookup() {
    var set = HashSet[I32].new()
    loop i in 0 to 10 { set.insert(i) }
    loop i in 0 to 10 {
        let _ = set.contains(i)
    }
}
```

Running these side by side reveals the crossover point where hash-based lookups outperform linear search.
