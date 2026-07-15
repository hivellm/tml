# Proposal: phase0x_remove-i32-fallback-codegen

## Why

The AST and MIR codegen paths have 13 locations where an unresolved type silently
falls back to `i32` instead of emitting an error. These fallbacks mask real type
resolution bugs and produce invalid LLVM IR downstream — for example, a struct
type silently becoming `i32` causes `sub nsw i32 %struct_value, 1` errors that
are extremely hard to trace back to the source.

User mandate: "NEVER return default i32 if the compiler is not certain of the type
— it must return ERROR, not fallback to i32. NEVER."

## What Changes

Replace all 13 dangerous `= "i32"; // default/fallback` patterns with proper error
handling: either resolve the type from available context, or emit a codegen error
with the source location and unresolved type name.

### Files to fix (13 instances)

| File | Line | Context |
|------|------|---------|
| `mir/instructions.cpp` | 483 | AwaitInst result_type |
| `llvm/expr/method_generic.cpp` | 706 | Closure param type |
| `llvm/expr/method_generic.cpp` | 785 | Func param type |
| `llvm/expr/method_generic.cpp` | 857 | Generic func return type |
| `llvm/expr/method_class.cpp` | 70 | Class method return type |
| `llvm/expr/binary.cpp` | 168 | Deref assignment type |
| `llvm/expr/binary.cpp` | 804 | Array element type |
| `llvm/expr/closure.cpp` | 146 | Closure param type |
| `llvm/expr/method_primitive_ext.cpp` | 652 | Primitive ext arg type |
| `llvm/expr/unary.cpp` | 550 | Deref operation type |
| `llvm/builtins/intrinsics_extended.cpp` | 246 | checked_add/sub/mul[T] |
| `llvm/builtins/mem.cpp` | 259 | mem::zeroed[T] type |
| `llvm/expr/call_indirect.cpp` | 42,216 | Indirect call return |
| `llvm/expr/method_dyn.cpp` | 134 | Dyn trait method return |
| `llvm/expr/method_impl_module.cpp` | 370 | Impl module arg type |

### Already fixed (not in scope)

- `llvm/control/loop.cpp:528` — fixed in phase0p (item_llvm_type)
- `llvm/control/loop.cpp:350` — fixed in phase0p (range_type → i64)
- `llvm/control/if.cpp:182,270,493` — SAFE (intentional init/discriminant)

## Impact

- Affected specs: compiler/codegen
- Affected code: 13 files in `compiler/src/codegen/`
- Breaking change: NO (errors will surface bugs that already produce invalid IR)
- User benefit: Codegen bugs surface as clear errors instead of cascading type mismatches
