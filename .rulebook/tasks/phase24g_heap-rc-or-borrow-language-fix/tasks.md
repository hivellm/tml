## 1. Design
- [x] 1.1 Discovered `lib/core/src/alloc/shared.tml` already provides `Shared[T]` — a non-atomic `Rc[T]`-style refcounted pointer with `new`, `duplicate` (refcount-increment), `Drop` (decrement + free at zero), `is_unique`, `get_mut`, `try_unwrap`, `downgrade`/`SharedWeak`. No language change needed. Selecting option (a) via the existing `Shared[T]`.
- [x] 1.2 Refcount is non-atomic — `Shared[T]` already targets single-threaded sharing (cc_driver fits). `Sync[T]` (atomic variant) exists for multi-threaded use; not needed here.
- [x] 1.3 Allocation layout for `Shared[T]` is already defined: `SharedInner[T] { value: T, strong_count: I32, weak_count: I32 }` allocated as one block; `Shared { ptr: *SharedInner[T] }` in the user-facing struct.

## 2. Migration (CTypeEnv + CType)
- [ ] 2.1 Migrate `CTypeEnv` bucket types in `compiler-tml/src/cc/types.tml`: `typedefs: HashMap[Str, Heap[CType]]` → `HashMap[Str, Shared[CType]]`. Same for `structs`, `unions`, `enums` (`Shared[CAggregate]`, `Shared[CEnumType]`).
- [ ] 2.2 At every `env.X.get(k)`, call `.duplicate()` on the returned `Shared` to refcount-increment.
- [ ] 2.3 Migrate `CType` Heap-bearing variants to `Shared`: `Ptr(Shared[CType])`, `Array(Shared[CArrayType])`, `Struct(Shared[CAggregate])`, `Union(Shared[CAggregate])`, `Enum(Shared[CEnumType])`, `Func(Shared[CFuncType])`, `Qualified(Shared[CType], CQualifiers)`.
- [ ] 2.4 Update `impl Duplicate for CType` to call `.duplicate()` on `Shared` payloads (cheap refcount-increment chain instead of deep clone).
- [ ] 2.5 Migrate `CFuncType.ret`, `CFuncType.params`, `CArrayType.elem`, `CAggregateField.ty`, `CDeclarator::Pointer`/`Array`/`Func` from `Heap` to `Shared` where they hold inner CType / CDeclarator references.
- [ ] 2.6 Audit construction sites (`Heap::new(...)` in lower.tml, parser.tml, types.tml) — replace with `Shared::new(...)` for the migrated types.

## 3. Cleanup phase24c/24d/24e/24f workarounds
- [ ] 3.1 Remove `into_raw()` + `Heap::from_raw()` patches in `compiler-tml/src/cc/types.tml` Typedef/StructRef/UnionRef/EnumRef arms. Replace with `env.X.get(tag).duplicate()`.
- [ ] 3.2 Remove `impl Duplicate for CType` deep-clone path; replace with refcount-increment (item 2.4).
- [ ] 3.3 Remove `declarator_name_value_leak` in `compiler-tml/src/cc/parser.tml`; restore the simpler recursive `declarator_name → declarator_name_heap → declarator_name` loop with `Shared`-aware `Heap[CDeclarator]` → `Shared[CDeclarator]` migration.
- [ ] 3.4 Remove the for-init `into_raw + from_raw` patch (commit `6c48664a`) once the migration covers `List[Shared[CDecl]]`.

## 4. Verify
- [ ] 4.1 `./build/debug/cc_driver.exe .sandbox/sig_alone.c --emit=ast` exits 0 deterministically across 10 consecutive runs.
- [ ] 4.2 `./build/debug/cc_driver.exe compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` exits 0 deterministically.
- [ ] 4.3 Phase 0x bug #7/#8/#9 + heap_decl_var_repro + phase24c heap_ctype_return_repro tests still pass.
- [ ] 4.4 Full compiler test suite: 299/318 baseline preserved (only pre-existing K001/X002/X003 failures).

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update or create documentation covering the implementation
- [ ] 5.2 Write tests covering the new behavior
- [ ] 5.3 Run tests and confirm they pass
