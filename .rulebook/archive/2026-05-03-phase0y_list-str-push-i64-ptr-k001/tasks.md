## 1. Reproduce + isolate
- [x] 1.1 Build `compiler-tml/src/main_frontend.tml` → capture full LLVM IR via `--emit-ir`. Identify the failing call site at line ~15948 and the surrounding function (which TML source produced it).
- [x] 1.2 Reduce to a minimal `.tml` repro: the trivial `l.push("hello")` builds clean; the real trigger is `l.push(s as I64)` (mirrors `compiler-tml/src/intern.tml::InternTable::intern`). Captured in `.sandbox/list_str_push_i64_repro.tml` and lifted into `compiler/tests/compiler/list_str_push_k001.test.tml`.
- [x] 1.3 Decide the path: AST legacy codegen path (`try_gen_impl_method_call` in `method_impl.cpp`). Confirmed via `--stage=parser:cpp` reproducing.

## 2. Rust-as-Reference IR comparison
- [x] 2.1 Rust analogue documented in `docs/patches/v0.3.46.md`: `Vec<String>::push(s)` lowers to a struct-by-value `(ptr, ptr)` call with no integer-mangling fallback; the mangling we emitted (`9push__I64`) had no Rust counterpart because Rust's monomorphization tracks impl-level vs method-level params via the type-system, not via a heuristic on `type_args.size()`.
- [x] 2.2 Side-by-side noted in patch doc: TML emitted `tml_N3std11collections4list9List__Str9push__I64E(ptr, ptr)` for an `(ptr, i64)` call site; Rust emits `_ZN…push…(ptr, ptr)` for an `(ptr, ptr)` site (with `inttoptr` if the user wrote a transmute equivalent).

## 3. Diagnose root cause
- [x] 3.1 Mangler audit (`compiler/src/codegen/llvm/core/llvm_utils.cpp`): mangling itself is correct — the bug was upstream in the suffix-construction logic (`method_type_suffix`), not in the type-code lookup.
- [x] 3.2 `method_impl.cpp::try_gen_impl_method_call` lines ~365-373 — heuristic `if (impl_param_count >= func_sig->type_params.size()) impl_param_count = 0;` collapses impl-level T into "infer from args" mode for the impl-method-registration shape. Confirmed by tracing `func_sig->type_params=["T"]`, `named.type_args=["Str"]`, condition true, then T re-inferred as `I64` from the argument.
- [x] 3.3 `compiler/src/types/env_module_load_decls.cpp:466-475` (impl) puts impl-level generics at the head of `type_params`; `:904-909` (trait) only stores method-level generics. The two registration shapes need different handling — `FuncSig::impl_self_type_args` already distinguishes them but was unused in codegen.

## 4. Fix
- [x] 4.1 Two-part fix in `compiler/src/codegen/llvm/expr/method_impl.cpp::try_gen_impl_method_call`: (a) use `func_sig->impl_self_type_args.size()` to authoritatively count impl-level params when the field is set; fall back to legacy heuristic only for trait/default-method shape. (b) Add `int → ptr` (`inttoptr`) and `ptr → int` (`ptrtoint`) coercions to the argument coercion block to handle user-written `as I64` casts on `Str` values.
- [x] 4.2 Rebuilt compiler via `scripts\build.bat`. tml.exe reports v0.3.46.
- [x] 4.3 `tml build compiler-tml/src/main_frontend.tml` exits 0; `main_frontend.exe` produced.

## 5. Regression suite
- [x] 5.1 `tml build compiler-tml/src/cc/bin/cc_driver.tml` exits 0; `cc_driver.exe` produced.
- [x] 5.2 Re-measured phase24g: `cc_driver.exe sig_alone.c --emit=ast` ×10 → 8/10 (was 2/10 — net +6 deterministic). `cc_driver.exe essential.c -I compiler/runtime/include/c-stdlib --emit=ast` ×5 → 0/5 (unrelated remaining Heap[T] structural issue from phase24g; not fixed by this patch and not in scope for this codegen bug).
- [x] 5.3 Compiler suite: 312/319 (was 299/318) — net +13 passes, exceeds ≥310/318 target. The 7 remaining failures are unrelated pre-existing K001/X002/N001 patterns (different signatures: `i64`-into-struct, `i64`-into-`i1`, `Heap__Expr` vs `i64`, integer-constant-must-have-integer-type, two X002 codegen timeouts on parser_basic / parse_decl_expr_stmt_pattern, one N001 link failure on when_block_body). The 8 K001 lexer/parser tests called out in the task description (`lexer_basic`, `lexer_strings`, `parse_path_full`, `parse_path_only`, `parse_named_type`, `parse_type_basic`, `parse_type_new_keyword`, `test_tokenize_basic`) all now PASS.
- [x] 5.4 Regression test `compiler/tests/compiler/list_str_push_k001.test.tml` added with 4 sub-tests: literal push, variable push, `as I64` cast push (the original trigger), and multiple-element push. Passes.

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Update or create documentation covering the implementation: VERSION bumped 0.3.45 → 0.3.46, CHANGELOG.md row added, `docs/patches/v0.3.46.md` documents both bugs, the fix, verification numbers, and Rust IR comparison.
- [x] 6.2 Write tests covering the new behavior: `compiler/tests/compiler/list_str_push_k001.test.tml` (4 sub-tests covering literal, variable, `as I64` cast, and multi-element push).
- [x] 6.3 Run tests and confirm they pass: regression suite 4/4 pass; full compiler suite 312/319 (was 299/318, net +13).
