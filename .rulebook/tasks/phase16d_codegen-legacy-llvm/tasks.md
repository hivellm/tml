# Tasks: Legacy LLVM Codegen — Port Remaining to TML

**Status**: Planned (0/25)
**Depends on**: phase16c (full MIR codegen path complete including calls)
**Blocks**: phase17c (bootstrap verification needs complete codegen)
**Duration**: 10–14 weeks
**Risk**: HIGH — 41K LOC, diverse patterns; some files can be deleted rather than ported
**C++ reference**: ~41K LOC → ~26.7K TML (estimated; retirement reduces actual scope)

---

## Phase 1: Retirement Audit (3 items)

- [ ] 1.1 Audit all files in `compiler/src/codegen/llvm/` — for each file, determine if its cases are already handled by the MIR codegen path (phases 16a–16c); mark as `PORT` or `RETIRE`
- [ ] 1.2 Create `compiler-tml/src/codegen/legacy/retirement-log.md` — list each retired C++ file, the MIR codegen item that covers it, and the IR-diff test that proves equivalence
- [ ] 1.3 Run IR-diff on full stdlib after phase16a–16c TML path is active — identify any remaining cases where C++ legacy path still produces output not covered by MIR path; these become the actual port scope

## Phase 2: Builtin Intrinsics (4 items)

- [ ] 2.1 Create `compiler-tml/src/codegen/emit_intrinsic.tml` — `IntrinsicEmitter`: maps TML builtin calls to LLVM intrinsics
- [ ] 2.2 Implement memory intrinsics: `mem_alloc(n)` → `call ptr @malloc(i64 %n)`, `mem_free(p)` → `call void @free(ptr %p)`, `copy_nonoverlapping(dst, src, n)` → `call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %src, i64 %n, i1 false)`
- [ ] 2.3 Implement math intrinsics: `sqrt(x)` → `call double @llvm.sqrt.f64(double %x)`, `fabs`, `floor`, `ceil`, `pow`, `log`, `exp`, `sin`, `cos` — all via `@llvm.*.f64` intrinsics
- [ ] 2.4 Implement overflow-checked arithmetic: `add_overflow(a, b)` → `call { i64, i1 } @llvm.sadd.with.overflow.i64(i64 %a, i64 %b)`; extract result and overflow flag separately

## Phase 3: Destructor Emission (drop glue) (4 items)

- [ ] 3.1 Create `compiler-tml/src/codegen/emit_drop.tml` — `DropEmitter`: generates `drop_glue_TypeName()` functions for types that implement `Drop`
- [ ] 3.2 Implement primitive drop (no-op): for types without `Drop` impl, emit empty drop function `define void @drop_glue_I64(ptr %p) { ret void }` — LLVM will eliminate it
- [ ] 3.3 Implement struct drop: emit calls to field drop functions in reverse declaration order, then call the type's own `drop()` method if present; matches `core/drop.cpp` destruction order
- [ ] 3.4 Implement enum/Maybe/Outcome drop: check discriminant, jump to per-variant drop block; each variant drops its payload fields in reverse order; merge at exit block

## Phase 4: Auto-Derived Impls (3 items)

- [ ] 4.1 Create `compiler-tml/src/codegen/emit_derive.tml` — `DeriveEmitter`: generates IR for `#[derive(Clone, Debug, Eq, Hash, Ord)]` automatically
- [ ] 4.2 Implement derived `Clone`: generate `@TypeName__clone(ptr sret(%struct.TypeName) %out, ptr %self)` — field-by-field copy; recurse into field clone for non-Copy types
- [ ] 4.3 Implement derived `Eq`/`Hash`/`Ord`: `@TypeName__eq` → field-by-field compare with early false return; `@TypeName__hash` → FNV-1a hash of each field; `@TypeName__cmp` → lexicographic field comparison

## Phase 5: Let Statement Codegen (3 items)

- [ ] 5.1 Create `compiler-tml/src/codegen/emit_let.tml` — `LetEmitter`: handles `let` statement forms not yet covered by the MIR instruction emitter
- [ ] 5.2 Implement destructuring let: `let (a, b) = pair` → GEP indices 0 and 1, store to separate allocas; `let Foo { x, y } = val` → GEP named fields, store to allocas
- [ ] 5.3 Implement `let Just(x) = expr else { ... }` (let-else): extract discriminant, conditional branch to else block; in success branch, GEP payload field; matches `llvm_ir_gen_stmt_let.cpp` pattern

## Phase 6: Runtime Module Declarations (3 items)

- [ ] 6.1 Create `compiler-tml/src/codegen/runtime_decls.tml` — complete list of all C runtime function signatures that TML code can call via `@extern("c")`
- [ ] 6.2 Implement selective declaration emission: `emit_runtime_decls(used: HashSet[Str]) -> Text` — emits only the `declare` lines for functions actually referenced in the module (not all 500+)
- [ ] 6.3 Verify runtime declarations match actual C function signatures in `compiler/runtime/core/essential.c` and `compiler/runtime/memory/mem.c` — any mismatch is an ABI bug; add one test per runtime function

## Phase 7: Cranelift Backend (optional, 2 items)

- [ ] 7.1 Assess Cranelift backend scope: read `compiler/src/codegen/cranelift/` — determine if it is used in any test path or only by experimental flags; if unused in CI, mark as `LOW PRIORITY / SKIP`
- [ ] 7.2 Port Cranelift emission stubs only if the backend is exercised by any test suite; otherwise document as `deferred until Cranelift backend is production-ready`

## Phase 8: Full-Pipeline Differential Testing (3 items)

- [ ] 8.1 IR-diff: compile all 93 stdlib modules through the complete TML MIR codegen pipeline (16a + 16b + 16c + 16d) → zero differences against C++ codegen output
- [ ] 8.2 Run full TML test suite (1,659 tests) using the TML codegen path exclusively — all tests must pass, no regressions vs C++ codegen path
- [ ] 8.3 Retire C++ legacy LLVM codegen files confirmed covered by TML path — remove `compiler/src/codegen/llvm/` files one subdirectory at a time; rebuild and re-run tests after each removal
