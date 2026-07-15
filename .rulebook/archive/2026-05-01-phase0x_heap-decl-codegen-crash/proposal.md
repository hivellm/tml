# Proposal: phase0x_heap-decl-codegen-crash

## Why

Constructing a `CVarDecl` with multiple `Str` fields and pushing it
into a `List[Heap[CDecl]]` via `decls.push(Heap[CDecl]::new(CDecl::Var(vd)))`
crashes with ACCESS_VIOLATION (null pointer READ) — both with literal
and heap-allocated Strs. This is the immediate cause of the
`cp_parse_translation_unit` segfault on `int x;` (the CToken
dangling-Str fix in commit XXXX was a prerequisite; this is the
next-stage crash).

Minimal reproducer compiled from `compiler-tml/src/cc/parser.tml`
(literal Str variant — equally broken):

```tml
let vd: CVarDecl = CVarDecl {
    name: "x", specifiers: specs, declarator: CDeclarator::Ident("x"),
    init: Nothing,
    file: "t.c", line: 1, col: 1
}
decls.push(Heap[CDecl]::new(CDecl::Var(vd)))
```

The existing regression tests for bug #7/#8/#9
(`compiler/tests/compiler/nested_constructor_push.test.tml`,
`large_enum_by_value_duplicate.test.tml`) pass because they use
synthetic `BigStruct` / `Inner` / `Outer` types that differ from
`CVarDecl` / `CDecl` in field shape. The CDecl-specific shape — most
likely the `specifiers: CDeclSpecifiers` field whose nested
`CBaseType` enum has variants carrying `Str` payloads
(`StructRef(Str)`, `UnionRef(Str)`, `EnumRef(Str)`, `Typedef(Str)`),
combined with `declarator: CDeclarator` whose `Func` variant carries
`Heap[CDeclarator]` recursively — exposes a codegen path the existing
regression tests miss.

This task unblocks `phase24_cc-cli-integration` Phase 4
(`tml cc` self-compile of `essential.c` / `mem.c`), which depends on
`cp_parse_translation_unit` round-tripping declarations through
`List[Heap[CDecl]]`.

## What Changes

1. Reproduce the crash with a minimal CDecl-shaped fixture, isolated
   from the full parser. Land it as a new regression test under
   `compiler/tests/compiler/`.
2. Identify the offending codegen path: candidate sites are
   `Heap[T]::new` for `T = CDecl` (heap allocation + value
   initialization), the `CDecl::Var(vd)` enum-variant construction
   when `vd` contains nested struct/enum payloads, or the drop glue
   for the resulting `Heap[CDecl]` slot in `List`.
3. Compare against Rust's `Box::new(Decl::Var(vd))` IR for the
   equivalent shape (Rust-as-Reference IR methodology).
4. Apply the codegen fix in `compiler/src/codegen/llvm/...` (likely
   `core/drop.cpp` or `core/heap.cpp` depending on the offending
   primitive).
5. Re-enable the `cp_parse_translation_unit` regression test in
   `compiler-tml/tests/native/c_parser.test.tml` (currently NOTE-only).
6. Run `tml cc .sandbox/int_x.c --emit=ast` and confirm exit 0.

## Impact

- Affected specs: none (pure codegen fix, no semantic change).
- Affected code:
  - `compiler/src/codegen/llvm/...` (the offending pass).
  - `compiler-tml/tests/native/c_parser.test.tml` (re-enable TU test).
  - `compiler/tests/compiler/heap_decl_var_repro.test.tml` (new).
- Breaking change: NO.
- User benefit: unblocks `tml cc` self-compilation of C runtime
  (essential.c, mem.c) — the immediate phase24 milestone.
