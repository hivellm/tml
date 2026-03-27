# Tasks: Split Large Files (1500+ lines)

**Status**: In Progress
**Priority**: Medium

## Phase 1: Tier 1 — Critical Files (2000+ lines)

- [x] 1.1 Split `compiler/src/codegen/llvm/expr/call.cpp` (2618→372 lines) — extracted into call_enum, call_indirect, call_generic_func, call_class, call_primitive
- [x] 1.2 Update CMakeLists.txt for call.cpp split, rebuild, run codegen tests — build passes
- [x] 1.3 Split `compiler/src/codegen/llvm/core/generate.cpp` (2189→1606 lines) — extracted entry points into generate_entry.cpp (614 lines)
- [x] 1.4 Update CMakeLists.txt for generate.cpp split, rebuild — build passes
- [x] 1.5 Split `compiler/src/codegen/mir/instructions.cpp` (2122→795 lines) — extracted call methods into instructions_call.cpp (1355 lines)
- [x] 1.6 Update CMakeLists.txt for instructions.cpp split, rebuild — build passes
- [x] 1.7 Split `compiler/src/doc/generators_html.cpp` (2079→1217 lines) — extracted CSS/JS into generators_html_assets.cpp (875 lines)
- [x] 1.8 Update CMakeLists.txt for generators_html.cpp split, rebuild — build passes

## Phase 2: Tier 2 — High Priority (1700-2000 lines)

- [x] 2.1 Split `compiler/src/codegen/llvm/core/runtime_modules.cpp` (1992→1962 lines) — extracted emit_module_pure_tml_functions into runtime_modules_tml.cpp
- [x] 2.2 Split `compiler/src/codegen/llvm/core/generic.cpp` (1952→268 lines) — extracted generate_pending_instantiations into generic_instantiate.cpp
- [ ] 2.3 Split `compiler/src/codegen/llvm/builtins/intrinsics.cpp` (1918 lines) — single massive method, needs section extraction
- [ ] 2.4 Split `compiler/include/codegen/llvm/llvm_ir_gen.hpp` (1861 lines) — extract sub-headers, keep main as facade
- [ ] 2.5 Split `compiler/src/types/checker/expr_call_method.cpp` (1767 lines) — single massive method, needs section extraction
- [x] 2.6 Split `compiler/src/codegen/llvm/llvm_ir_gen_stmt.cpp` (1767→713 lines) — extracted gen_let_stmt into llvm_ir_gen_stmt_let.cpp
- [ ] 2.7 Rebuild + full test suite after Phase 2

## Phase 3: Tier 3 — Medium Priority (1500-1700 lines)

- [x] 3.1 Split `compiler/src/testing/testing_coverage.cpp` (1764→803) — extracted write_coverage_html into testing_coverage_html.cpp
- [x] 3.2 `compiler/src/testing/testing_compile.cpp` (1759) — SKIP: too many shared static globals
- [x] 3.3 Split `compiler/src/types/env_module_support.cpp` (1731→525) — extracted load_module_from_file into env_module_load.cpp
- [x] 3.4 Split `compiler/src/mcp/mcp_tools_project.cpp` (1724→896) — extracted analysis tools into mcp_tools_project_analysis.cpp
- [x] 3.5 Split `compiler/src/mcp/mcp_tools_docs.cpp` (1686→948) — extracted doc indexing into mcp_tools_docs_index.cpp
- [x] 3.6 Split `compiler/src/codegen/llvm/expr/infer.cpp` (1649→787 lines) — extracted infer_expr_type_extended into infer_types.cpp
- [x] 3.7 Split `compiler/src/cli/builder/builder_helpers.cpp` (1555→691 lines) — extracted get_runtime_objects into builder_helpers_runtime.cpp
- [x] 3.8 `compiler/src/cli/explain/type_errors.cpp` (1531) — SKIP: single data map literal
- [x] 3.9 `lib/std/src/sync/atomic.tml` (1507) — SKIP: TML library file
- [x] 3.10 Build passes after all Phase 3 splits

## Phase 4: Validation

- [ ] 4.1 Verify no file exceeds 1500 lines
- [ ] 4.2 Full test suite passes (0 regressions)
- [ ] 4.3 Incremental build still works correctly
