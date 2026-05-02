## 1. Audit
- [ ] 1.1 Grep for all `HashMap[K, Heap[V]].get(...)` call sites in `lib/`, `compiler-tml/`, `compiler/runtime/`. Classify each: does the result flow into a longer-lived structure (move) or is it consumed in-place (read)?
- [ ] 1.2 Confirm `essential.c` segfault root cause is StructRef/UnionRef/EnumRef Heap-borrow-drop (vs other unrelated codegen gaps). Add `File::append_all` traces in `base_to_ctype`'s tag arms and run `./build/debug/cc_driver.exe compiler/runtime/core/essential.c --emit=ast` to confirm.

## 2. Localized fix in types.tml
- [ ] 2.1 Apply `into_raw()`-style workaround to `StructRef` arm: reconstruct the Heap from the bucket's pointer before moving into `CType::Struct(...)` so the local's drop is a no-op.
- [ ] 2.2 Same for `UnionRef`.
- [ ] 2.3 Same for `EnumRef`.
- [ ] 2.4 Type-check clean. Rebuild cc_driver.exe.
- [ ] 2.5 Verify `./build/debug/cc_driver.exe compiler/runtime/core/essential.c --emit=ast` makes progress (no longer crashes at the StructRef point).

## 3. Long-term design
- [ ] 3.1 Write design doc comparing options (a) Rc-style Heap[T], (b) HashMap.get_ref, (c) language-level borrow tracking. Pick one.
- [ ] 3.2 If option (b): add `HashMap.get_ref(key) -> ref V` to `lib/std/src/collections/hashmap.tml` alongside existing `get`.
- [ ] 3.3 Migrate cc/types.tml + audit hits to use the new API.
- [ ] 3.4 Document migration guide in `docs/` for any external HashMap[K, Heap[V]] users.

## 4. Verify
- [ ] 4.1 cc_driver passes through StructRef/UnionRef/EnumRef without crash on real C inputs.
- [ ] 4.2 Document the next limitation surfaced by `essential.c` (separate task entry).

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
