# Proposal: phase24c_cc-driver-return-path-codegen

## Why

After phase24b shipped option (a) — `env: ref CTypeEnv` everywhere —
the value-pass + drop-glue crash is gone (verified by
`test_phase24b_base_to_ctype_typedef_repeat`, which calls
`base_to_ctype` 3× in a row without crashing). The compiled
`cc_driver.exe` now builds cleanly and parses simple C inputs:

- `int x;` — exit 0, `cc_driver: parsed`
- `typedef int int32_t;` — exit 0, `cc_driver: parsed`
- `typedef int int32_t; int32_t x;` — exit 0, `cc_driver: parsed`

But the original phase24b reproducer still crashes:

- `typedef int int32_t; int32_t add(int32_t a, int32_t b) { return a + b; }`
  → exit 127, no output

`File::append_all` traces inside `lower_func_decl` and `base_to_ctype`
show the precise crash site is no longer the `t.get()` Heap deref,
nor the second-call HashMap bucket access. Trace from the failing run:

```
lfd:enter
btc:enter (return-type lookup via return_type_of_func_decl)
btc:typedef name='int32_t'
btc:typedef:has
btc:typedef:got_heap
btc:typedef:got_value kind=6
btc:typedef:about_to_return
lfd:ret_c          ← OK, return-type base_to_ctype returns cleanly
lfd:ret_mir
lfd:params_loop n=2
lfd:param[0]:enter
lfd:param[0]:got
btc:enter (param 0 base_type)
btc:typedef name='int32_t'
btc:typedef:has
btc:typedef:got_heap
btc:typedef:got_value kind=6
btc:typedef:about_to_return    ← reaches return statement
                                    ← but caller never resumes
[CRASH]
```

So:

- The first `base_to_ctype` call (return-type, called from
  `return_type_of_func_decl`) returns OK.
- The second `base_to_ctype` call (param 0, called directly from
  `lower_func_decl`'s parameter loop) reaches the `return r`
  statement but the caller's `let p_base: CType = ...` slot never
  gets the value — process aborts during the SSA return + caller
  resume sequence.

Both calls return the same `CType::Int` (kind=6, primitive variant —
no Heap payload). Both have identical signatures. The difference is
the caller context: `return_type_of_func_decl` is a separate TML
function whose body returns CType directly, while
`lower_func_decl`'s parameter loop captures the return value into a
local `let p_base: CType`.

This is the proposal's original "symmetric return-path" hypothesis
from phase24b — return of a value-type enum across a function
boundary, in the specific shape `Func(B)::base_to_ctype → Func(A)::let
p_base = ...`. Phase 0x fixed the call-site arg path for
`Heap[T]::new(...)` static methods; the return path of a value-type
enum from a free function in a parameter-loop context is unfixed.

End-to-end `tml cc t.c` on any C source with a typedef'd type used as
a function parameter is blocked by this crash; phase24b items 4.1
and 4.2 cannot be exercised in CI until it lands.

## What Changes

1. **Reproduce in C++ unit-test land.** Author a TML test fixture
   that mirrors the failing shape: outer function with a `loop` that
   captures the return of an inner function returning
   `enum E { A(Heap[T]), B(Heap[U]), Primitive }` into a `let x: E`
   binding. Iteration N=2 should crash if the codegen bug is real.
2. **Compare against rustc.** Write the equivalent in Rust
   (`fn outer() { for _ in 0..2 { let x: E = inner(); … } }`),
   compile to LLVM IR, diff against TML's IR for
   `lower_func_decl`'s parameter loop. The Rust-as-Reference IR
   methodology (CLAUDE.md) should pinpoint the lowering difference.
3. **Apply the codegen fix** in
   `compiler/src/codegen/llvm/decl/func.cpp` (return-type lowering)
   and/or `compiler/src/codegen/llvm/expr/return_stmt.cpp` (return
   instruction emission), structurally similar to the phase0x patch
   in `expr/method_static_dispatch.cpp` but on the return-path side.
4. **Verify** `./build/debug/cc_driver.exe .sandbox/test_no_inc.c
   --emit=ast` exits 0 with `cc_driver: parsed`. Re-run phase24b
   item 4.2 (`essential.c --emit=ast`) and document the next
   limitation (separate task entry).

## Impact

- Affected specs: none (codegen fix, no language change).
- Affected code:
  - `compiler/src/codegen/llvm/decl/func.cpp` — likely the return
    type / sret call lowering path.
  - `compiler/src/codegen/llvm/expr/return_stmt.cpp` — return value
    materialization.
  - `compiler/tests/compiler/heap_ctype_return_repro.test.tml` —
    minimal regression in C++ test infrastructure.
- Breaking change: NO.
- User benefit: unblocks `tml cc` on any non-trivial C source.
  Without this fix, every typedef'd type used as a function
  parameter crashes `tml cc`, making phase24 Phase 4 (essential.c
  self-compile) unreachable.
