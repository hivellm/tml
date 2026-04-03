# Tasks: CGValue Wrapper — Typed Values with Pointer/Value Distinction

**Status**: In progress. 80% (16/20). **Priority**: HIGH — depends on phase0a
**Reference**: `docs/analyses/codegen/04-VALUE-REPRESENTATION.md`

## 1. CGValue Type Design

- [x] 1.1 Create `compiler/include/codegen/cg_value.hpp` — already exists
- [x] 1.2 Add `CGValueKind` enum: `Immediate`, `Address`, `FatPointer`, `ZeroSized` — already exists
- [x] 1.3 Add `CGValue` struct: `{reg, llvm_type, kind, mir_type}` — already exists
- [x] 1.4 Add `is_aggregate()`, `is_pointer()`, `is_zero_sized()` helpers — already exists
- [x] 1.5 Add `to_address()` → `cg_to_address()` free function — already exists
- [x] 1.6 Add `to_immediate()` → `cg_to_immediate()` free function — already exists
- [x] 1.7 Build — verified header compiles

## 2. Emit Integration (Transition)

- [x] 2.1 Add `cg_values_` map to MirCodegen: `ValueId → CGValue` — in mir_codegen.hpp
- [x] 2.2 In `emit_instruction()`: populate `cg_values_` alongside `value_regs_` for all major instruction types
- [x] 2.3 `AllocaInst` → CGValue::address (kind=Address, pointee_type=alloc_type)
- [x] 2.4 `LoadInst` → CGValue::immediate (kind=Immediate)
- [x] 2.5 `ConstantInst` → CGValue::immediate for int/float/bool/string, zero_sized for unit
- [x] 2.6 `StructInitInst` → CGValue::immediate (aggregate value)
- [x] 2.7 Build + run `core/str` 25/25 — dual-tracking verified

Also populated: GEP (Address), BinaryInst, ExtractValue, InsertValue (via Extract), TupleInit, ArrayInit, CastInst (7 paths), PhiInst, SelectInst (via type), AwaitInst, ClosureInit, MakeDynObject, AtomicLoad/RMW/CmpXchg, CallInst (sret/normal/intrinsic/indirect), MethodCallInst (all paths), inline array/ptr intrinsics.

## 3. Convert Call Sites

- [x] 3.1 In `emit_call_inst()`: use `cg_values_` for arg type/kind lookup (prefer over value_types_)
- [x] 3.2 Replace `value_types_.find()` with `cg_values_.find()` fallback in call arg processing loop
- [x] 3.3 Use `CGValue::is_aggregate()` for aggregate spill detection (alongside string-based check)
- [x] 3.4 Run `core/str` 25/25 + `core/fmt` 46/46 + `core/num` 53/53 — no regressions. `std/json` has 2 pre-existing compile errors (module resolution bugs, not related).

## 4. Remove value_types_ (final)

- [ ] 4.1 Replace remaining `value_types_` reads with `cg_values_` reads
- [ ] 4.2 Remove `value_types_` map from MirCodegen
- [ ] 4.3 Remove `value_spill_allocas_` map (replaced by CGValue::to_address)
- [ ] 4.4 Run full test suite — verify zero regressions
