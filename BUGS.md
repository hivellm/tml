# Known Bugs and Limitations

## Compiler Issues

### 1. Generic Functions + Closures (CRITICAL)

**Status**: BLOCKING stdlib combinators
**Discovered**: 2025-12-26
**Severity**: HIGH

**Description**:
Generic functions that accept closures as parameters generate invalid LLVM IR. The closure is incorrectly passed as a primitive type instead of a function pointer.

**Example**:
```tml
func apply[T](x: T, f: func(T) -> T) -> T {
    return f(x)
}

func main() {
    // This fails to compile
    let result: I32 = apply(5, do(val: I32) val * 2)
}
```

**Error**:
```
error: global variable reference must have pointer type
  %t4 = call i32 @tml_apply__I32(i32 5, i32 @tml_closure_0)
                                            ^
```

**Root Cause**:
When monomorphizing generic functions, the type of closure parameters is incorrectly lowered to the wrong LLVM type (i32 instead of function pointer).

**Workaround**:
Use non-generic helper functions:
```tml
func map_i32(m: Maybe[I32], f: func(I32) -> I32) -> Maybe[I32] {
    when m {
        Just(val) => return Just(f(val)),
        Nothing => return Nothing
    }
}
```

**Impact**:
- ❌ Cannot implement generic combinators (map, filter, fold)
- ❌ Blocks stdlib Maybe[T] and Outcome[T,E] combinator functions
- ❌ Limits functional programming patterns

**Tests**:
- `packages/std/tests/closure_simple.test.tml` ✓ (non-generic closures work)
- `packages/std/tests/generic_closure_simple.test.tml` ✗ (generic closures fail)
- `packages/std/tests/maybe_map_simple.test.tml` ✗ (Maybe[T] map fails)

**Related Files**:
- `packages/compiler/src/codegen/llvm_ir_gen.cpp` - Closure code generation
- `packages/compiler/src/types/checker.cpp` - Generic instantiation

**Next Steps**:
1. Investigate closure type lowering in LLVM IR generator
2. Fix monomorphization to correctly handle function pointer types
3. Add regression tests for generic + closure combinations

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
