# Proposal: phase31c_native-builtins-reflect-dyncall

## Why
TML's plugin system, test framework, and serialization layer all depend on runtime
reflection: knowing a type's name at runtime (`type_name[T]()`), how many fields
a struct has (`field_count[T]()`), and what each field is named (`field_name[T](i)`).
Dynamic FFI calls are needed by the C-interop layer and the plugin loader to invoke
functions through pointers discovered at runtime. Both capabilities fall back to the
LLVM backend today, blocking the native backend from running the test harness itself.
Without reflection the native compiler cannot self-host because the compiler uses
type_name for error messages and field_count for derive-generated Display impls.

## What Changes
- New `compiler-tml/src/native/x86/reflect.tml`: emits static `TypeDescriptor`
  records (name: *const u8, field_count: i64, fields: *const FieldDescriptor) into
  the `.rodata` section for each type that uses `@reflect` or any Reflect-derived
  behavior; `type_name[T]()` and `field_name[T](i)` intrinsics load from these records.
- New `compiler-tml/src/native/x86/dyncall.tml`: emits an indirect CALL through a
  function pointer with a caller-constructed argument frame; supports the System V
  AMD64 ABI (integer args in RDI/RSI/RDX/RCX/R8/R9, FP args in XMM0-7, rest on stack).

## Impact
- Affected specs: native-backend/reflect, native-backend/dyncall
- Affected code: compiler-tml/src/native/x86/reflect.tml (new), compiler-tml/src/native/x86/dyncall.tml (new)
- Breaking change: NO
- User benefit: Reflection intrinsics and dynamic FFI calls work natively, enabling the test framework, plugin loader, and self-hosting compiler to run without the LLVM backend.
