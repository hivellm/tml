## 1. Optional chaining
- [x] 1.1 std/json/types.tml -- already let-else from phase31c, no ?. eligible
- [x] 1.2 compiler-tml/types/imports.tml -- multi-statement Just arms, not eligible
- [x] 1.3 compiler-tml/types/module.tml -- no when-on-Maybe patterns
- [x] 1.4 Scanned option.tml, thread/mod.tml -- 0 eligible (combinator defs, complex arms)

## 2. Behavior aliases
- [x] 2.1 Defined `ThreadSafe = Send + Sync` in core/traits/marker.tml (7 occurrences found)
- [x] 2.2 Copyable/Hashable not created (below 3-occurrence threshold)
- [x] 2.3 Applied ThreadSafe in arc.tml (4), rwlock.tml (1), once.tml (1), atomic/mod.tml (1)

## 3. Tail
- [x] 3.1 ThreadSafe alias defined and applied across 4 std/sync files
- [x] 3.2 No regressions
- [x] 3.3 Committed
