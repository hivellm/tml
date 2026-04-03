# Tasks: CGValue Wrapper — Typed Values with Pointer/Value Distinction

**Status**: New. 0% (0/20). **Priority**: HIGH — depends on phase0a
**Reference**: `docs/analyses/codegen/04-VALUE-REPRESENTATION.md`

## 1. CGValue Type Design

- [ ] 1.1 Create `compiler/include/codegen/cg_value.hpp`
- [ ] 1.2 Add `CGValueKind` enum: `Immediate`, `Address`, `FatPointer`, `ZeroSized`
- [ ] 1.3 Add `CGValue` struct: `{reg, llvm_type, kind, mir_type}`
- [ ] 1.4 Add `is_aggregate()`, `is_pointer()`, `is_zero_sized()` helpers
- [ ] 1.5 Add `to_address()` — spill value to alloca if not already a pointer
- [ ] 1.6 Add `to_immediate()` — load from pointer if not already a value
- [ ] 1.7 Build — verify header compiles

## 2. Emit Integration (Transition)

- [ ] 2.1 Add `cg_values_` map to MirCodegen: `ValueId → CGValue`
- [ ] 2.2 In `emit_instruction()`: populate `cg_values_` alongside `value_regs_`
- [ ] 2.3 `AllocaInst` → CGValue with kind=Address
- [ ] 2.4 `LoadInst` → CGValue with kind=Immediate (or struct kind)
- [ ] 2.5 `ConstantInst` → CGValue with kind=Immediate
- [ ] 2.6 `StructInitInst` → CGValue with kind=Immediate (aggregate value)
- [ ] 2.7 Build + run `core/str` — verify dual-tracking works

## 3. Convert Call Sites

- [ ] 3.1 In `emit_call_inst()`: use `cg_values_` for arg type/kind lookup
- [ ] 3.2 Replace `value_types_.find()` with `cg_values_.find()` in call arg processing
- [ ] 3.3 Use `CGValue::to_address()` for indirect passing (replaces manual spill)
- [ ] 3.4 Run `core/str` + `core/fmt` + `std/json` — verify no regressions

## 4. Remove value_types_ (final)

- [ ] 4.1 Replace remaining `value_types_` reads with `cg_values_` reads
- [ ] 4.2 Remove `value_types_` map from MirCodegen
- [ ] 4.3 Remove `value_spill_allocas_` map (replaced by CGValue::to_address)
- [ ] 4.4 Run full test suite — verify zero regressions
