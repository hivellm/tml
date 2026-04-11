# Value Representation: TML vs Established Compilers

## The Problem

TML's codegen represents all values as **strings** (LLVM register names like `%v42`). The codegen has no way to know, from a register name alone, whether it holds:
- A direct value (e.g., `i32 42`)
- A pointer to a value (e.g., `ptr %alloca`)
- A fat pointer (e.g., `{ ptr, ptr }` for closures)
- A struct value (e.g., `%struct.Point { i32, i32 }`)

This ambiguity causes the codegen to emit wrong instructions. For example:
- Passing a struct value where a pointer is expected → LLVM verification error
- Loading from a value that's already loaded → double indirection
- Storing to a register instead of a memory location → crash

### The `value_types_` Workaround

To compensate, the codegen maintains a `value_types_` side-table (61 lookup sites across 5 files) that maps ValueId → LLVM type string. But this is:

1. **Incomplete** — Not all instructions update `value_types_`, so lookups can miss
2. **String-based** — Requires parsing strings like `"%struct.Point"` to detect aggregates
3. **Doesn't distinguish value vs pointer** — `value_types_[v] = "ptr"` doesn't tell you what it points to
4. **Doesn't distinguish rvalue vs lvalue** — Is `%v5` a loaded value or an alloca address?

### The Spill Pattern

When the codegen discovers (too late) that a value needs to be a pointer, it "spills" it:

```cpp
// This pattern appears 10+ times in instructions_call.cpp
std::string spill_ptr = "%spill" + std::to_string(spill_counter_++);
emitln("    " + spill_ptr + " = alloca " + actual_type);
emitln("    store " + actual_type + " " + arg + ", ptr " + spill_ptr);
arg = spill_ptr;
arg_type = "ptr";
```

This generates unnecessary allocas that LLVM's mem2reg pass usually removes, but:
- It inflates IR size (more instructions = slower compilation)
- It masks the real issue (the value should have been a pointer from the start)
- It sometimes fails (when the value is already a pointer, causing double indirection)

## How Rust Handles Value Representation

### OperandRef: Typed + Categorized

rustc uses `OperandRef` which carries both the LLVM value and metadata about what kind of value it is:

```rust
enum OperandValue {
    Ref(PlaceValue),          // Pointer to value in memory (lvalue)
    Immediate(llvm::Value),   // Direct SSA value (rvalue, fits in register)
    Pair(llvm::Value, llvm::Value), // Fat pointer (ptr + metadata)
    ZeroSized,                // Unit type, no value needed
}

struct OperandRef<'tcx, V> {
    val: OperandValue<V>,
    layout: TyAndLayout<'tcx>, // Full type info (size, align, ABI)
}
```

The codegen always knows:
- Is this a pointer to memory or a direct value?
- What's the full type (for ABI decisions)?
- Is this zero-sized (can be skipped)?

When passing a struct by pointer, rustc checks `OperandValue::Ref` — it's already a pointer. When passing by value, it checks `OperandValue::Immediate`. No spilling needed because the representation matches the ABI from the start.

### PlaceRef: Always a Pointer

For lvalues (memory locations), rustc uses `PlaceRef` which is **always** a pointer:

```rust
struct PlaceRef<'tcx, V> {
    val: PlaceValue<V>,        // Always a pointer
    layout: TyAndLayout<'tcx>,
}
```

There's no ambiguity — `PlaceRef` is always an address you can store to. `OperandRef` is always a value you can compute with. The distinction is in the type system, not in ad-hoc runtime checks.

## How Go Handles Value Representation

### Value with Explicit Type

Go's SSA values carry their type directly. But more importantly, the codegen tracks whether a value is "in a register" or "on the stack" through explicit `LocalSlot`:

```go
type LocalSlot struct {
    N    *ir.Name     // The variable
    Type *types.Type  // Its type
    Off  int64        // Offset within the slot
}
```

When the codegen needs a pointer to a value, it allocates a stack slot explicitly during register allocation — not as an afterthought during IR emission.

## How Clang Handles Value Representation

### RValue vs LValue: The Clean Split

Clang has the clearest distinction:

```cpp
class RValue {
    // Three kinds:
    // 1. Scalar: a single llvm::Value* (int, float, ptr)
    // 2. Complex: pair of llvm::Value* (real, imag)
    // 3. Aggregate: an Address (pointer + type + alignment)
    
    llvm::Value *V1, *V2;
    enum { Scalar, Complex, Aggregate } Flavor;
};

class LValue {
    // ALWAYS an address in memory
    Address Addr;       // llvm::Value* (ptr) + element type + alignment
    QualType Type;      // The Clang type
    
    // Never ambiguous — if you have an LValue, you have an address
};

class Address {
    llvm::Value *Pointer;     // Always a pointer
    llvm::Type *ElementType;  // What it points to
    CharUnits Alignment;      // Guaranteed alignment
};
```

The key insight: when Clang's codegen converts an LValue to an RValue, it emits an explicit `load`. When it converts an RValue to an LValue (for passing by pointer), it emits an explicit `alloca` + `store`. The decision is made at a clear boundary, not scattered throughout.

## What TML Must Change

### Current Value Flow (Broken)

```
MIR instruction → emit_instruction() → returns string "%v42"
  → caller doesn't know if %v42 is a value or pointer
  → looks up value_types_[42] → might find "ptr", might find "%struct.Point", might find nothing
  → if it's a struct and ABI needs ptr → spill to alloca (12+ lines of code)
  → if value_types_ didn't have entry → use i32 fallback → silent bug
```

### Target Value Flow (Correct)

```
MIR instruction → emit_instruction() → returns CGValue{reg="%v42", type="ptr", kind=Pointer}
  → caller knows it's a pointer, uses directly for by-ref call
  → OR returns CGValue{reg="%v42", type="%struct.Point", kind=Value}
  → caller knows it's a value, spills to alloca for by-ref call
  → kind=ZeroSized → caller skips entirely (Unit type)
```

### Concrete Design: CGValue

```cpp
// compiler/include/codegen/cg_value.hpp

enum class CGValueKind {
    Immediate,  // Direct SSA value (fits in register)
    Address,    // Pointer to memory location
    FatPointer, // { ptr, metadata } (slice, dyn, closure)
    ZeroSized,  // Unit type — no value, no register
};

struct CGValue {
    std::string reg;            // LLVM register name (e.g., "%v42")
    std::string llvm_type;      // LLVM type string (e.g., "i64", "%struct.Point")
    CGValueKind kind;           // What this value IS
    mir::MirTypePtr mir_type;   // Original MIR type (for ABI queries)

    // Convenience methods
    bool is_pointer() const { return kind == CGValueKind::Address; }
    bool is_aggregate() const {
        return llvm_type.starts_with("%struct.") || llvm_type.starts_with("%enum.");
    }
    bool is_zero_sized() const { return kind == CGValueKind::ZeroSized; }

    // Convert to pointer if not already (for by-ref passing)
    // Returns a new CGValue with kind=Address
    CGValue to_address(MirCodegen& cg) const;

    // Convert to immediate if currently a pointer (load)
    // Returns a new CGValue with kind=Immediate
    CGValue to_immediate(MirCodegen& cg) const;
};
```

### Migration Path

This can be introduced **incrementally**:

1. **Phase 1**: Add CGValue struct, make `emit_instruction()` return CGValue (while still populating `value_types_` for backward compat)
2. **Phase 2**: Convert call sites to use CGValue instead of `value_types_` lookups
3. **Phase 3**: Remove `value_types_` side-table entirely
4. **Phase 4**: Make CGValue the only way to reference values in codegen

### Comparison Table

| Aspect | Rust | Go | Clang | TML (current) |
|--------|------|-----|-------|---------------|
| Value representation | OperandRef (typed enum) | Value (typed struct) | RValue/LValue (classes) | **String** (register name) |
| Value/pointer distinction | OperandValue enum | LocalSlot | RValue vs LValue | **None** (ad-hoc checks) |
| Type carried with value | Yes (TyAndLayout) | Yes (Type field) | Yes (QualType) | **No** (side-table lookup) |
| Zero-sized handling | ZeroSized variant | Implicit skip | ABIArgInfo::Ignore | **Ad-hoc void checks** |
| "Need pointer" conversion | Clear boundary | Stack slot allocation | LValue→store | **Scattered spill pattern** |
| Fat pointer representation | Pair variant | Explicit itab | Address with metadata | **String check "{ ptr, ptr }"** |
