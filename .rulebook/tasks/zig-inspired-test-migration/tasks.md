# Tasks: Zig-Inspired Test Migration

**Status**: In Progress (Phase 1-2 infrastructure complete, blocked on codegen bottleneck)

## Key Findings (2026-03-16)

### Architecture Implemented
- `cached_library_state` + `library_ir_only` + `capture_library_state()` pipeline works
- `compile_unified_binary()` with 4 parallel codegen threads implemented
- Mega-binary coordinator path with NDJSON event parsing implemented
- `emit_referenced_library_declarations()` fallback to `functions_` map for cached state

### Blockers Discovered
1. **`library_decls_only=true` incomplete**: Lazy library functions (core::char, core::encoding, etc.) not declared when not in bootstrap's imports. Fix: comprehensive stdlib .obj with all modules, but building that hangs (287 modules causes codegen to take 10+ minutes with no termination)
2. **`library_decls_only=false` too large**: Each test .obj = ~900KB with full library defs. 500+ objects = ~450MB → LLD OOM/crash
3. **`cached_library_state` scope limited**: Only covers bootstrap file's imports (core::str, core::option, core::fmt). Tests using core::char, core::encoding, std::file etc. fail with undefined symbols
4. **Per-file QueryContext overhead**: Each `codegen_unit()` takes ~250ms even with incremental GREEN cache. 1483 files × 250ms = ~370s irreducible

### What Works
- Per-suite compilation path (original) — no regressions
- `max_per_suite=50` for full suite runs — reduces suites from 350 → 268
- Incremental cache for codegen results
- Runtime archive pre-build (shared across suites)

## 1. Stdlib Pre-Compiled Object Cache — BLOCKED

- [x] 1.1 `build_stdlib_object()` function with `library_ir_only=true`
- [x] 1.2 `capture_library_state()` for codegen state
- [x] 1.3 Cache stdlib .obj with mtime fingerprint
- [x] 1.4 `cached_library_state` in QueryOptions
- [x] 1.5 `provide_codegen_unit()` passthrough to LLVMIRGen
- [x] 1.6 `emit_referenced_library_declarations()` fallback to functions_ map
- [ ] 1.7 Fix `library_decls_only=true` to declare ALL library functions (not just bootstrap imports)
- [ ] 1.8 Fix comprehensive stdlib .obj build (287 modules hangs — needs incremental/batched approach)
- [ ] 1.9 Verify all 1483 tests pass with library_decls_only=true + stdlib .obj

## 2. Suite Aggregation (Mega-Binary) — IMPLEMENTED, BLOCKED ON 1.7

- [x] 2.1 `compile_unified_binary()` with parallel 4-thread codegen
- [x] 2.2 `UnifiedTestMapping` for global index → suite/test mapping
- [x] 2.3 Mega-dispatcher generation (reuses existing infrastructure)
- [x] 2.4 Coordinator mega-binary execution with NDJSON std::visit parsing
- [x] 2.5 `max_per_suite=50` for full suite runs (reduces link count)
- [ ] 2.6 Fix LLD link: needs library_decls_only=true (fix 1.7 first)
- [ ] 2.7 Verify all tests pass in mega-binary mode
- [ ] 2.8 Benchmark vs per-suite baseline

## 3-9. Remaining phases — pending Phase 1-2 completion

(See previous version for full phase list)

## Files Modified

| File | Changes |
|------|---------|
| `compiler/include/query/query_context.hpp` | Added `cached_library_state`, `library_decls_only` to QueryOptions |
| `compiler/src/query/query_core.cpp` | Pass cached state to AST codegen path |
| `compiler/src/testing/testing_compile.cpp` | `build_stdlib_object()`, `compile_unified_binary()`, globals |
| `compiler/include/testing/testing_compile.hpp` | `UnifiedTestMapping`, `compile_unified_binary()` declaration |
| `compiler/src/testing/testing_coordinator.cpp` | `max_per_suite=50` for full runs |
| `compiler/src/codegen/llvm/core/runtime_modules.cpp` | `emit_referenced_library_declarations()` fallback to functions_ |
| `compiler/include/codegen/llvm/llvm_ir_gen.hpp` | Reference only (existing infrastructure) |

## Next Steps (Priority Order)

1. **Fix comprehensive stdlib .obj** — batch the 287 modules into groups of 20-30 and build partial stdlib .obj files, or find why the full pass hangs
2. **Fix `library_decls_only` declarations** — ensure all lazy functions get declare stubs
3. **Test mega-binary end-to-end** — with fixed stdlib .obj + declarations
4. **Benchmark** — compare total time: per-suite (current) vs mega-binary (new)
