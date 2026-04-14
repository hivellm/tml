# Analysis: benchmark

> Created by benchmark analysis on 2026-04-13
> Analysis ID: `benchmark`

## Index

| File | Purpose |
|------|---------|
| [01-executive-summary.md](./01-executive-summary.md) | Overview and key metrics |
| [02-math-arithmetic.md](./02-math-arithmetic.md) | Integer, float, bitwise benchmarks |
| [03-control-flow.md](./03-control-flow.md) | If-else, when/match, loops, short-circuit |
| [04-collections.md](./04-collections.md) | List vs Vec, HashMap comparison |
| [05-memory-structs.md](./05-memory-structs.md) | Struct access, allocation, array ops |
| [06-functions-closures.md](./06-functions-closures.md) | Function calls, closures, higher-order |
| [07-encoding.md](./07-encoding.md) | Base64, Hex, Base32 |
| [08-compilation.md](./08-compilation.md) | Compile time, binary size, toolchain |
| [09-memory-management.md](./09-memory-management.md) | Leaks, allocation, RAII comparison |
| [10-gap-analysis.md](./10-gap-analysis.md) | All gaps ranked by severity |
| [11-recommendations.md](./11-recommendations.md) | Prioritized action plan |
| [12-oop-dispatch.md](./12-oop-dispatch.md) | OOP: class, method dispatch, composition |
| [13-type-conversions.md](./13-type-conversions.md) | core::num type casts (int, float, byte) |
| [14-text-stringbuilder.md](./14-text-stringbuilder.md) | std::text, core::str (BLOCKED) |
| [15-networking.md](./15-networking.md) | std::net TCP/UDP/async (BLOCKED) |
| [16-crypto-json-blocked.md](./16-crypto-json-blocked.md) | Blocked modules: crypto, JSON, string |
| [17-core-coverage-map.md](./17-core-coverage-map.md) | Core library — per-module benchmark coverage |
| [18-std-coverage-map.md](./18-std-coverage-map.md) | Std library — per-module benchmark coverage |
| [19-per-operation-heatmap.md](./19-per-operation-heatmap.md) | All operations ranked by speed |
| [manifest.json](./manifest.json) | Analysis metadata |

## Methodology

- **Platform**: Windows 10 Pro x64
- **TML**: v0.3.1 (debug build, LLVM backend, `--stage=parser:cpp`)
- **Rust**: rustc with `-O` (release optimization)
- **Warmup**: All benchmarks include warmup iterations
- **Data**: Single-run, `.sandbox/*.log` raw output preserved

## Fairness Caveat

TML is benchmarked in **debug mode**; Rust in **release mode**. This is the single largest confound. Many gaps (especially struct access, closure inlining) would shrink significantly with TML release builds and optimization passes enabled.
