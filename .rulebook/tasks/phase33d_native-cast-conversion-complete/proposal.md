# Proposal: phase33d_native-cast-conversion-complete

## Why
The native backend handles numeric widening and truncation casts but is missing several cast forms that appear throughout real TML programs: `bitcast` between same-size types (e.g. `I64` to `RawPtr`), enum discriminant extraction (`EnumType as I32`), integer-to-enum conversion for FFI boundaries, and behavior-object (vtable) upcasting. Without these, programs that cross ABI boundaries, deserialize data, or use dynamic dispatch fail to compile on the native backend with an unhandled cast error, blocking entire categories of real code.

## What Changes
- `emit_inst.tml`: add `CastKind::Bitcast` branch — emit LLVM `bitcast` instruction for same-size, different-type conversions.
- `emit_inst.tml`: add `CastKind::EnumToInt` branch — GEP to the discriminant field and `load i32` (or `i64` depending on `@repr`), then zero-extend if the target integer type is wider.
- `emit_inst.tml`: add `CastKind::IntToEnum` branch — emit a bounds check against the maximum discriminant, panic on out-of-range, store the integer value into a freshly allocated enum slot, return the enum pointer.
- `emit_inst.tml`: add `CastKind::TraitUpcast` branch — for behavior-object upcasting, extract the vtable pointer from the wide pointer and replace it with the target behavior's vtable from the global vtable table, preserving the data pointer.
- New test file `compiler-tml/tests/codegen/cast_conversion.test.tml` covering all four new cast kinds.

## Impact
- Affected specs: `compiler-tml/src/codegen/emit_inst.tml`
- Affected code: `compiler-tml/src/codegen/emit_inst.tml`
- Breaking change: NO
- User benefit: FFI code, serialization/deserialization, and behavior-polymorphic programs compile and run correctly on the native backend.
