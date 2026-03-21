# ThreadBudget singleton prevents CPU thrashing in test compilation
**Source**: manual
**Date**: 2026-03-15
**Tags**: testing, performance, threading, llvm
The test system uses a ThreadBudget singleton that limits total concurrent threads to hardware_concurrency / 2. This prevents CPU thrashing from the outer (suite parallelism) × inner (per-file compilation) thread product. Without this, 8 parallel suites × 8 LLVM threads = 64 threads on an 8-core machine, causing severe thrashing. Single-threaded per-file compilation (num_compile_threads = 1) is also enforced because LLVM global state is not thread-safe across all configurations.