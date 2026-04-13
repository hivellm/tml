# Proposal: phase32c_native-dyn-dispatch-vtable

## Why
TML's trait objects (`dyn Behavior`) are used heavily in the standard
library for iterators (`dyn Iterator[T]`), formatters (`dyn Display`),
error types (`dyn Error`), and async executors (`dyn Future`). The
native backend has stub entries for `MakeDyn` and `DynCall` MIR
instructions but they emit no code, causing a link failure whenever a
trait object is constructed or called. Without working `dyn` dispatch,
the native backend cannot compile any program that uses closures stored
in collections, heterogeneous error types, or the iterator adapter chain.
This is the last major type-system feature blocking a usable native backend.

## What Changes
- `compiler-tml/src/codegen/emit_method.tml` gains full vtable emission:
  - Vtable struct layout: `{ drop_fn: I64, size: I64, align: I64,
    method_0: I64, method_1: I64, ... }` where each method slot holds
    the address of the concrete implementation cast to `I64`.
  - Vtable constant emission: for each `(ConcreteType, Behavior)` pair
    encountered at a `MakeDyn` site, a static constant struct is emitted
    named `<Type>_as_<Behavior>__vtable`.
  - `make_dyn` lowering: `MakeDyn { value, behavior }` is lowered to
    allocating a fat pointer `{ data_ptr: I64, vtable_ptr: I64 }` on the
    stack (or heap if stored), writing `data_ptr = &value` and
    `vtable_ptr = &<Type>_as_<Behavior>__vtable`.
  - `dyn call` lowering: `DynCall { fat_ptr, method_index, args }` is
    lowered to loading `vtable_ptr` from offset 8 of the fat pointer,
    GEP to `method_index * 8 + 24` (skipping drop/size/align), loading
    the function pointer, and emitting an indirect call with `data_ptr`
    prepended to args.

## Impact
- Affected specs: native-backend/dyn-dispatch
- Affected code: compiler-tml/src/codegen/emit_method.tml
- Breaking change: NO
- User benefit: Trait objects (dyn Iterator, dyn Display, dyn Error, dyn Future) compile and dispatch correctly through the native backend.
