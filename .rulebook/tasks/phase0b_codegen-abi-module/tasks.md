# Tasks: Centralized ABI Module — Eliminate Scattered Struct Passing

**Status**: New. 0% (0/20). **Priority**: HIGH
**Reference**: `docs/analyses/codegen/03-ABI-CALLING-CONVENTION.md`

## 1. ABI Types & Interface

- [ ] 1.1 Create `compiler/include/codegen/abi.hpp` — PassMode enum, ArgABI, FnABI structs
- [ ] 1.2 Add `PassMode::Direct`, `Indirect`, `Ignore`, `Pair` variants
- [ ] 1.3 Add `ArgABI` — `{mode, llvm_type, sret}` per argument
- [ ] 1.4 Add `FnABI` — `{ret: ArgABI, args: vector<ArgABI>}`
- [ ] 1.5 Build — verify header compiles clean

## 2. ABI Computation (Win64)

- [ ] 2.1 Create `compiler/src/codegen/abi.cpp`
- [ ] 2.2 Implement `classify_argument()` — aggregate detection via MirType (not string prefix)
- [ ] 2.3 Implement `classify_return()` — sret for large returns, void for Unit
- [ ] 2.4 Implement `compute_fn_abi(mir::Function)` — computes FnABI for a function
- [ ] 2.5 Handle `this`/`self` parameter → always Indirect for aggregates
- [ ] 2.6 Handle fat pointers (slice, dyn, closure) → PassMode::Pair
- [ ] 2.7 Build + unit test: verify FnABI matches current behavior for 10 representative functions

## 3. Integration — Declarations

- [ ] 3.1 In `emit_function_declaration()`: compute FnABI, use for parameter types
- [ ] 3.2 Remove `starts_with("%struct.")` checks from declaration path
- [ ] 3.3 Run `core/str` + `std/json` — verify no regressions

## 4. Integration — Call Sites

- [ ] 4.1 Add `fn_abi_cache_` map to MirCodegen (func_name → FnABI)
- [ ] 4.2 In `emit_call_inst()`: look up callee FnABI, use `mode` for spill decisions
- [ ] 4.3 Remove `is_aggregate_value` string checks from call path
- [ ] 4.4 In `emit_function()`: store FnABI, use for self/this handling
- [ ] 4.5 Remove `func_param_types_` map (replaced by fn_abi_cache_)
- [ ] 4.6 Run full test suite — verify zero regressions
