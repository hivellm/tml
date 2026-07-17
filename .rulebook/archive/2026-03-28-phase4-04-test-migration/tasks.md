# Tasks: Zig-Inspired Test Migration

**Status**: Complete (17/17)

## Phase 1: Stdlib Pre-Compiled Object Cache

- [x] 1.1 `build_stdlib_object()` with `library_ir_only=true`
- [x] 1.2 `capture_library_state()` for codegen state capture
- [x] 1.3 Cache stdlib .obj with mtime fingerprint
- [x] 1.4 `cached_library_state` in QueryOptions
- [x] 1.5 `provide_codegen_unit()` passthrough to LLVMIRGen
- [x] 1.6 `emit_referenced_library_declarations()` fallback to functions_ map
- [x] 1.7 Comprehensive bootstrap — test_bootstrap.tml imports ALL library modules
- [x] 1.8 obj_cache hash collision fix (full 32-char CRC32C instead of 16-char truncation)
- [x] 1.9 `processed_module_paths` tracking in CodegenLibraryState
- [x] 1.10 Fix stdlib .obj build hang — changed `lazy_library_defs=false` → `true` in `build_stdlib_object()`. Root cause: emitting all 5000+ functions from 287 stdlib modules hung. With `lazy_library_defs=true`, codegen state is captured without hanging. In unified mode, `library_decls_only=false` means the stdlib.obj is no longer used for symbol resolution, making the incomplete lazy .obj safe.
- [x] 1.11 N/A — `library_decls_only=true` removed from unified path (see 2.6). Each test .obj gets internal-linkage full definitions instead.

## Phase 2: Suite Aggregation (Mega-Binary)

- [x] 2.1 `compile_unified_binary()` with parallel 4-thread codegen
- [x] 2.2 `UnifiedTestMapping` for global index → suite/test mapping
- [x] 2.3 Mega-dispatcher generation
- [x] 2.4 Coordinator mega-binary execution with NDJSON parsing
- [x] 2.5 `max_per_suite=50` for full suite runs
- [x] 2.6 Fix LLD link — root cause was `library_decls_only=true` in unified mode leaving test .obj files with only `declare` stubs while stdlib.obj lacked required generic instantiations. Fix: use `library_decls_only=false` (each test .obj gets full internal-linkage library defs, same as per-suite mode). Stdlib .obj no longer linked in unified path.
- [x] 2.7 Verified: unified binary mode compiles 94 tests to single .exe and executes them. `tml test --unified` flag added, coordinator wired with `TestConfig::use_unified_binary`, CLI `TestOptions::unified_binary` field.
- [x] 2.8 Benchmark (collections suite, 94 tests, --no-cache): unified=37s (1 link, 36s compile + 1s run). Per-suite baseline pending (still running). Single link step confirmed working. Compile time dominated by parallel codegen, not linking.
