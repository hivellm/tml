## 1. Migrate AST types (ast.tml)
- [x] 1.1 Migrate `CBlockItem` variants: `StmtItem(Heap[CStmt])` → `Shared[CStmt]`. Verify `DeclItem` already uses Shared[CDecl] post-phase24g.
- [x] 1.2 Migrate `CStmt` variants: every `Heap[CExpr]`, `Heap[CStmt]`, `Heap[CBlockItem]` payload → `Shared[T]`.
- [x] 1.3 Migrate `CExpr` variants: every nested `Heap[CExpr]` payload → `Shared[CExpr]`.
- [x] 1.4 Migrate `CInit` variants: every `Heap[CExpr]` / `Heap[CInit]` payload → `Shared[T]`.
- [x] 1.5 Add `impl Duplicate for CBlockItem`, `impl Duplicate for CStmt`, `impl Duplicate for CExpr`, `impl Duplicate for CInit` (manual `when` blocks). Also added impls for CTypeName, CGenericAssoc, CDesignator, CInitElement; plus free helpers `dup_designator_value`, `dup_block_item_value`, `dup_block_items`, `dup_init_elem_value`, `dup_init_elems`, `dup_designators`, `dup_shared_expr_list`, `dup_generic_assocs` to route around two codegen ABI bugs (small-enum method dispatch mismatch + List[Shared[T]] duplicate return-type mistype).
- [x] 1.6 Add `use core::alloc::shared::Shared` import. Type-check ast.tml. (Already imported; removed unused `use core::alloc::heap::Heap`.)

## 2. Migrate construction sites (parser.tml + parse_*.tml)
- [x] 2.1 `compiler-tml/src/cc/parser.tml`: every `Heap[CStmt|CExpr|CBlockItem|CInit|CTypeName]::new(...)` → `Shared[T]::new(...)`.
- [x] 2.2 `compiler-tml/src/cc/parse_stmt.tml`: re-exports only, no constructor sites.
- [x] 2.3 `compiler-tml/src/cc/parse_expr.tml`: re-exports only, no constructor sites.
- [x] 2.4 Type-check all 3 files. (parser.tml + parse_stmt.tml + parse_expr.tml all clean.)

## 3. Add .duplicate() at partial-move sites
- [x] 3.1 Audit parser.tml / parse_stmt.tml / parse_expr.tml for partial-field moves of CBlockItem/CStmt/CExpr/CInit values. Appended `.duplicate()` to ~70 sites per phase24h pattern (cp_parse_postfix, cp_parse_unary, cp_parse_cast, cp_parse_binary, cp_parse_cond, cp_parse_assign_expr, cp_parse_expr, cp_parse_initializer, cp_parse_top_decl, cp_parse_compound_stmt, cp_parse_for, all stmt sub-parsers).
- [x] 3.2 Type-check all touched files. Clean.

## 4. Migrate consumers (lower.tml, cc_driver.tml)
- [x] 4.1 `compiler-tml/src/cc/lower.tml`: `for_rest` signature updated `Maybe[Heap[CExpr]]` → `Maybe[Shared[CExpr]]`. Other consumers use `.get()` which works for both Heap and Shared.
- [x] 4.2 `compiler-tml/src/cc/bin/cc_driver.tml`: no direct Heap[CStmt/CExpr/CBlockItem] ownership; uses public CTranslationUnit/CParser API only.
- [x] 4.3 Type-check all consumers. Clean.

## 5. Verify
- [ ] 5.1 `cc_driver essential.c -I compiler/runtime/include/c-stdlib --emit=ast` × 5 → **0/5** (target 5/5; deterministic SIGSEGV remains — see follow-up phase24k).
- [x] 5.2 `cc_driver sig_alone.c --emit=ast` × 10 → 10/10 (preserves phase24h baseline).
- [x] 5.3 phase24h regression repros (`int (*p);`, `typedef void (*sig_t)(int);`, deep nested function) × 30 each → 30/30 each.
- [x] 5.4 c_lexer, c_parser, c_frontend test suites pass (752/754; 2 pre-existing X002 timeouts in core/num).
- [x] 5.5 Compiler suite 290/295 ran (5 pre-existing K001/X002 — same as 0.3.50 baseline).

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Bump VERSION 0.3.50 → 0.3.51, CHANGELOG entry, `docs/patches/v0.3.51.md`.
- [x] 6.2 Add regression test in `compiler-tml/tests/native/c_frontend.test.tml` exercising deep-nested function body parse + clean shutdown (`test_phase24j_deep_nested_function_parses`, 30+ lines of mixed if/for/while).
- [x] 6.3 Run all touched tests and confirm pass.

## Status: PARTIAL FIX SHIPPED
phase0z gate (essential.c × 5 = 5/5) NOT met. Follow-up filed:
`phase24k_essential-cleanup-segv`. phase24j stays in active state per
the brief: "If 5.1 < 5/5 but improvement... mark fixed items, document
next blocker, file follow-up. Leave phase24j active."

Improvement: large/complex C programs (200 globals, deeply nested
control flow, function-pointer typedefs, all phase24h regressions)
all 30/30 deterministic. essential.c specifically still triggers a
content-shape-dependent cleanup-time fault.

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 7.1 Update or create documentation covering the implementation
- [ ] 7.2 Write tests covering the new behavior
- [ ] 7.3 Run tests and confirm they pass
