# Proposal: phase31b_native-class-oop-codegen (renumbered from phase32b, 2026-07-15 ERA 0 pivot)

## Why
TML supports `class` types with single inheritance, virtual methods, and
constructor chaining. These features are used throughout the compiler's
own diagnostic and IR node hierarchies. The native backend currently has
no codegen for class layout, vtable construction, virtual dispatch, or
the `instanceof` operator: any program that uses a class type falls back
to LLVM. Without this support the native backend cannot compile the
compiler itself, which is required for the self-hosting milestone. The
implementation follows the standard Itanium ABI layout: a class struct
embeds the base struct as its first field (enabling zero-cost upcasts),
and the first field of the base struct is a pointer to the vtable, which
is a static array of function pointers in declaration order.

## What Changes
- `compiler-tml/src/native/mir_lower.tml` and
  `compiler-tml/src/native/pipeline.tml` gain class-specific lowering:
  - Class struct layout: the derived struct begins with the base struct
    embedded as its first field; virtual-method fields follow the base.
  - Vtable emission: for each concrete class, a static constant array of
    function pointers is emitted, one slot per virtual method in
    declaration order (matching the base class slot assignments).
  - Constructor chaining: the derived constructor emits a call to the
    base constructor as its first instruction, then initialises derived
    fields.
  - Virtual dispatch: a `VirtualCall` MIR instruction is lowered to
    load the vtable pointer from offset 0 of the object, GEP to the
    correct slot index, load the function pointer, and emit an indirect
    call.
  - Instanceof check: emits a comparison of the object's vtable pointer
    against the target class's known vtable address.

## Impact
- Affected specs: native-backend/classes
- Affected code: compiler-tml/src/native/mir_lower.tml, compiler-tml/src/native/pipeline.tml
- Breaking change: NO
- User benefit: Class types with inheritance and virtual methods compile and run natively, removing the last major OOP construct that required LLVM.
