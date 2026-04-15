# Proposal: phase0n_daemon-compile-integration

## Why
Even after phase0l (DLL caching) and phase0m (release build), each `tml.exe` invocation starts a new process, re-parses the project manifest, and re-initializes LLVM context. A persistent background daemon (similar to `rustc`'s `rust-analyzer` or Kotlin's `kotlin-daemon`) keeps the compiler warm between invocations: DLLs mapped, LLVM context initialized, cached module metadata loaded. Incremental compilation already tracks file hashes (`incr.bin`) but the query cache is cold on each new process. With a daemon, the second compile of a project (even touching different files) pays zero startup cost. This is the final step to match Rust's <5ms incremental compile wall-time goal. See `docs/analysis/benchmark/08-compilation.md`.

## What Changes
1. A new `tml daemon` command starts a background process that listens on a named pipe / local socket for compile requests.
2. `tml build`/`tml run`/`tml test` will detect if a daemon is running for the current project (via a PID file in `.tml-daemon.pid`) and forward the request over the pipe instead of loading DLLs in-process.
3. The daemon holds the LLVM context, incremental cache, and all loaded DLLs resident — a compile request over the pipe does only parsing + codegen, no init overhead.
4. `tml daemon stop` sends a shutdown signal; the daemon auto-exits after 30 minutes of inactivity.

## Impact
- Affected specs: compiler/daemon-mode, build/compilation-latency
- Affected code: new `compiler/src/cmd/cmd_daemon.cpp`, `compiler/src/plugin/plugin_loader.cpp`, `compiler/src/cmd/cmd_build.cpp`
- Breaking change: NO (daemon is opt-in; direct invocation continues to work)
- User benefit: Incremental compiles drop to <5ms — matching `cargo check` incremental performance. Zero source-level changes required.
