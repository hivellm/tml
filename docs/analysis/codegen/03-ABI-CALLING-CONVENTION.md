# ABI & Calling Convention: TML vs Established Compilers

## The Problem

TML's codegen makes ABI decisions **at every call site** by checking string prefixes like `starts_with("%struct.")`. The Win64 ABI rules for struct passing are duplicated across 10+ locations in the codebase. When a new struct type is introduced (or an existing one changes), these scattered checks frequently miss cases, causing:

- Structs passed by value when Win64 requires by-pointer → crash
- sret mismatch between caller and callee → garbage return values
- `this`/`self` parameter not converted to pointer → type mismatch error
- Enum types treated as primitives → incorrect codegen

### Evidence from Codebase

In `mir_codegen.cpp` (function declarations):
```cpp
// Lines 635-638: Ad-hoc aggregate detection
if (param_type.starts_with("%struct.") || param_type.starts_with("%enum.") ||
    param_type.starts_with("%class.") || param_type.starts_with("%union.")) {
    param_type = "ptr";
}
```

In `instructions_call.cpp` (call sites):
```cpp
// Lines 992-994: Same check duplicated
bool is_aggregate_value =
    actual_type.find("%struct.") == 0 || actual_type.find("%enum.") == 0 ||
    actual_type.find("%class.") == 0 || actual_type.find("%union.") == 0;
```

In `mir_codegen.cpp` (self/this parameter):
```cpp
// Lines 326-333: Yet again
if ((p.name == "this" || p.name == "self") && p.type) {
    std::string llvm_ty = mir_type_to_llvm(p.type);
    if (llvm_ty.starts_with("%struct.") || llvm_ty.starts_with("%enum.") || ...)
        param_types.push_back(mir::make_ptr_type());
}
```

**This pattern is duplicated in at least 10 places**, each with slightly different logic, creating a maintenance nightmare.

## How Rust Handles ABI

### FnAbi: Centralized ABI Decisions

rustc has a dedicated `FnAbi` type that computes all ABI decisions **once** per function signature. The codegen reads the pre-computed ABI info — it never makes ABI decisions itself.

```rust
// rustc: FnAbi computed once, used everywhere
struct FnAbi<'a, Ty> {
    args: Vec<ArgAbi<'a, Ty>>,    // How each arg is passed
    ret: ArgAbi<'a, Ty>,          // How return value is passed
    c_variadic: bool,
    conv: Conv,                    // Calling convention
}

struct ArgAbi<'a, Ty> {
    layout: TyAndLayout<'a, Ty>,  // Type layout (size, align)
    mode: PassMode,                // Direct, Indirect, Pair, Cast, Ignore
}

enum PassMode {
    Ignore,                        // Zero-sized types (Unit)
    Direct(ArgAttributes),         // Passed in register
    Pair(ArgAttributes, ArgAttributes), // Fat pointer (ptr + metadata)
    Cast { .. },                   // Coerce to different type
    Indirect { .. },               // Passed by pointer (sret, etc.)
}
```

The ABI computation is in `rustc_target/src/abi/call/`. For Win64:
- Structs ≤ 8 bytes, power-of-2 size: passed in register
- Structs > 8 bytes or non-power-of-2: passed by pointer (Indirect)
- Return values follow the same rule; large returns use sret

### Key Insight: ABI is Decoupled from Codegen

rustc computes ABI before codegen starts. The codegen just reads `PassMode::Indirect` and emits a pointer — it doesn't check struct sizes or string prefixes. If a new type is added, the ABI computation handles it automatically.

## How Go Handles ABI

### ABIConfig: Register vs Stack

Go's recent register ABI (Go 1.17+) uses `ABIConfig` to decide which arguments go in registers vs stack:

```go
type ABIConfig struct {
    IntRegs   int   // Available integer registers
    FloatRegs int   // Available float registers
    // ...
}

type ABIParamResult struct {
    InRegs   bool     // True if passed in registers
    Offset   int64    // Stack offset if not in registers
    Regs     []RegIndex
}
```

The assignment is computed per-function and stored. The codegen reads it directly.

### Key Insight: One Decision Point

Go makes the "how to pass this argument" decision in ONE place (`abi.go`), and the codegen in `ssa.go` reads the pre-computed result. There are no scattered `if size > 8` checks in the codegen.

## How Clang Handles ABI

### ABIInfo: Platform-Specific ABI Computation

Clang has the most sophisticated ABI handling. Each platform has its own `ABIInfo` subclass:

```cpp
class ABIInfo {
    virtual ABIArgInfo classifyReturnType(QualType RetTy) const = 0;
    virtual ABIArgInfo classifyArgumentType(QualType Ty) const = 0;
};

class WinX86_64ABIInfo : public ABIInfo {
    ABIArgInfo classifyArgumentType(QualType Ty) const override {
        // Win64 rules:
        // - Trivially copyable, 1/2/4/8 bytes → direct in register
        // - Everything else → indirect (pointer)
        if (isAggregateType(Ty)) {
            uint64_t size = getContext().getTypeSize(Ty);
            if (size <= 64 && isPowerOf2(size))
                return ABIArgInfo::getDirect(); // coerce to iN
            return ABIArgInfo::getIndirect(align, /*byval=*/false);
        }
        return ABIArgInfo::getDirect();
    }
};
```

The result is `ABIArgInfo` — a tagged union of Direct, Extend, Indirect, Ignore, Expand, CoerceAndExpand. The codegen reads this and emits accordingly.

### CGFunctionInfo: Cached Per-Signature

```cpp
class CGFunctionInfo {
    ABIArgInfo ReturnInfo;           // How to return
    SmallVector<ArgInfo> Arguments;  // How to pass each arg
    CallingConv CC;
    bool HasSRet;                    // sret flag computed HERE
};
```

All ABI decisions are in `CGFunctionInfo`, computed once. The codegen never inspects types to decide passing convention — it reads the pre-computed info.

## What TML Must Change

### Current Architecture (Wrong)

```
MIR Function → codegen reads params → for each param:
  → convert type to string → check string prefix → if aggregate → emit "ptr"
  → repeat at EVERY call site, EVERY declaration, EVERY self/this param
```

### Target Architecture (Correct)

```
MIR Function → ABI module computes FnABI → codegen reads pre-computed info:
  → param.mode == PassMode::Indirect → emit "ptr" + alloca + store
  → param.mode == PassMode::Direct → emit value directly
  → param.mode == PassMode::Ignore → skip (Unit type)
```

### Concrete Changes

#### 1. Create `ABI` Module (`compiler/src/codegen/abi/`)

```cpp
// compiler/include/codegen/abi.hpp
enum class PassMode {
    Direct,     // Pass in register (primitives, small structs)
    Indirect,   // Pass by pointer (large structs, sret returns)
    Ignore,     // Zero-sized (Unit type)
    Pair,       // Fat pointer (slice, dyn, closure)
};

struct ArgABI {
    PassMode mode;
    std::string llvm_type;     // The actual LLVM type to emit
    bool sret = false;         // True for sret return parameter
};

struct FnABI {
    ArgABI ret;                        // Return value ABI
    std::vector<ArgABI> args;          // Argument ABIs
    std::string calling_convention;    // "ccc" or "win64cc"
};

// Compute FnABI from function signature — called ONCE per function
FnABI compute_fn_abi(const mir::Function& func, const MirCodegen& codegen);
```

#### 2. Compute ABI Once Per Function

In `emit_function()` and `emit_function_declaration()`:
```cpp
FnABI abi = compute_fn_abi(func, *this);
// Use abi.args[i].mode for every parameter decision
// Use abi.ret.mode for return value decision
```

#### 3. Use ABI Info at Call Sites

In `emit_call_inst()`:
```cpp
FnABI callee_abi = get_or_compute_abi(func_name);
for (size_t i = 0; i < args.size(); ++i) {
    if (callee_abi.args[i].mode == PassMode::Indirect) {
        // Spill to alloca, pass pointer
    } else if (callee_abi.args[i].mode == PassMode::Direct) {
        // Pass value directly
    } else if (callee_abi.args[i].mode == PassMode::Ignore) {
        // Skip Unit parameters
    }
}
```

#### 4. Win64 ABI Rules (Centralized)

```cpp
PassMode classify_argument(const MirTypePtr& type, const std::string& llvm_type) {
    // Rule 1: Unit/void → Ignore
    if (llvm_type == "void" || llvm_type == "{}") return PassMode::Ignore;

    // Rule 2: Primitives → Direct
    if (is_primitive(llvm_type)) return PassMode::Direct;

    // Rule 3: Pointers → Direct
    if (llvm_type == "ptr") return PassMode::Direct;

    // Rule 4: Fat pointers (slice, dyn, closure) → Pair
    if (llvm_type == "{ ptr, i64 }" || llvm_type == "{ ptr, ptr }")
        return PassMode::Pair;

    // Rule 5: Small aggregates (≤8 bytes, power-of-2) → Direct (coerce to iN)
    size_t size = compute_type_size(type);
    if (size <= 8 && is_power_of_2(size)) return PassMode::Direct;

    // Rule 6: Large aggregates → Indirect (pass by pointer)
    return PassMode::Indirect;
}
```

### Comparison Table

| Aspect | Rust | Go | Clang | TML (current) |
|--------|------|-----|-------|---------------|
| ABI decision location | FnAbi (centralized) | ABIConfig (centralized) | ABIInfo (centralized) | **10+ scattered sites** |
| Decision computed when | Before codegen | Before codegen | Before codegen | **During codegen** |
| Decision cached | Yes (per signature) | Yes (per function) | Yes (CGFunctionInfo) | **No (recomputed every time)** |
| sret detection | In FnAbi.ret.mode | In ABIResult | In CGFunctionInfo | **sret_functions_ side-table** |
| Aggregate detection | Type layout query | Type size query | isAggregateType() | **String prefix check** |
| Unit type handling | PassMode::Ignore | Zero-size skip | ABIArgInfo::Ignore | **Ad-hoc void/"{}" checks** |
