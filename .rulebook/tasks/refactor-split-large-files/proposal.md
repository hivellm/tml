# Proposal: Split Large Files (1500+ lines)

## Status: APPROVED

**Priority**: Medium
**Impact**: Maintainability, compile times, code navigation, AI agent effectiveness

## Why

19 files in the codebase exceed 1500 lines. Large files cause:

1. **AI context waste** — agents must read 2000+ lines to find the 50 they need to edit
2. **Merge conflicts** — multiple features touching the same giant file
3. **Cognitive overload** — developers can't hold 2600 lines of method dispatch logic in their head
4. **Incremental build impact** — changing one function recompiles the entire translation unit
5. **Code duplication risk** — hard to find existing helpers buried in a 2000-line file

## What Changes

Split each file into cohesive sub-modules by responsibility. Each resulting file should be 300-800 lines. The split must be purely mechanical — no logic changes, no refactoring, no behavior changes.

### Split Strategy Per File Type

**Codegen files** (`codegen/llvm/`): Split by dispatch target — one file per major type/pattern (struct calls, enum calls, generic calls, builtin calls, etc.)

**Type checker files** (`types/`): Split by phase or construct — method resolution, operator overloading, generic instantiation, etc.

**Testing files** (`testing/`): Split by concern — discovery, compilation, execution, reporting, coverage analysis.

**MCP files** (`mcp/`): Already somewhat split by domain — further split by tool group.

**Header files** (`include/`): Extract sub-headers per logical grouping, keep a main header that `#include`s them all for backward compatibility.

### Rules for Every Split

1. **Zero behavior change** — before/after must produce identical output for all tests
2. **Include compatibility** — existing `#include` paths must still work (add forwarding headers if needed)
3. **No cross-file refactoring** — don't rename functions, change signatures, or move logic between split files
4. **Test after each file** — rebuild + run affected tests after splitting each file
5. **One file per commit** — each split is its own atomic commit

## Files to Split (19 total)

### Tier 1: Critical (2000+ lines) — 4 files

| Lines | File | Suggested Split |
|-------|------|-----------------|
| 2618 | `compiler/src/codegen/llvm/expr/call.cpp` | call_dispatch.cpp, call_method.cpp, call_builtin.cpp, call_generic.cpp |
| 2189 | `compiler/src/codegen/llvm/core/generate.cpp` | generate_decl.cpp, generate_expr.cpp, generate_type.cpp, generate_module.cpp |
| 2122 | `compiler/src/codegen/mir/instructions.cpp` | instructions_call.cpp, instructions_alloc.cpp, instructions_control.cpp, instructions_ops.cpp |
| 2079 | `compiler/src/doc/generators_html.cpp` | html_module.cpp, html_type.cpp, html_function.cpp, html_index.cpp, html_css.cpp |

### Tier 2: High (1700-2000 lines) — 6 files

| Lines | File | Suggested Split |
|-------|------|-----------------|
| 1992 | `compiler/src/codegen/llvm/core/runtime_modules.cpp` | runtime_core.cpp, runtime_collections.cpp, runtime_sync.cpp, runtime_io.cpp |
| 1952 | `compiler/src/codegen/llvm/core/generic.cpp` | generic_instantiate.cpp, generic_resolve.cpp, generic_cache.cpp |
| 1916 | `compiler/src/codegen/llvm/builtins/intrinsics.cpp` | intrinsics_memory.cpp, intrinsics_math.cpp, intrinsics_string.cpp, intrinsics_io.cpp |
| 1844 | `compiler/include/codegen/llvm/llvm_ir_gen.hpp` | llvm_ir_gen.hpp (core), llvm_ir_gen_expr.hpp, llvm_ir_gen_types.hpp, llvm_ir_gen_decl.hpp |
| 1767 | `compiler/src/types/checker/expr_call_method.cpp` | method_resolve.cpp, method_dispatch.cpp, method_generic.cpp |
| 1767 | `compiler/src/codegen/llvm/llvm_ir_gen_stmt.cpp` | stmt_control.cpp, stmt_assign.cpp, stmt_decl.cpp |

### Tier 3: Medium (1500-1700 lines) — 9 files

| Lines | File | Suggested Split |
|-------|------|-----------------|
| 1764 | `compiler/src/testing/testing_coverage.cpp` | coverage_track.cpp, coverage_report.cpp, coverage_merge.cpp |
| 1759 | `compiler/src/testing/testing_compile.cpp` | compile_discover.cpp, compile_suite.cpp, compile_parallel.cpp |
| 1731 | `compiler/src/types/env_module_support.cpp` | env_module_resolve.cpp, env_module_import.cpp, env_module_register.cpp |
| 1724 | `compiler/src/mcp/mcp_tools_project.cpp` | mcp_project_build.cpp, mcp_project_coverage.cpp, mcp_project_structure.cpp |
| 1686 | `compiler/src/mcp/mcp_tools_docs.cpp` | mcp_docs_search.cpp, mcp_docs_get.cpp, mcp_docs_list.cpp |
| 1649 | `compiler/src/codegen/llvm/expr/infer.cpp` | infer_type.cpp, infer_generic.cpp, infer_coerce.cpp |
| 1555 | `compiler/src/cli/builder/builder_helpers.cpp` | helpers_link.cpp, helpers_compile.cpp, helpers_cache.cpp |
| 1531 | `compiler/src/cli/explain/type_errors.cpp` | errors_type.cpp, errors_borrow.cpp, errors_generic.cpp |
| 1507 | `lib/std/src/sync/atomic.tml` | atomic_types.tml, atomic_ops.tml, atomic_fence.tml |

## Risk

Low. Purely mechanical file splits with no logic changes. Risk is limited to:
- Missing `#include` in a new file (caught immediately by build)
- Breaking CMakeLists.txt source lists (fixed by adding new files)
- Forgetting a forward declaration in a split header
