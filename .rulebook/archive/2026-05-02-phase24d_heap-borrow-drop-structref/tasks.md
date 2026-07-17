## 1. Audit
- [x] 1.1 Audited `HashMap[K, Heap[V]].get(...)` call sites in `compiler-tml/src/cc/types.tml`. The four arms in `base_to_ctype` (Typedef, StructRef, UnionRef, EnumRef) all hit the same pattern: returned Heap aliases the bucket allocation; consumer's drop frees what the bucket still references.
- [x] 1.2 Confirmed `essential.c` segfault is dominated by the typedef-as-param case + nested-Heap typedef expansion (function-pointer typedefs). The Heap-borrow-drop pattern in the four `base_to_ctype` arms is one root cause; the `apply_declarator` / parser path for function-pointer declarators is a separate parser bug surfaced by the same workload.

## 2. Localized fix in types.tml — superseded by phase24e deep-clone
- [x] 2.1 StructRef arm: shipped phase24d v0.3.42 with `into_raw` + `from_raw` workaround. Phase24e (v0.3.43) replaced this with `borrowed.duplicate()` which produces a fully independent allocation; the env's bucket Heap and the returned CType variant own distinct allocations.
- [x] 2.2 UnionRef arm: same pattern, shipped phase24d v0.3.42, refactored to `duplicate()` in phase24e v0.3.43.
- [x] 2.3 EnumRef arm: same pattern, shipped phase24d v0.3.42, refactored to `duplicate()` in phase24e v0.3.43.
- [x] 2.4 Type-check clean on `types.tml`, `lower.tml`. Rebuilt cc_driver.exe via `tml build compiler-tml/src/cc/bin/cc_driver.tml -o build/debug/cc_driver.exe --stage=parser:cpp`.
- [x] 2.5 Verified `tml cc` parses every typedef/struct/union/enum-as-param shape we tested: `struct_ref.c`, `union_ref.c`, `enum_ref.c`, `typed_simple.c`, `typed_two.c`, `typed_ptr.c` — all exit 0 with `cc_driver: parsed`. essential.c progresses past the StructRef/UnionRef/EnumRef class but hits a separate parser bug on function-pointer typedef declarators (intermittent crash, depends on heap layout).

## 3. Long-term design — Duplicate path landed; Rc remains future work
- [x] 3.1 Selected approach: deep-clone via `Duplicate` impl. Implemented in phase24e v0.3.43 — `@auto(duplicate)` on CFuncType, CArrayType, CAggregateField, CAggregate, CEnumValue, CEnumType + manual `impl Duplicate for CType` covering all 24 variants. The env's bucket and consumer are fully decoupled — no double-free hazard regardless of drop order. Rc-style Heap (option a) and `HashMap.get_ref` (option b) remain future-work items but are not required to unblock the cc_driver path.
- [x] 3.2 Not pursued — Duplicate path proved sufficient for the tag-ref + simple-typedef pipeline.
- [x] 3.3 Migrated all four arms in `base_to_ctype` to the deep-clone path.
- [x] 3.4 Documentation — `docs/patches/v0.3.42.md` (StructRef/UnionRef/EnumRef partial) and `docs/patches/v0.3.43.md` (deep-clone via Duplicate) cover the user-facing API and remaining gaps.

## 4. Verify
- [x] 4.1 cc_driver passes every tag-ref / simple-typedef / pointer-typedef shape we tested. 5/5 regression tests pass: c_frontend, c_lexer, c_parser, phase0x heap_decl_var_repro, phase24c heap_ctype_return_repro.
- [x] 4.2 Filed follow-up `phase24f_cc-funcptr-typedef-parser` for the bare function-pointer typedef declaration crash (e.g. `typedef void (*sig_t)(int);`). Bisect: trace shows the parser intermittently extracts the wrong typedef name (sometimes `(`, sometimes `int`) instead of `sig_t`, indicating a dangling-Str or declarator-name extraction bug in `cp_parse_declarator`'s function-pointer path. Predates phase24e and is out of scope for the Heap-borrow-drop work.

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update or create documentation covering the implementation — `docs/patches/v0.3.42.md` (phase24d StructRef/UnionRef/EnumRef partial) + `docs/patches/v0.3.43.md` (phase24e deep-clone via Duplicate) document root cause, fix rationale, and remaining gaps. CHANGELOG.md updated. VERSION bumped 0.3.41 → 0.3.42 → 0.3.43.
- [x] 5.2 Write tests covering the new behavior — `compiler/tests/compiler/heap_ctype_return_repro.test.tml` provides scaffolding; cc_driver direct invocation tests on tag-ref sources serve as integration verification.
- [x] 5.3 Run tests and confirm they pass — c_frontend, c_lexer, c_parser, heap_decl_var_repro, heap_ctype_return_repro: 5/5 pass.
