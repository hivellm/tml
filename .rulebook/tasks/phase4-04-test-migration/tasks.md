# Tasks: Zig-Inspired Test Migration

**Status**: In Progress (71%, 12/17)

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
⚠️ BLOCKER: Building 287 stdlib modules as .obj files causes the compiler to hang. Needs a batched/incremental approach — compile N modules at a time instead of all at once.

- [ ] 1.10 Fix stdlib .obj build hang (287 modules — needs batched/incremental approach)
- [ ] 1.11 Verify all tests pass with `library_decls_only=true` + stdlib .obj

## Phase 2: Suite Aggregation (Mega-Binary)

- [x] 2.1 `compile_unified_binary()` with parallel 4-thread codegen
- [x] 2.2 `UnifiedTestMapping` for global index → suite/test mapping
- [x] 2.3 Mega-dispatcher generation
- [x] 2.4 Coordinator mega-binary execution with NDJSON parsing
- [x] 2.5 `max_per_suite=50` for full suite runs
⚠️ BLOCKER: LLD fails to link when using library_decls_only=true. Root cause: some symbols expected by test code are missing from the .obj files. Needs investigation of which symbols are missing and why.

- [ ] 2.6 Fix LLD link with `library_decls_only=true`
- [ ] 2.7 Verify all tests pass in mega-binary mode
- [ ] 2.8 Benchmark vs per-suite baseline
