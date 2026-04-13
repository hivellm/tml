# Proposal: phase33c_native-collections-codegen

## Why
`List` and `HashMap` are the two most-used collection types in TML programs. The native backend currently emits every collection operation as an opaque function call into the runtime library, which defeats LLVM's inliner and prevents any optimization across collection boundaries. Inlining the hot-path operations (`new`, `push`, `len`, basic `get`) directly into the emitted IR allows LLVM to eliminate bounds checks, hoist allocations, and vectorize loops that iterate over collections — exactly the wins that motivate a native backend in the first place.

## What Changes
- `emit_intrinsic.tml`: inline `List::new(cap)` — emit a `malloc(cap * elem_size)` call and construct the fat-pointer struct `{ptr, len=0, cap}` on the stack.
- `emit_intrinsic.tml`: inline `List::push(item)` — emit a capacity check, conditional `realloc` doubling branch, store to `ptr[len]`, and `len += 1`.
- `emit_intrinsic.tml`: inline `HashMap::new(cap)` — emit `calloc` for bucket array, store metadata struct.
- `emit_intrinsic.tml`: inline `.len()` accessor for both `List` and `HashMap` — a single `extractvalue` from the fat pointer, zero call overhead.
- New test file `compiler-tml/tests/codegen/collections_inline.test.tml` with five cases covering creation, mutation, and length queries.

## Impact
- Affected specs: `compiler-tml/src/codegen/emit_intrinsic.tml`
- Affected code: `compiler-tml/src/codegen/emit_intrinsic.tml`
- Breaking change: NO
- User benefit: Collection-heavy code runs faster on the native backend; LLVM can optimize across push/get boundaries; reduced call overhead for tight loops.
