# Tasks: Tracy Profiler Integration

**Status**: In Progress (Phase 1 + Phase 4 foundation done)
**Priority**: High (needed for HTTP benchmark optimization)

## Phase 1: Setup & Build Integration — DONE

- [x] 1.1 Tracy as git submodule in `src/tracy/`
- [x] 1.2 `compiler/include/profiler.hpp` with compile-out macros
- [x] 1.3 `TML_PROFILE` CMake option (OFF by default)
- [x] 1.4 `tml_tracy` static library with TracyClient.cpp
- [x] 1.5 `--profile` flag in `scripts/build.bat`
- [x] 1.6 Normal build verified: zero overhead, str 22/22 tests pass
- [ ] 1.7 Verify profile build connects to Tracy viewer

## Phase 2: Compiler Pipeline Instrumentation (C++) — DONE

- [x] 2.1 query::typecheck_module + query::codegen_unit
- [x] 2.2 lexer::tokenize
- [x] 2.3 parser::parse
- [ ] 2.4 Instrument `TypeChecker::check()` — zone per module
- [x] 2.5 types::check_module
- [x] 2.6 hir::lower_module
- [x] 2.7 mir::build
- [x] 2.8 codegen::generate
- [x] 2.9 llvm::compile_ir_to_object
- [x] 2.10 lld::link + test::run_tests

## Phase 3: Detailed Compiler Instrumentation (C++)

- [ ] 3.1 Add zone text with file/function names for context
- [ ] 3.2 Instrument generic monomorphization (count + time per instantiation)
- [ ] 3.3 Instrument MIR optimization passes individually
- [ ] 3.4 Instrument memory arena allocations (TracyAlloc/TracyFree)
- [ ] 3.5 Instrument incremental cache lookups (hit/miss as plot)
- [ ] 3.6 Instrument test subprocess coordination
- [ ] 3.7 Add Tracy plots for: active threads, memory usage, cache hit rate

## Phase 4: TML Runtime & Standard Library Profiling

- [x] 4.1 FFI bindings: begin/end/message/plot/frame_mark in `lib/core/src/profiler.tml`
- [x] 4.2 C shim in `essential.c`: zone stack, Tracy C API calls, stubs when not profiling
- [ ] 4.3 `@inline` Zone guard with `drop` — deferred (needs Drop codegen for reliable cleanup)
- [ ] 4.4 Create `#ifdef PROFILE` conditional compilation support so TML instrumentation compiles out
- [ ] 4.5 Instrument `core/alloc` — `mem_alloc`/`mem_free` report to Tracy memory profiler
- [ ] 4.6 Instrument `core/str` — zone on `Str::from`, `Str::concat`, `Str::split`, `Str::replace`
- [ ] 4.7 Instrument `core/fmt` — zone on `format()`, `Display::to_string()`, `Debug::debug_string()`
- [ ] 4.8 Instrument `core/iter` — zone on `collect()`, `fold()`, `map().collect()`
- [ ] 4.9 Instrument `core/slice` — zone on `sort()`, `binary_search()`, `chunks()`
- [ ] 4.10 Instrument `std/collections` — zone on `HashMap::insert`/`get`/`resize`, `List::push`/`sort`
- [ ] 4.11 Instrument `std/json` — zone on `Json::parse()`, `Json::stringify()`
- [ ] 4.12 Instrument `std/file` — zone on `File::read()`, `File::write()`, `File::read_to_string()`
- [ ] 4.13 Instrument `std/crypto` — zone on hash/encrypt/decrypt operations
- [ ] 4.14 Instrument `std/net` — zone on `TcpStream::connect`/`read`/`write`, `UdpSocket::send`/`recv`
- [ ] 4.15 Add Tracy message logging: `profiler::message(text)` for runtime trace messages
- [ ] 4.16 Add Tracy plots from TML: `profiler::plot(name, value)` for custom metrics (e.g., collection sizes, cache hits)

## Phase 5: End-to-End Application Profiling

- [ ] 5.1 `tml run --profile` flag — compiles with profiling enabled, connects to Tracy viewer
- [ ] 5.2 `tml test --profile` flag — profiles test execution including stdlib hot paths
- [ ] 5.3 Combined view: compiler zones (C++) + runtime zones (TML) in same Tracy timeline
- [ ] 5.4 Create example profiled program in `examples/profiling_demo.tml` showing Tracy integration
- [ ] 5.5 Profile-guided docs: `docs/PROFILING.md` covering compiler profiling, runtime profiling, and how to read the Tracy timeline

## Phase 6: Developer Workflow & CI

- [ ] 6.1 Add `tml build --tracy` CLI flag to enable profiled compilation
- [ ] 6.2 Add Tracy capture file export (`tml build --tracy-export trace.tracy`)
- [ ] 6.3 Create `.sandbox/profile_*.tracy` workflow for comparing before/after
- [ ] 6.4 CI job: profile full test suite, export `.tracy` capture, compare against baseline
- [ ] 6.5 Performance regression alert: flag >10% slowdown in any pipeline zone

## Validation

- [ ] V.1 Normal build (`scripts/build.bat`) has zero Tracy symbols in binary
- [ ] V.2 Normal TML programs have zero profiling overhead (macros/conditionals compiled out)
- [ ] V.3 Profile build connects to Tracy viewer and shows compiler zones
- [ ] V.4 TML runtime zones appear in Tracy viewer alongside compiler zones
- [ ] V.5 Full compilation pipeline visible as nested zones in Tracy timeline
- [ ] V.6 `HashMap::insert` in a TML program shows as a named zone in Tracy
- [ ] V.7 Memory allocation tracking shows both arena (C++) and heap (TML) usage
- [ ] V.8 At least one optimization identified and implemented using Tracy data
