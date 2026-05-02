## 1. Design
- [ ] 1.1 Write design doc comparing options (a) refcount header, (b) Borrow[T] type, (c) language-level lifetimes. Pick one (recommend option a).
- [ ] 1.2 Decide refcount semantics: non-atomic for single-threaded use, or atomic from day one.
- [ ] 1.3 Define new Heap[T] allocation layout: `[refcount: U64][T payload]`. Document alignment rules for arbitrary T.

## 2. Implementation
- [ ] 2.1 Update `lib/core/src/alloc/heap.tml`: header allocation, Drop refcount-decrement gate, explicit `clone()` method (or auto-Duplicate fallthrough that increments).
- [ ] 2.2 Update `lib/core/src/alloc/heap.tml::Heap::new(value)` to allocate header + payload, initialize `refcount = 1`, return `Heap { ptr: payload_addr }`.
- [ ] 2.3 Update `Heap::from_raw` / `Heap::into_raw` semantics to handle the header (e.g. `from_raw` claims an existing allocation with refcount=1; `into_raw` strips ownership without dropping).
- [ ] 2.4 Audit other Heap[T] consumers across `lib/` and `compiler-tml/` for breakage.

## 3. Cleanup phase24c/24d/24e/24f workarounds
- [ ] 3.1 Remove `into_raw()` + `Heap::from_raw()` patches in `compiler-tml/src/cc/types.tml` Typedef/StructRef/UnionRef/EnumRef arms. Replace with direct return of `env.X.get(tag)` (refcount handles sharing).
- [ ] 3.2 Remove `impl Duplicate for CType` and `@auto(duplicate)` decorations on CFuncType / CArrayType / CAggregateField / CAggregate / CEnumValue / CEnumType — refcount removes the need for deep-clone.
- [ ] 3.3 Remove `declarator_name_value_leak` in `compiler-tml/src/cc/parser.tml`; restore the simpler recursive `declarator_name → declarator_name_heap → declarator_name` loop.

## 4. Verify
- [ ] 4.1 `./build/debug/cc_driver.exe .sandbox/sig_alone.c --emit=ast` exits 0 deterministically across 10 consecutive runs.
- [ ] 4.2 `./build/debug/cc_driver.exe compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` exits 0 deterministically.
- [ ] 4.3 Phase 0x bug #7/#8/#9 + heap_decl_var_repro + phase24c heap_ctype_return_repro tests still pass.
- [ ] 4.4 Full compiler test suite: 299/318 baseline preserved (only pre-existing K001/X002/X003 failures).

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
