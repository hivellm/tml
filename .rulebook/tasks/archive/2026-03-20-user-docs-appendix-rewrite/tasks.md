# Tasks: Rewrite User Documentation Appendices

- [x] Read all four existing appendix files before editing
- [x] Rewrite `docs/user/appendix-00.md` — index page with accurate, concise descriptions
- [x] Rewrite `docs/user/appendix-01-keywords.md` — all 57 reserved words organized by category, compile-time constants, naming conventions
- [x] Rewrite `docs/user/appendix-02-operators.md` — all operators with full precedence table (16 levels), compound assignment operators, examples
- [x] Rewrite `docs/user/appendix-03-builtins.md` — complete builtin reference: I/O, assertions, control, memory intrinsics, atomics, threads, time, sync primitives
- [x] Remove stale/incorrect keywords from old file (`decorator`, `crate`, `super`, `async`, `await`, `quote`, `base`, `prop`, `life`) that are not in the authoritative 57-word list
- [x] Add missing keywords from authoritative list (`var`, `enum`, `extend`, `catch`, `Self`, `move`, `requires`, `ensures`, `private`, `protected`, `with`, `extends`, `implements`, `new`, `sealed`, `virtual`, `abstract`, `override`, `namespace`, `class`, `interface`, `static`)
- [x] Add `__FUNC__` compile-time constant (was missing from all files)
- [x] Correct atomic function naming to match actual builtin names (`atomic_add_i32` not `atomic_fetch_add_i32`, `atomic_cas_i32` not `atomic_compare_exchange_i32`)
- [x] Document `size_of[T]()` and `align_of[T]()` intrinsics (were missing)
- [x] Document `ptr_read` and `ptr_write` with correct two-parameter signature (ptr + offset)
- [x] Add `lowlevel` block requirement notice to all memory/atomic/thread sections
- [x] Mark deprecated time functions clearly and point to `Instant`/`Duration` API
