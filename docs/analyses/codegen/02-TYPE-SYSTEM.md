# Type System in Codegen: TML vs Established Compilers

## The Problem

TML's codegen has **23 sites** that fall back to `make_i32_type()` when type information is missing, and **61 sites** that look up types from the `value_types_` side-table. This means the codegen regularly operates without knowing the correct type of a value, leading to:

- Wrong LLVM IR instructions (e.g., `add i32` when the value is `i64`)
- Wrong ABI decisions (e.g., passing a struct by value instead of by pointer)
- Silent data corruption (e.g., truncating a 64-bit value to 32 bits)
- Crashes at LLVM verification time with cryptic "type mismatch" errors

## How Rust Handles Types

### Guaranteed Types at Codegen

In rustc, **every MIR operand has a known, monomorphized type before codegen begins**. The monomorphization pass replaces all generic type parameters with concrete types. By the time the codegen sees a `_1: Vec<i32>`, it knows it's `Vec<i32>` — never `Vec<T>`.

```
// rustc's MIR (after monomorphization)
_1 = move _2 as Vec<i32> (Unsize)    // Type is KNOWN
_3 = &_1                              // Type is &Vec<i32>, KNOWN
_4 = <Vec<i32> as Index<usize>>::index(move _3, const 0_usize) // KNOWN
```

The codegen (`compiler/rustc_codegen_llvm/src/`) never needs to guess types. The `FunctionCx` holds a `TyCtxt` (type context) that can answer any type query instantly. There are no fallback chains.

### Type Conversion is Centralized

rustc has a dedicated `type_of` module (`rustc_codegen_llvm/src/type_of.rs`) that converts Rust types to LLVM types. This is a **pure function** — same Rust type always produces the same LLVM type. The conversion is memoized.

```rust
// rustc: type conversion is deterministic and memoized
fn llvm_type(&self, ty: Ty<'tcx>) -> &'ll Type {
    match ty.kind() {
        ty::Bool => self.type_i1(),
        ty::Char => self.type_i32(),
        ty::Int(t) => self.isize_ty(t),
        ty::Adt(def, substs) => self.layout_of(ty).llvm_type(self),
        // ... exhaustive match, no fallback
    }
}
```

### What TML Does Differently

TML's `mir_type_to_llvm()` converts MIR types to **strings**. When a MIR instruction doesn't carry type information (which happens often), the codegen falls back to `i32`:

```cpp
// TML: 23 sites like this
mir::MirTypePtr type_ptr = i.result_type ? i.result_type : mir::make_i32_type();
```

This means the codegen silently generates wrong IR for any instruction where the MIR builder failed to annotate the type. The i32 fallback masks the real problem — a missing type in the MIR — and converts a "would crash" bug into a "silently wrong" bug.

## How Go Handles Types

### Types Embedded in SSA Values

In Go's SSA, every `Value` carries its type directly:

```go
type Value struct {
    Type types.Type    // ALWAYS present, NEVER nil
    Op   Op
    Args []*Value
    // ...
}
```

The type is set at SSA construction time and is immutable. The codegen never needs to look up types from a side-table — it reads `v.Type` directly. There is no equivalent of TML's `value_types_` map.

### No Fallbacks

If a Go SSA value somehow had a nil type, the compiler would panic at SSA construction time, long before codegen. This is by design — the SSA builder is responsible for type correctness, not the codegen.

## How Clang Handles Types

### CodeGenTypes: Centralized Type Cache

Clang has a dedicated `CodeGenTypes` class that maps Clang AST types to LLVM types. The mapping is cached and deterministic. The codegen never operates on untyped values.

### CGValue: Typed Value Wrappers

Clang wraps all LLVM values in `RValue` and `LValue` classes that carry both the LLVM value and its type. The codegen never holds a raw `llvm::Value*` without knowing its type:

```cpp
class RValue {
    llvm::Value *V1;    // The actual value
    // Carries: whether it's a scalar, aggregate, or complex
};

class LValue {
    llvm::Value *V;     // Always a pointer to the value
    QualType Type;      // The Clang type (for ABI decisions)
    // Never ambiguous: LValue is ALWAYS a pointer
};
```

## What TML Must Change

### Problem 1: MIR Values Often Lack Types

The MIR builder (both HIR→MIR and THIR→MIR paths) doesn't always set `inst.type` on every instruction. This is the root cause — the codegen is compensating for incomplete MIR.

**Fix**: Make `inst.type` **required** (not optional) on all MIR instructions. Add a MIR validation pass after building that asserts every instruction has a non-null type. Fail loudly at MIR construction time instead of silently at codegen time.

### Problem 2: Types Stored in 4+ Side-Tables

The codegen tracks types in:
1. `value_types_` (ValueId → LLVM type string)
2. `value_regs_` (ValueId → LLVM register string)
3. `struct_field_types_` (struct name → field type strings)
4. `func_param_types_` (func name → param MirTypePtrs)
5. `sret_functions_` (func name → return type string)

**Fix**: Introduce a `CGValue` wrapper that holds `{llvm_reg: string, llvm_type: string, mir_type: MirTypePtr}`. Every instruction emission returns a CGValue instead of a raw string. The side-tables become unnecessary.

### Problem 3: i32 Fallback Masks Bugs

Every `make_i32_type()` fallback is a potential silent data corruption. In 23 sites, missing type info produces `i32` instead of crashing.

**Fix**: Replace `make_i32_type()` fallbacks with assertions that crash the compiler with a clear error message: "BUG: instruction at line N has no type annotation". This makes MIR builder bugs immediately visible instead of manifesting as mysterious runtime failures.

### Comparison Table

| Aspect | Rust (rustc) | Go (gc) | Clang | TML (current) |
|--------|-------------|---------|-------|---------------|
| Type guarantee before codegen | Yes (monomorph) | Yes (SSA builder) | Yes (Sema) | **No** (23 fallbacks) |
| Type stored where | In TyCtxt, memoized | In Value.Type | In QualType | 4+ side-tables |
| Fallback for missing type | Compile error | Panic | Compile error | **Silent i32** |
| Type conversion is | Pure function | Direct field | Cached class | String concat |
| Type = string? | No (llvm::Type*) | No (types.Type) | No (llvm::Type*) | **Yes** (string) |
