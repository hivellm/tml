# Proposal: phase41b_shared-stdlib-object

## Why
Every test file currently re-emits the **entire stdlib** (~5000 functions across 287 modules) during codegen, and every test object embeds the full stdlib with internal linkage:

- The stdlib codegen-state fast-path is **disabled**: the `build_stdlib_object(config)` call is commented out at `compiler/src/testing/testing_compile_parallel.cpp:41-43` with the note "cached state causes codegen issues: I32::duplicate redefinition … i64/i32 type mismatches". With `g_stdlib_codegen_state` null, `compile_suite` (`testing_compile.cpp:623-628`) falls back to `qopts.incremental=true` and runs `emit_module_pure_tml_functions()` for the whole stdlib per test file. (Finding F-006, **Very High** impact.)
- `library_decls_only=false` (`testing_compile.cpp:995`, `testing_compile_parallel.cpp:219-224`): each test `.obj` carries full internal-linkage stdlib defs → ~345 KB per EXE, 837 MB cache, heavy per-EXE links. Rooted in the same LLD multiple-definition problem. (F-007, **High**.)
- `object_compiler.cpp:269-274` constructs and initializes a fresh `LLVMBackend` per object compilation. (F-012, **Medium**.)

This is the highest-value structural fix in the test pipeline: it multiplies with everything else (per-file or aggregated).

## What Changes
Root-cause and fix the duplicate-symbol / type-mismatch issues that forced both band-aids, then:
- Re-enable `build_stdlib_object` so the stdlib is codegen'd **once** into a shared object (with external linkage), and test objects compile with `library_decls_only=true` (declarations only), linking against the shared stdlib object.
- Reuse one initialized `LLVMBackend`/TargetMachine per worker thread instead of per object.
- Methodology: Rust-as-Reference where applicable; reproduce the historical failures (`I32::duplicate` redefinition, i64/i32 mismatches) first, fix the root cause in codegen/linkage decisions, not by re-hiding symbols.

## Impact
- Affected specs: none (tooling/codegen internals; language semantics unchanged)
- Affected code: `compiler/src/testing/testing_compile.cpp`, `testing_compile_parallel.cpp`, `compiler/src/cli/builder/object_compiler.cpp`, possibly linkage decisions in codegen (`compiler/src/codegen/llvm/...`)
- Breaking change: NO (identical test results required; only compile pipeline internals change)
- User benefit: eliminates the ~5000-function redundant stdlib codegen per test file and shrinks every link; compounds with phase41a aggregation
