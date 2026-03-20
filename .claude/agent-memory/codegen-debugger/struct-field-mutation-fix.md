---
name: struct-field-mutation-fix
description: Mutable struct field assignment (p.x = 99) dead code bug in THIR MIR builder — alloca path unreachable (2026-03-20, FIXED)
type: project
---

## Bug: struct field mutation generates invalid IR in MIR codegen path

**Symptom**: `store i32 99, ptr %v4` where `%v4` is i32 not ptr

**Root cause**: In `thir_mir_builder.cpp:build_let_stmt`, two consecutive `if (let.pattern->is<ThirBindingPattern>())` blocks. The first (array fast-path, lines 535-609) handles ALL binding patterns and returns unconditionally at line 608 via `build_pattern_binding(let.pattern, init_val); return;`. The second block (mutable struct alloca, lines 614-643) was dead code — never reachable.

**Why:** The mutable struct alloca code was added AFTER the array fast-path but placed as a SEPARATE if-block instead of being merged INTO the existing binding pattern block.

**Fix:** Moved the `if (bp.is_mut && init_val.type && init_val.type->is_struct())` check inside the first binding pattern block, after the array check at line 590-605 and before the `build_pattern_binding` fallthrough.

**How to apply:** When adding new handling for specific types in `build_let_stmt`, always add it INSIDE the existing `ThirBindingPattern` block (lines 535-609), not as a new block after it. The pattern is: check for zero-array → check for array → check for mutable struct → fallthrough to build_pattern_binding.

**Files changed:**
- `compiler/src/mir/thir_mir_builder.cpp` (build_let_stmt)
- `compiler/src/mir/thir_mir_builder_expr.cpp` (build_assign — field assignment GEP+store, whole-struct reassignment store)

**Three related code paths for mutable struct vars:**
1. `build_let_stmt` — creates alloca, registers in `mut_struct_vars`
2. `build_var` — loads from alloca when reading the variable
3. `build_assign` — GEP+store for field assignment, direct store for whole-struct reassignment
