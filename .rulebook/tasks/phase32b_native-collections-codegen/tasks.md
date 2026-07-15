## 1. Implementation
- [ ] 1.1 Inline `List::new(cap)` in `emit_intrinsic.tml`: emit `malloc(cap * elem_stride)`, build `{ptr, len=0, cap}` struct on stack, return fat pointer
- [ ] 1.2 Inline `List::push(item)` in `emit_intrinsic.tml`: emit capacity check branch, `realloc`-doubling on overflow, GEP store to `ptr[len]`, increment `len` field
- [ ] 1.3 Inline `HashMap::new(cap)` in `emit_intrinsic.tml`: emit `calloc` for bucket array sized to next power-of-two capacity, initialize metadata fields
- [ ] 1.4 Inline `.len()` accessor for `List` and `HashMap` in `emit_intrinsic.tml`: lower to a single `extractvalue` on the fat pointer with no branching
- [ ] 1.5 Write `compiler-tml/tests/codegen/collections_inline.test.tml` covering: `List::new` returns empty list, `push` appends element, `len` after push, `HashMap::new` creates empty map, `HashMap.len` returns 0 on fresh map

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
