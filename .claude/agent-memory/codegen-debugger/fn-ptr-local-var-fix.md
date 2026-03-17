---
name: fn-ptr-local-var-fix
description: Fix for function pointer in local variable generating direct call instead of indirect call (let f = add100; f(42))
type: project
---

## Function Pointer Local Variable Indirect Call Fix (2026-03-17)

**Bug**: `let f: func(I32) -> I32 = add100; f(42)` generated `call i32 @"f"(i32 42)` — treating `f` as a global function symbol instead of an indirect call through the local variable.

### Root Causes (3 issues)

1. **THIR builder `build_call`** (thir_mir_builder.cpp:788): Did not distinguish between `f(42)` where `f` is a function name vs a local variable holding a function pointer. Always created `CallInst{func_name="f"}`.

2. **MIR DCE pass** (mir_pass.cpp:754): `is_value_used()` for `CallInst` only checked `i.args`, not `i.callee`. The ConstFuncRef result had no users in MIR operands (the link was only in THIR builder's `ctx_.variables`), so DCE eliminated it.

3. **MIR codegen local var detection** (instructions.cpp:1037-1052): Searched `value_regs_` for `"%f"` but the register was `"%v<N>"` — name mismatch. Also passed `nullptr` as `func_type` to `emit_indirect_call` which would crash.

### Fix (4 files)

1. **mir.hpp**: Added `std::optional<Value> callee` and `MirTypePtr callee_func_type` fields to `CallInst`. When set, codegen emits indirect call through that value.

2. **thir_mir_builder.cpp:build_call**: After creating `CallInst`, checks if `func_name` matches a local variable in `ctx_.variables` with `MirFunctionType`. If so, sets `callee` and `callee_func_type`.

3. **instructions_misc.cpp:ConstFuncRef**: Changed from storing bare `ptr` (`@add100`) to emitting `{ ptr, ptr }` fat pointer via `insertvalue` (matching closure calling convention).

4. **mir_pass.cpp:is_value_used**: Added check for `CallInst.callee` field so DCE doesn't eliminate the ConstFuncRef instruction.

5. **instructions.cpp:emit_indirect_call**: Changed `fat_ptr` resolution to use `value_regs_[value_id]` lookup instead of `"%" + param_name` (which only worked for parameters, not local variables).

### Key Insight
The `ConstFuncRef` → `{ ptr, ptr }` fat pointer change is critical. Without it, `emit_indirect_call` expects a fat pointer but gets a bare pointer, causing extractvalue to fail. The fat pointer convention `{ fn_ptr, env_ptr }` with `env_ptr = null` for plain function references unifies the calling convention with closures.
