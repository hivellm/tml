# Proposal: phase0y_list-str-push-i64-ptr-k001

## Why

`tml build compiler-tml/src/main_frontend.tml` (and several lexer/parser tests:
`lexer_basic`, `lexer_strings`, `parser_basic`, `parse_*`, `test_tokenize_basic`,
`test_frontend_sim`, `parse_decl_expr_stmt_pattern`) fails LLVM verification with:

```
[K001] Failed to parse LLVM IR: ir:15948:86: error: '%t6437' defined with type
'i64' but expected 'ptr'
  %t6438 = call {} @tml_N3std11collections4list9List__Str9push__I64E(
                       ptr %t6433, ptr %t6437)
```

The mangled callee name `List__Str9push__I64E` indicates that during generic
instantiation of `List[Str].push`, the second parameter type was mangled as
`I64` even though `T = Str` should map to `ptr` at the LLVM level. The
verifier sees the signature `push(ptr, ptr)` (correct) but the call site
passes an `i64` for the value argument. So either:

1. The mangled name is wrong (should be `List__Str9push__StrE`) AND the
   instantiation is correct → callsite codegen has the wrong type.
2. The mangled name is right (codegen fell back to a default I64 instantiation)
   AND the instantiation is wrong → instantiation type-substitution bug.

This blocks rebuilding `cc_driver`, which blocks empirical verification of
phase24g (the `Heap[T] → Shared[T]` migration just shipped) and blocks
`essential.c` self-compile via `tml cc`. It is also one of the largest
clusters of pre-existing K001 failures in the compiler test suite (≥11
tests) — fixing it likely unblocks all of them.

## What Changes

Trace the IR generation for `List[Str].push(value: Str)` calls in
compiler-tml sources:

- `compiler-tml/tests/lexer/lexer_basic.test.tml` and similar tests use
  `var tokens: List[Str] = …` then `tokens.push(<some_expr>)`.
- The mangling and instantiation path lives in
  `compiler/src/codegen/llvm/expr/method_static_dispatch.cpp` (per phase0x
  bisect notes — static method calls go through `gen_method_static_dispatch`,
  not `gen_call`).
- The argument value type comes from the expression-emission path in
  `compiler/src/codegen/llvm/expr/binary_ops.cpp` /
  `compiler/src/codegen/llvm/expr/literal.cpp` /
  `compiler/src/codegen/llvm/expr/method.cpp`.

Fix-candidates:

(a) Mangling fix: ensure `T = Str` mangles to `Str` (not `I64`) in the
    push specialization. Likely site:
    `compiler/src/codegen/llvm/symbol/mangler.cpp` or wherever the generic
    type parameter → mangled-name table is built.
(b) Substitution fix: when monomorphizing `List[Str].push`, propagate
    the substituted parameter type into the FuncInfo entry so the
    LLVM signature matches the callsite.
(c) Callsite cast: if the callsite legitimately has an i64 (e.g. char
    code being pushed into a `List[Str]`), insert an `inttoptr` coercion
    before the call. Less likely — would mask a deeper substitution bug.

The right fix needs Rust-as-Reference IR comparison: compile equivalent
`Vec<String>::push(s: String)` in Rust, see how the IR shapes the call.

## Impact

- Affected specs: none (pure codegen bug fix).
- Affected code: `compiler/src/codegen/llvm/expr/method_static_dispatch.cpp`,
  `compiler/src/codegen/llvm/symbol/mangler.cpp` (likely), possibly
  `compiler/src/codegen/llvm/core/runtime.cpp`.
- Breaking change: NO. Pure bugfix.
- User benefit:
  - Unblocks `tml build compiler-tml/src/main_frontend.tml` (full TML self-host
    rebuild path).
  - Unblocks `tml build compiler-tml/src/cc/bin/cc_driver.tml` (C frontend
    binary rebuild, required to empirically verify phase24g).
  - Unblocks `essential.c` self-compile via `tml cc`.
  - Likely fixes 11+ pre-existing K001 lexer/parser test failures in the
    compiler suite, raising the baseline from 299/318 toward 310+/318.
