# Tasks: Centralized ABI Module — Eliminate Scattered Struct Passing

**Status**: In progress. 75% (15/20). **Priority**: HIGH
**Reference**: `docs/analyses/codegen/03-ABI-CALLING-CONVENTION.md`

## 1. ABI Types & Interface

- [x] 1.1 Create `compiler/include/codegen/abi.hpp` — PassMode enum, ArgABI, FnABI structs
- [x] 1.2 Add `PassMode::Direct`, `Indirect`, `Ignore`, `Pair` variants
- [x] 1.3 Add `ArgABI` — `{mode, llvm_type, sret}` per argument
- [x] 1.4 Add `FnABI` — `{ret: ArgABI, args: vector<ArgABI>}`
- [x] 1.5 Build — verify header compiles clean

## 2. ABI Computation (Win64)

- [x] 2.1 Create `compiler/src/codegen/abi.cpp`
- [x] 2.2 Implement `classify_type()` — aggregate detection via MirType (not string prefix)
- [x] 2.3 Implement `classify_return()` — sret for large returns, void for Unit
- [x] 2.4 Implement `compute_fn_abi(mir::Function)` — computes FnABI for a function
- [x] 2.5 Handle `this`/`self` parameter → always Indirect for aggregates
- [x] 2.6 Handle fat pointers (slice, dyn, closure) → PassMode::Pair
- [x] 2.7 Build + unit test: verify FnABI matches current behavior for 10 representative functions

## 3. Integration — Replace String Checks with `is_aggregate_llvm_type()`

- [x] 3.1 Add `is_aggregate_llvm_type()` transitional helper to abi.hpp/abi.cpp
- [x] 3.2 Replace `find("%struct.")` in instructions_misc.cpp (2 sites: cast-to-ptr spill, bitcast upcast)
- [x] 3.3 Replace `find("%struct.")` in instructions_method.cpp (1 site: receiver aggregate detection)
- [x] 3.4 Replace `find("%struct.")/find("%enum.")/find("%class.")/find("%union.")` in instructions_call.cpp (1 site: Win64 ABI arg spill)
- [x] 3.5 Replace `starts_with("%struct.")/starts_with("%enum.")/starts_with("%class.")/starts_with("%union.")` in instructions.cpp (1 site: extractvalue ptr-to-aggregate GEP)
- [x] 3.6 Verified zero raw `%struct.` string checks remain in `compiler/src/codegen/mir/`
- [x] 3.7 Run core/str (25/25), core/fmt (46/46), std/http (161/161) — zero regressions

## 4. Integration — FnABI Cache & Call Site Migration

- [x] 4.1 Add `fn_abi_cache_` map to MirCodegen class (func_name → FnABI)
- [ ] 4.2 In `emit_call_inst()`: look up callee FnABI, use `mode` for spill decisions
- [ ] 4.3 In `emit_function()`: store FnABI, use for self/this handling
- [ ] 4.4 Remove `func_param_types_` map (replaced by fn_abi_cache_)
- [ ] 4.5 Run full test suite — verify zero regressions
