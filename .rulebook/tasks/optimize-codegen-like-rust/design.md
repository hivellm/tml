# Design: Optimize TML Codegen Using Rust as Reference

## Methodology: Rust-as-Reference IR Comparison

For every codegen issue (bug or optimization), the agent MUST follow this workflow:

### Step 1: Write Equivalent Code

Create minimal Rust and TML files in `.sandbox/` that exercise the specific pattern:

```
.sandbox/temp_<feature>.rs    — Rust version
.sandbox/temp_<feature>.tml   — TML version (equivalent semantics)
```

### Step 2: Generate IR from Both

```bash
# Rust debug IR
rustc --edition 2021 --emit=llvm-ir -C opt-level=0 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust_debug.ll

# Rust release IR
rustc --edition 2021 --emit=llvm-ir -C opt-level=3 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust_release.ll

# TML debug IR
tml build .sandbox/temp_<feature>.tml --emit-ir --legacy

# TML release IR
tml build .sandbox/temp_<feature>.tml --emit-ir --legacy --release
```

### Step 3: Compare Function-by-Function

For each function, compare:
- **Instruction count**: TML should not exceed 2x Rust's count for equivalent logic
- **Alloca count**: TML should not have allocas that Rust avoids
- **Type layouts**: Struct/enum sizes should match
- **Call overhead**: Indirect calls, extra parameters, unnecessary wrappers
- **Safety features**: Overflow checks, null checks, bounds checks

### Step 4: Fix and Verify

Fix the TML codegen, rebuild, regenerate IR, and verify the improvement. Run the test suite to ensure no regressions.

## Architecture

### Enum Layout Specialization

Current generic layout for all enums:
```llvm
; Generic: tag + max(variant_sizes) rounded up to i64 boundary
%struct.Maybe__I32 = type { i32, [1 x i64] }  ; 16 bytes
```

Target specialized layouts:
```llvm
; Specialized for small primitives: tag + value, same size
%struct.Maybe__I32 = type { i32, i32 }         ; 8 bytes

; Specialized for pointers: nullable pointer, no tag
; Maybe[ref T] = ptr (null = Nothing, non-null = Just)
```

Implementation: In `llvm_ir_gen.hpp`, the `get_llvm_type()` method for `EnumType` should check if the enum is `Maybe[T]` where T is a primitive, and emit a compact layout.

### SSA-Direct Struct Construction

Current pattern (stack-based):
```llvm
%tmp = alloca %struct.Foo
%f0 = getelementptr %struct.Foo, ptr %tmp, i32 0, i32 0
store i32 %val0, ptr %f0
%f1 = getelementptr %struct.Foo, ptr %tmp, i32 0, i32 1
store i32 %val1, ptr %f1
%result = load %struct.Foo, ptr %tmp
ret %struct.Foo %result
```

Target pattern (SSA-direct):
```llvm
%r0 = insertvalue %struct.Foo undef, i32 %val0, 0
%result = insertvalue %struct.Foo %r0, i32 %val1, 1
ret %struct.Foo %result
```

Implementation: In `gen_struct_init()`, detect when all fields are known values (not mutable, not address-taken) and emit `insertvalue` chain instead of alloca+GEP+store.

### On-Demand Declarations

Current: All 500+ runtime declarations emitted at start of every module.

Target: Declarations emitted only when a `call @function_name(...)` is generated.

Implementation: In `LLVMIRGen`, maintain a `std::set<std::string> emitted_declarations_`. When emitting a `call` to an external function, check if it's in the set. If not, add the declaration to a deferred buffer and mark it. At finalization, prepend all collected declarations.

### Checked Arithmetic

Current:
```llvm
%result = add nsw i32 %a, 1   ; UB on overflow
```

Target (debug mode):
```llvm
%ov = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %a, i32 1)
%result = extractvalue { i32, i1 } %ov, 0
%overflowed = extractvalue { i32, i1 } %ov, 1
br i1 %overflowed, label %panic.overflow, label %continue

panic.overflow:
  call void @panic(ptr @.str.overflow_at_line_N)
  unreachable

continue:
  ; use %result
```

Implementation: In `gen_binary_expr()` for `Add`, `Sub`, `Mul`, check `CompilerOptions::checked_math` flag. If true, emit the overflow-checking pattern. Map operations to intrinsics:
- `+` → `@llvm.sadd.with.overflow` (signed) / `@llvm.uadd.with.overflow` (unsigned)
- `-` → `@llvm.ssub.with.overflow` / `@llvm.usub.with.overflow`
- `*` → `@llvm.smul.with.overflow` / `@llvm.umul.with.overflow`

## Key Decisions

1. **Keep alloca for mutable locals**: Only optimize the "construct and return" pattern. Mutable variables, loop variables, and address-taken values still use alloca.

2. **Enum specialization is type-specific**: Don't try to optimize all enums generically. Start with `Maybe[T]` and `Outcome[T,E]` which are the most common and have clear optimization patterns.

3. **Checked arithmetic is opt-in**: Default to checked in debug, unchecked in release. User can override with `--checked-math` / `--no-checked-math`.

4. **Exception handling is Phase 6**: This is the most complex change and depends on a working Drop system. Defer until the simpler optimizations are done.
