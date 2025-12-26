# Known Bugs and Limitations

## Compiler Issues

### 1. Generic Functions + Closures (FIXED ✅)

**Status**: FIXED
**Discovered**: 2025-12-26
**Fixed**: 2025-12-26
**Severity**: HIGH (was blocking stdlib combinators)

**Description**:
Generic functions that accept closures as parameters were generating invalid LLVM IR. The closure was incorrectly passed as a primitive type instead of a function pointer.

**Example**:
```tml
func apply[T](x: T, f: func(T) -> T) -> T {
    return f(x)
}

func main() {
    let result: I32 = apply(5, do(val: I32) val * 2)  // Now works! ✅
}
```

**Root Cause**:
The `gen_closure` function in `llvm_ir_gen_expr.cpp` was not setting `last_expr_type_` to `"ptr"` after generating a closure. When generic function call code used `last_expr_type_` to determine argument types, it used stale type information.

**Fix**:
Added `last_expr_type_ = "ptr"` before returning from `gen_closure` function.

**Changed File**:
- `packages/compiler/src/codegen/llvm_ir_gen_expr.cpp` line 812

**Tests**:
- `packages/std/tests/closure_simple.test.tml` ✅ (non-generic closures work)
- `packages/std/tests/generic_closure_simple.test.tml` ✅ (generic closures now work!)
- `packages/std/tests/maybe_map_simple.test.tml` ❌ (blocked by Bug #2: Maybe__Unit)

---

### 2. Maybe__Unit Invalid LLVM IR

**Status**: RELATED TO #1
**Discovered**: 2025-12-26
**Severity**: MEDIUM

**Description**:
When using `Nothing` without explicit type context, the compiler generates `Maybe__Unit` which produces invalid LLVM IR.

**Example**:
```tml
let z: Maybe[I32] = map(Nothing, do(val: I32) val * 2)
```

**Error**:
```
error: base element of getelementptr must be sized
  %t64 = getelementptr inbounds %struct.Maybe__Unit, ptr %t63, i32 0, i32 0
                                                      ^
```

**Workaround**:
Always explicitly type `Nothing`:
```tml
let n: Maybe[I32] = Nothing
let z: Maybe[I32] = map(n, do(val: I32) val * 2)
```

---

## Language Limitations

### 1. Enum Variant Import Not Implemented

**Status**: KNOWN LIMITATION
**Severity**: LOW (Workaround available)

**Description**:
Cannot import enum variants directly from modules:
```tml
use std::types::{Maybe, Just, Nothing}  // Just and Nothing not importable
```

**Workaround**:
Redefine types locally in test files (see `packages/std/tests/types.test.tml`).

**Impact**:
- Tests need to duplicate type definitions
- Verbose code in some cases

---

## Performance Issues

(None documented yet)

---

## Documentation Needed

- [ ] Add this BUGS.md to repository docs
- [ ] Update contributing guide with bug reporting template
- [ ] Add "Known Issues" section to README.md
