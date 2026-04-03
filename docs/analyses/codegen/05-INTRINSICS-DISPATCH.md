# Intrinsic Dispatch: TML vs Established Compilers

## The Problem

TML's `instructions_call.cpp` is **1,357 lines** of if/else chains matching function names as strings. Every memory intrinsic (`ptr_read`, `ptr_write`, `ptr_offset`, `copy_nonoverlapping`, `mem_free`, `memcpy`, `memmove`, `memset`, `mem_zero`, `write_bytes`, `mem_copy`, `mem_move`), every math intrinsic (`sqrt`, `sin`, `cos`, `log`, etc.), and every inline method (`to_string`, `debug_string`, `len`, `hash`, `eq`, `partial_cmp`, `cmp`) is handled by string comparison in a single massive function.

### Evidence

```cpp
// instructions_call.cpp — 45+ special-case string matches:
if (base_name == "ptr_write" && i.args.size() >= 2) { ... }    // 50 lines
if (base_name == "ptr_read" && i.args.size() >= 1) { ... }     // 50 lines
if (base_name == "ptr_read_volatile" || ...) { ... }            // 50 lines
if (base_name == "ptr_write_volatile" || ...) { ... }           // 40 lines
if (base_name == "ptr_read_unaligned" && ...) { ... }           // 50 lines
if (base_name == "ptr_write_unaligned" && ...) { ... }          // 40 lines
if (base_name == "ptr_offset" && ...) { ... }                   // 30 lines
if (base_name == "mem_free" && ...) { ... }                     // 20 lines
if (base_name == "copy_nonoverlapping" && ...) { ... }          // 30 lines
if (base_name == "memcpy" || base_name == "mem_copy" && ...) { ... } // 40 lines
if (base_name == "memmove" || base_name == "mem_move" || ...) { ... } // 40 lines
if (base_name == "memset" || base_name == "mem_set" || ...) { ... }  // 80 lines
// ... plus Char::to_string, Str::to_string, Bool::to_string
// ... plus bare to_string / debug_string with type detection
// ... plus black_box, store_byte
```

### Problems with This Approach

1. **Duplication**: `ptr_read`, `ptr_read_volatile`, `ptr_read_unaligned` share 90% of their logic but are 3 separate 50-line blocks
2. **Fragile**: Adding a new name alias (e.g., `volatile_read` alongside `ptr_read_volatile`) requires finding and duplicating the block
3. **Type guessing**: Each intrinsic has its own 5-level type resolution chain (type_args → value_types_ → arg.type → arg_types → i32 fallback)
4. **Hard to audit**: 1,357 lines of sequential if/else — missing a case means a function call to a non-existent symbol

### The `ensure_ptr` Anti-Pattern

The same pointer-coercion pattern is copied 12+ times:

```cpp
// This lambda appears in copy_nonoverlapping, memcpy, memmove, memset, ...
auto ensure_ptr = [&](const mir::Value& v, std::string& reg) {
    auto vt = value_types_.find(v.id);
    std::string vtype;
    if (vt != value_types_.end()) vtype = vt->second;
    else if (v.type) vtype = mir_type_to_llvm(v.type);
    if (vtype.size() > 0 && vtype[0] == 'i' && vtype != "i1") {
        std::string id = std::to_string(temp_counter_++);
        std::string conv = "%itp.XX." + id;
        emitln("    " + conv + " = inttoptr " + vtype + " " + reg + " to ptr");
        reg = conv;
    }
};
```

## How Rust Handles Intrinsics

### Enum-Based Dispatch

rustc identifies intrinsics by their `DefId` (a unique identifier assigned during name resolution), not by string matching at codegen time:

```rust
fn codegen_intrinsic_call(
    &mut self,
    instance: Instance<'tcx>,  // Typed, resolved function reference
    args: &[OperandRef<'tcx>],
    // ...
) {
    let name = self.tcx.item_name(instance.def_id());
    match name {
        sym::size_of => self.const_usize(layout.size.bytes()),
        sym::copy_nonoverlapping => self.memcpy(dst, src, size, align),
        sym::transmute => { /* ... */ },
        // Each intrinsic has a dedicated handler
    }
}
```

Key differences from TML:
- **Types are guaranteed** — `args[0]` has a known type from OperandRef
- **No string prefix matching** — Symbol comparison is interned
- **Dedicated handlers** — Each intrinsic is a separate method, not inline code
- **No type guessing** — The type comes from the MIR, always correct

### Intrinsic Categories are Explicit

rustc categorizes intrinsics at the type-checking stage, not at codegen:

```rust
enum IntrinsicDef {
    Copy,           // copy_nonoverlapping
    WriteBytes,     // write_bytes (memset)
    Transmute,      // transmute
    SizeOf,         // size_of
    AlignOf,        // align_of
    // ... exhaustive enum, no string matching
}
```

## How Go Handles Intrinsics

### SSA Rewrite Rules

Go handles intrinsics via **rewrite rules** in the SSA, not in the codegen. Intrinsics are lowered to machine operations before the final codegen sees them:

```go
// Generic SSA → architecture-specific SSA via rewrite rules
(Sqrt x) => (FSQRT x)           // On ARM64
(Copy dst src len) => ...        // Lowered to REP MOVSB or equivalent
```

The codegen only handles machine-level operations — it never sees `copy_nonoverlapping` as a function name.

## How Clang Handles Intrinsics

### Builtin Recognition at Sema Stage

Clang recognizes builtins during semantic analysis (`Sema`), not during codegen:

```cpp
// Each builtin has an ID assigned at compile time
enum BuiltinID {
    BI__builtin_memcpy,
    BI__builtin_memset,
    BI__builtin_sqrt,
    // ... hundreds of builtins
};
```

The codegen dispatches on this ID:

```cpp
Value *EmitBuiltinExpr(const GlobalDecl &GD, unsigned BuiltinID, ...) {
    switch (BuiltinID) {
    case Builtin::BI__builtin_memcpy:
        return Builder.CreateMemCpy(Dest, Src, Size, /*isVolatile=*/false);
    case Builtin::BI__builtin_sqrt:
        return Builder.CreateUnaryIntrinsic(Intrinsic::sqrt, Op);
    // ...
    }
}
```

Key: Clang uses `IRBuilder::CreateMemCpy()` — a typed API that handles all the alignment/volatile/type details internally. TML emits the raw LLVM IR string.

## What TML Must Change

### 1. Table-Driven Intrinsic Dispatch

Replace the if/else chain with a dispatch table:

```cpp
// compiler/include/codegen/intrinsic_table.hpp

enum class IntrinsicKind {
    // Memory operations
    PtrRead,        // ptr_read[T](ptr) → T
    PtrWrite,       // ptr_write[T](ptr, val)
    PtrReadVolatile,
    PtrWriteVolatile,
    PtrReadUnaligned,
    PtrWriteUnaligned,
    PtrOffset,      // ptr_offset[T](ptr, offset) → *T
    MemFree,        // mem_free(ptr)
    CopyNonOverlapping,  // copy_nonoverlapping(src, dst, count)
    Memcpy,         // memcpy/mem_copy
    Memmove,        // memmove/mem_move/copy
    Memset,         // memset/mem_set/mem_zero/write_bytes

    // Math intrinsics
    Sqrt, Sin, Cos, Log, Exp, Pow, Floor, Ceil, Round, Trunc,
    Fma, Fabs, Minnum, Maxnum, Copysign,

    // String methods
    CharToString, StrToString, BoolToString,

    // Utility
    BlackBox, StoreByte,

    // Not an intrinsic — emit normal function call
    None,
};

struct IntrinsicInfo {
    IntrinsicKind kind;
    int min_args;       // Minimum argument count
    bool has_result;    // Whether it produces a result value
};

// Lookup: O(1) via unordered_map, populated once at startup
IntrinsicInfo lookup_intrinsic(const std::string& func_name);
```

### 2. Shared Type Resolution

Extract the "resolve element type from type_args / value_types_ / arg.type" logic into a single helper:

```cpp
// Used by ALL ptr_read/ptr_write/ptr_offset variants
std::string resolve_element_type(
    const mir::CallInst& call,
    size_t arg_index,           // Which arg has the pointer
    const std::string& default_type = "i32"
) {
    // 1. Explicit type argument [T]
    if (!call.type_args.empty() && call.type_args[0]) {
        std::string ta = mir_type_to_llvm(call.type_args[0]);
        if (ta != "void") return ta;
    }
    // 2. Pointee type from pointer argument
    // 3. Return type
    // 4. Default
    // ... ONE implementation, used by 12+ intrinsics
}
```

### 3. Shared Pointer Coercion

Replace the 12 copies of `ensure_ptr` with a single method:

```cpp
// One method, used everywhere
std::string ensure_ptr_value(const mir::Value& v) {
    std::string reg = get_value_reg(v);
    std::string vtype = get_value_type(v);  // Uses CGValue, no side-table
    if (is_integer_type(vtype)) {
        std::string conv = new_temp();
        emitln("    " + conv + " = inttoptr " + vtype + " " + reg + " to ptr");
        return conv;
    }
    return reg;
}
```

### 4. Emit Helpers for Common Patterns

```cpp
// Instead of hand-writing LLVM IR for memcpy:
void emit_memcpy(const std::string& dst, const std::string& src,
                 const std::string& size, bool is_volatile = false) {
    emitln("    call void @llvm.memcpy.p0.p0.i64(ptr " + dst +
           ", ptr " + src + ", i64 " + size +
           ", i1 " + (is_volatile ? "true" : "false") + ")");
}

void emit_memset(const std::string& dst, const std::string& val,
                 const std::string& size) {
    emitln("    call void @llvm.memset.p0.i64(ptr " + dst +
           ", i8 " + val + ", i64 " + size + ", i1 false)");
}
```

### Impact Assessment

| Change | Lines Removed | Lines Added | Risk |
|--------|--------------|-------------|------|
| Dispatch table | ~200 (if/else) | ~80 (table + enum) | Low |
| Shared type resolution | ~300 (duplicated) | ~40 (one function) | Medium |
| Shared ensure_ptr | ~120 (12 copies) | ~15 (one method) | Low |
| Emit helpers | ~60 (raw strings) | ~30 (helper calls) | Low |
| **Total** | **~680 lines** | **~165 lines** | **Medium** |

Net reduction: ~515 lines from `instructions_call.cpp` alone, making it about 40% smaller and far easier to audit.

### Comparison Table

| Aspect | Rust | Go | Clang | TML (current) |
|--------|------|-----|-------|---------------|
| Intrinsic identification | DefId (typed) | SSA Op enum | BuiltinID enum | **String match** |
| Dispatch mechanism | match on symbol | Rewrite rules | switch on ID | **if/else chain** |
| Type resolution | From OperandRef | From Value.Type | From QualType | **5-level fallback** |
| Duplication | None | None | Minimal | **12+ copies of ensure_ptr** |
| Adding new intrinsic | Add match arm | Add rewrite rule | Add switch case | **Copy 50-line block** |
| LOC for intrinsics | ~500 | ~300 (rules) | ~800 | **1,357** |
