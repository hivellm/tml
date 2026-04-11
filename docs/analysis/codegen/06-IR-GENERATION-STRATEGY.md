# IR Generation Strategy: Text Concatenation vs Builder API

## The Problem

TML generates LLVM IR by concatenating strings:

```cpp
emitln("    " + result_reg + " = load " + type_str + ", ptr " + ptr);
emitln("    " + result_reg + " = getelementptr inbounds " + recv_vt + ", ptr " + arr_ptr + ", i32 0, i32 " + idx);
emitln("    " + result_reg + " = call ptr @str_concat_opt(ptr " + a + ", ptr " + b + ")");
```

This is the LLVM IR equivalent of writing SQL by string concatenation instead of using parameterized queries. The problems:

1. **No type checking** — `emitln("load i32, ptr " + val)` compiles fine even if `val` holds an `i64` value
2. **No structural validation** — Malformed IR is only caught when LLVM parses the text
3. **String escaping bugs** — Must manually escape special characters in string constants
4. **Performance** — Text IR must be parsed by LLVM before it can be compiled; binary IR (bitcode) or in-memory IR (IRBuilder) avoids this overhead
5. **Debugging difficulty** — Typos in instruction names produce errors at LLVM verification, not at C++ compile time

### Scale of the Problem

The 5,700 LOC of MIR codegen produces text LLVM IR through ~500 `emit()`/`emitln()` calls. Each call manually formats instruction strings. There's no validation that the emitted IR is well-formed until LLVM tries to parse it.

## How Rust Handles IR Generation

### Direct LLVM C API Calls

rustc uses the LLVM C API through `rustc_llvm` bindings:

```rust
// rustc doesn't emit text — it builds IR objects directly
let llfn = unsafe {
    llvm::LLVMAddFunction(lmod, name.as_ptr(), fn_ty)
};

// Creating instructions via Builder
let val = unsafe {
    llvm::LLVMBuildLoad2(self.llbuilder, ty, ptr, name.as_ptr())
};

// Type mismatches caught at API call time, not at text parse time
let gep = unsafe {
    llvm::LLVMBuildInBoundsGEP2(self.llbuilder, ty, ptr, indices.as_ptr(), indices.len() as u32, name.as_ptr())
};
```

The `IRBuilder` API guarantees:
- Types match (load type matches pointer element type)
- Instructions are in valid positions (terminators only at block end)
- Phi nodes reference valid predecessors
- SSA form is maintained

### Benefits

- Errors caught at build time (C++ compile time or LLVM API call time)
- No parsing overhead — IR objects are already in memory
- Type safety enforced by the API
- Can query IR properties (type of a value, number of operands) without parsing

## How Clang Handles IR Generation

### IRBuilder: Typed API

Clang uses `llvm::IRBuilder<>` which provides typed instruction creation:

```cpp
// Clang: typed, validated instruction emission
llvm::Value *load = Builder.CreateLoad(ElemTy, Ptr, "loaded");
// If Ptr is not a pointer type, this fails at the API level

llvm::Value *gep = Builder.CreateInBoundsGEP(ElemTy, Ptr, Indices, "gep");
// If indices are wrong type/count, fails at API level

llvm::Value *call = Builder.CreateCall(FnTy, Callee, Args, "result");
// If arg types don't match FnTy params, fails at API level
```

### The Key Insight: Types are Objects, Not Strings

In Clang/LLVM's C++ API:
```cpp
llvm::Type *I32Ty = llvm::Type::getInt32Ty(Context);
llvm::Type *PtrTy = llvm::PointerType::get(Context, 0);
llvm::StructType *PointTy = llvm::StructType::create(Context, {I32Ty, I32Ty}, "Point");

// Comparison is pointer equality, not string comparison
if (val->getType() == I32Ty) { ... }  // O(1), type-safe
```

In TML:
```cpp
std::string type_str = mir_type_to_llvm(type_ptr);
if (type_str == "i32") { ... }       // O(n), error-prone
if (type_str.starts_with("%struct.")) { ... } // Fragile pattern match
```

## How Go Handles IR Generation

### Direct Machine Code

Go's compiler generates machine code directly (SSA → assembly) without an intermediate text format. The `ssagen` package creates `obj.Prog` instructions that map directly to machine instructions. No text IR is involved.

This is the opposite extreme from TML — Go doesn't even use LLVM, so there's no text IR to generate.

## What TML Could Do

### Option A: Migrate to LLVM C API (Maximum Benefit, Maximum Effort)

Replace all `emitln()` calls with LLVM C API calls:

```cpp
// Before (TML current):
emitln("    " + result_reg + " = load " + type_str + ", ptr " + ptr);

// After (LLVM C API):
LLVMValueRef loaded = LLVMBuildLoad2(builder, llvm_type, ptr_val, "loaded");
```

**Effort**: Very high — would require rewriting all 5,700 LOC of codegen
**Benefit**: Type safety, no parsing overhead, debuggable at API level
**Risk**: High — complete rewrite of working code
**Recommendation**: **Not now** — save for self-hosting compiler rewrite

### Option B: Typed Emit Helpers (Medium Benefit, Low Effort)

Wrap the text emission in typed helpers that validate at C++ compile time:

```cpp
// compiler/include/codegen/ir_emitter.hpp

class IREmitter {
    std::stringstream& output_;

public:
    // Typed load — validates type string is not empty
    std::string emit_load(const std::string& type, const std::string& ptr,
                          bool is_volatile = false) {
        assert(!type.empty() && type != "void");  // Catch void loads
        assert(!ptr.empty());
        std::string reg = new_temp();
        std::string vol = is_volatile ? "volatile " : "";
        output_ << "    " << reg << " = load " << vol << type
                << ", ptr " << ptr << "\n";
        return reg;
    }

    // Typed store — validates both operands
    void emit_store(const std::string& type, const std::string& value,
                    const std::string& ptr, bool is_volatile = false) {
        assert(!type.empty() && type != "void");  // Catch void stores
        assert(!value.empty() && !ptr.empty());
        std::string vol = is_volatile ? "volatile " : "";
        output_ << "    store " << vol << type << " " << value
                << ", ptr " << ptr << "\n";
    }

    // Typed GEP — validates indices
    std::string emit_gep(const std::string& base_type, const std::string& ptr,
                         const std::vector<std::string>& indices,
                         bool inbounds = true) {
        assert(!base_type.empty() && !ptr.empty());
        assert(!indices.empty());
        std::string reg = new_temp();
        std::string ib = inbounds ? "inbounds " : "";
        output_ << "    " << reg << " = getelementptr " << ib << base_type
                << ", ptr " << ptr;
        for (const auto& idx : indices) {
            output_ << ", i32 " << idx;
        }
        output_ << "\n";
        return reg;
    }

    // Typed call — validates arg count matches
    std::string emit_call(const std::string& ret_type, const std::string& func_name,
                          const std::vector<std::pair<std::string, std::string>>& typed_args) {
        std::string reg;
        if (ret_type != "void") {
            reg = new_temp();
            output_ << "    " << reg << " = ";
        } else {
            output_ << "    ";
        }
        output_ << "call " << ret_type << " @" << func_name << "(";
        for (size_t i = 0; i < typed_args.size(); ++i) {
            if (i > 0) output_ << ", ";
            output_ << typed_args[i].first << " " << typed_args[i].second;
        }
        output_ << ")\n";
        return reg;
    }

    // Alloca with alignment
    std::string emit_alloca(const std::string& type, size_t align = 8) {
        assert(!type.empty() && type != "void");
        std::string reg = new_temp();
        output_ << "    " << reg << " = alloca " << type
                << ", align " << align << "\n";
        return reg;
    }

    // Memcpy (wraps llvm.memcpy intrinsic)
    void emit_memcpy(const std::string& dst, const std::string& src,
                     const std::string& size, bool is_volatile = false) {
        output_ << "    call void @llvm.memcpy.p0.p0.i64(ptr " << dst
                << ", ptr " << src << ", i64 " << size
                << ", i1 " << (is_volatile ? "true" : "false") << ")\n";
    }
};
```

**Effort**: Low — wrap existing emit calls incrementally
**Benefit**: Catches void/empty type bugs at emission time; reduces typos
**Risk**: Low — purely additive, no behavior change
**Recommendation**: **Do this now** — can be done incrementally per-file

### Option C: LLVM Bitcode via C API (Maximum Performance, Medium Effort)

Instead of emitting text IR and having `llvm::parseAssemblyString()` parse it, use the LLVM C API to create `llvm::Module*` directly, then compile to object code. This skips the text→IR parsing step entirely.

**Effort**: Medium — requires linking against LLVM C API (already embedded in the compiler)
**Benefit**: ~20-30% faster compilation (no text parsing), typed API
**Risk**: Medium — changes the compilation pipeline
**Recommendation**: **Future** — after the codegen architecture stabilizes

### Comparison Table

| Aspect | Rust | Go | Clang | TML (current) |
|--------|------|-----|-------|---------------|
| IR format | LLVM objects (C API) | Machine objects | LLVM objects (IRBuilder) | **Text strings** |
| Type validation | At API call | At SSA build | At API call | **At LLVM parse time** |
| Parsing overhead | None | None | None | **Full text parse** |
| Typo detection | C++ compile time | Go compile time | C++ compile time | **Runtime (LLVM verify)** |
| Debugging | LLVM API errors | Compiler errors | LLVM API errors | **"Expected type" text errors** |
