# Proposal: phase24i_cc-variadic-macro-paste

## Why

After v0.3.49 fixed the `T *f(...)` function-definition parser bug
(phase0z residual), `cc_driver compiler/runtime/core/essential.c -I
compiler/runtime/include/c-stdlib --emit=ast` advances past lines
170–191 (the `tml_panic_hook_fn` typedef + every `TML_EXPORT void*`
definition) and surfaces the next downstream blocker:

```
cc_driver: parse failed at compiler/runtime/core/../diagnostics/log.h:216:31:
    expected expression
```

Source location:

```c
// compiler/runtime/diagnostics/log.h:211–216
#define RT_TRACE(module, fmt, ...) rt_log(RT_LOG_TRACE, module, fmt, ##__VA_ARGS__)
#define RT_DEBUG(module, fmt, ...) rt_log(RT_LOG_DEBUG, module, fmt, ##__VA_ARGS__)
#define RT_INFO (module, fmt, ...) rt_log(RT_LOG_INFO,  module, fmt, ##__VA_ARGS__)
#define RT_WARN (module, fmt, ...) rt_log(RT_LOG_WARN,  module, fmt, ##__VA_ARGS__)
#define RT_ERROR(module, fmt, ...) rt_log(RT_LOG_ERROR, module, fmt, ##__VA_ARGS__)
#define RT_FATAL(module, fmt, ...) rt_log(RT_LOG_FATAL, module, fmt, ##__VA_ARGS__)
```

The `##__VA_ARGS__` GCC extension (also adopted by C++20 / C23 as
`__VA_OPT__(,) __VA_ARGS__`) drops the preceding comma when the
variadic args are empty. essential.c invokes these macros with at
least one variadic arg (`RT_FATAL("runtime", "panic: %s", message)`),
but the preprocessor still has to handle the `##` token-paste
operator on `__VA_ARGS__`.

Closing this drives essential.c × 5 toward 5/5 exit 0 and unblocks
the `Maybe[Heap[CBlockItem]]` cleanup-time SIGSEGV in `c_frontend`
(also pre-existing baseline) which is the next item after this.

## What Changes

1. Bisect the actual failure point. Likely candidates inside
   `compiler-tml/src/cc/preproc/`:
   - `tokenize.tml` — recognising `##` as the paste operator vs `#`
     followed by `#`.
   - `macros.tml` — macro definition recording (does the `body`
     token list preserve `##__VA_ARGS__` correctly?).
   - `expand.tml` (or wherever variadic substitution lives) —
     handling the `##` operator during substitution.
2. Construct minimal repros. Increasing complexity:
   - `#define M(...) f(__VA_ARGS__)\nM(1, 2);` — basic VA_ARGS.
   - `#define M(x, ...) f(x, __VA_ARGS__)\nM(1, 2, 3);` — named arg + VA_ARGS.
   - `#define M(x, ...) f(x, ##__VA_ARGS__)\nM(1);` — `##` with empty VA.
   - `#define M(x, ...) f(x, ##__VA_ARGS__)\nM(1, 2);` — `##` with non-empty VA.
   - The exact `RT_FATAL` shape: `#define RT_FATAL(m, f, ...) g(L, m, f, ##__VA_ARGS__)\nRT_FATAL("a", "%s", "b");`
3. Identify whether the parse error is at preprocessing (token-paste
   not implemented / wrong) or at the C parser stage (e.g. the
   substituted token stream produces a malformed C expression).
4. Implement the fix in the preprocessor.

## Impact

- Affected specs: none (preprocessor bugfix).
- Affected code: `compiler-tml/src/cc/preproc/{tokenize,macros}.tml`
  and possibly a yet-unnamed `expand.tml`.
- Breaking change: NO.
- User benefit: `tml cc essential.c` parses past the diagnostic-macro
  definitions; closes one of the last gaps for self-compiling the
  C runtime via TML's C frontend.
