## 1. Implementation
- [ ] 1.1 Add `CastKind::Bitcast` in `emit_inst.tml`: emit LLVM `bitcast` for same-size type reinterpretation (e.g. `I64` to `RawPtr`, `F64` to `I64`); assert source and target have equal bit-width
- [ ] 1.2 Add `CastKind::EnumToInt` in `emit_inst.tml`: GEP to discriminant field, `load` its `i32`/`i64` value per `@repr`, zero-extend or truncate to the target integer width
- [ ] 1.3 Add `CastKind::IntToEnum` in `emit_inst.tml`: emit a discriminant range check against the enum's max variant index, call `__tml_panic` on out-of-range, store the validated integer as the discriminant into a stack-allocated enum slot and return it
- [ ] 1.4 Add `CastKind::TraitUpcast` in `emit_inst.tml`: extract data pointer and vtable pointer from the source wide pointer, replace vtable with the target behavior's global vtable constant, return the new wide pointer
- [ ] 1.5 Write `compiler-tml/tests/codegen/cast_conversion.test.tml` covering: `I64 as RawPtr` bitcast round-trip, enum discriminant extraction, integer-to-enum valid conversion, integer-to-enum out-of-range panic, behavior upcast preserves data pointer

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
