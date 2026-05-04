# Proposal: phase24j_cc-blockitem-shared-migration

## Why

phase24g/h/i closed the bulk of the Heap-borrow-drop bug class for
`CType`, `CDecl`, `CDeclarator`. Empirical residual: `cc_driver
essential.c` × 5 = 0/5 SIGSEGV (exit 139) on the same pre-existing
`Maybe[Heap[CBlockItem]]` cleanup crash.

`CBlockItem` is the last major Heap-bearing AST type still using
unique-owning `Heap[T]` rather than refcounted `Shared[T]`. Variants:

```tml
pub enum CBlockItem {
    DeclItem(Heap[CDecl]),       // already Shared[CDecl] post-phase24g via inner
    StmtItem(Heap[CStmt]),       // Heap[CStmt] still
    Mixed(...)
}
```

And `CStmt` itself holds `Heap[CExpr]` / `Heap[CBlockItem]` for nested
control flow:

```tml
pub enum CStmt {
    Expr(Heap[CExpr]),
    Compound(List[Heap[CBlockItem]]),
    If(Heap[CExpr], Heap[CStmt], Maybe[Heap[CStmt]]),
    For(Maybe[Heap[CBlockItem]], Maybe[Heap[CExpr]], Maybe[Heap[CExpr]], Heap[CStmt]),
    While(Heap[CExpr], Heap[CStmt]),
    ...
}
```

Same Heap-borrow-drop pattern as phase24g: when a `List[Heap[CStmt]]` or
`List[Heap[CBlockItem]]` is iterated and elements are partially moved
into wrapping enums, the source-list drop double-frees. essential.c has
deeply nested function bodies (1465 lines, lots of `if`/`for`/`while`)
which trigger the cleanup SIGSEGV deterministically.

Closing this enables `tml cc essential.c` to reach exit 0 in 5/5 runs,
unblocking phase0z's gate and the full C-runtime self-compile path.

## What Changes

Migrate `Heap[CBlockItem]`, `Heap[CStmt]`, `Heap[CExpr]`, `Heap[CInit]` →
`Shared[T]` across the cc parser/AST. Mirrors the phase24g pattern:

1. `compiler-tml/src/cc/ast.tml`:
   - `CBlockItem::DeclItem(Heap[CDecl])` already migrated; `StmtItem(Heap[CStmt])` → `Shared[CStmt]`
   - `CStmt` variants: every `Heap[CExpr]` / `Heap[CStmt]` / `Heap[CBlockItem]` → `Shared[T]`
   - `CExpr` variants: every nested `Heap[CExpr]` → `Shared[CExpr]`
   - `CInit` variants: similar
   - Add `impl Duplicate for CStmt`, `impl Duplicate for CExpr`, `impl Duplicate for CBlockItem`, `impl Duplicate for CInit` (manual `when` blocks since `@auto(duplicate)` doesn't yet support enums).

2. `compiler-tml/src/cc/parser.tml`:
   - Every `Heap[CStmt]::new(...)` / `Heap[CExpr]::new(...)` / `Heap[CBlockItem]::new(...)` / `Heap[CInit]::new(...)` → `Shared[T]::new(...)`.
   - Every partial-field move (`leaf = inner.expr`, `var x = parsed.stmt` etc.) gets explicit `.duplicate()` per phase24h pattern.

3. `compiler-tml/src/cc/parse_stmt.tml` / `parse_expr.tml` if they hold any of the above types.

4. `compiler-tml/src/cc/lower.tml`: callers of CTranslationUnit decls iteration that touch CBlockItem/CStmt/CExpr — add `.duplicate()` on iteration consumers.

5. `compiler-tml/src/cc/bin/cc_driver.tml`: any consumer printing/walking the AST that owns Heap[CStmt] etc.

## Impact

- Affected specs: none.
- Affected code:
  - `compiler-tml/src/cc/ast.tml`
  - `compiler-tml/src/cc/parser.tml`
  - `compiler-tml/src/cc/parse_stmt.tml`
  - `compiler-tml/src/cc/parse_expr.tml`
  - `compiler-tml/src/cc/lower.tml`
  - `compiler-tml/src/cc/bin/cc_driver.tml`
  - Possibly test files that construct CBlockItem / CStmt / CExpr directly.
- Breaking change: NO (internal refactor; cc API surface unchanged).
- User benefit: `tml cc essential.c` exits 0 deterministically. Closes
  the full Heap-borrow-drop bug class in cc/. Unblocks phase0z gate
  (essential.c × 5 = 5/5).

## Source

- phase24g (CType + CDecl + CDeclarator migration) — established pattern.
- phase24h (CDeclarator partial-move .duplicate() fix) — same pattern in declarator paths.
- phase24i (function-like macros) — left essential.c at 0/5 due to this residual.
- phase0z (essential.c self-compile gate) — gated on this closeout.
