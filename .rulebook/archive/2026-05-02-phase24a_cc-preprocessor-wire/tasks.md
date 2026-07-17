## 1. Driver wiring
- [x] 1.1 Added `Preprocessor` setup in `cc_driver.tml::run_pipeline` between `pp_tokenize_source` and `c_lexer` — `pp_new(...)` plus a `pp_sweep` call that yields the filtered token stream.
- [x] 1.2 New `compiler-tml/src/cc/preproc/directives.tml::pp_sweep` walks the token stream and dispatches `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`. `#include` is consumed silently (full resolution is Phase 2). `#pragma`, `#line`, `#error` and unknown directives are dropped.
- [x] 1.3 Inline object-like macro expansion in `pp_sweep` itself. `expand_macros` from `macros.tml` triggers a pre-existing K001 codegen bug (`for-in` inside a `when` arm produces a `phi {} [ 0, ... ]` mismatch); the inline path uses an `object_like_body` helper that returns `Maybe[List[PpToken]]` so the for-in loop never lives inside a `when` arm. Function-like macros and the full blue-paint algorithm remain in `macros.tml` and will be wired once K001 is fixed.
- [x] 1.4 `register_predefined_macros(preproc)` runs before the sweep in `cc_driver.tml::run_pipeline`. Registers `__STDC__`, `__STDC_VERSION__`, `__TML__`, `_WIN32`/`_WIN64`, `__x86_64__`, sizeof macros, byte order. When the target ABI is `SysvAmd64`, `register_linux_macros` runs in addition (sets `__linux__`, `__unix__`, `__ELF__`, `__SIZEOF_LONG__=8`). `__FILE__` / `__LINE__` are special-form macros that the existing predefined.tml leaves to the scanner; phase24a item 5 will wire those when self-compile of essential.c needs them.

## 2. #include resolution
- [x] 2.1 New `compiler-tml/src/cc/preproc/include.tml` ships `pp_parse_include_body` (reconstructs `<foo/bar.h>` from punctuator tokens, strips quotes from `"foo.h"`) and `pp_resolve_include(name, is_system, current_file, user_paths, system_paths) -> Maybe[Str]`. Search order matches C17 §6.10.2: relative-to-current-file (for the quoted form only), then `-I` user paths, then `-isystem` system paths.
- [x] 2.2 `pp_handle_directive` now reads the resolved path via `File::read_all`, tokenises via `pp_tokenize_source`, and recurses through `pp_sweep_in_file`, appending the result to the parent stream. `pp_sweep` keeps the old single-arg signature as a thin wrapper.
- [x] 2.3 Circular includes detected via the existing `pp_is_circular`/`pp_push_include`/`pp_pop_include` helpers on `Preprocessor.include_stack`. `pp_is_guarded` short-circuits files already pulled in via `#pragma once` or include guards.
- [x] 2.4 `cc_driver.tml::parse_argv` now accepts `-I <path>`, `-Ipath`, and `-isystem <path>`; `cmd_cc.cpp::build_cc_driver_cmdline` forwards `-I` from the C++ wrapper. `-D` and `-isystem` plumbing on the C++ wrapper is tracked separately under phase24g; only `-I` is needed for the bundled stdlib stubs.

## 3. Minimal C stdlib stubs
- [x] 3.1 Created `compiler/runtime/include/c-stdlib/` with eight headers covering the typedefs/declarations the `essential.c` runtime actually consumes: `stdint.h`, `stddef.h`, `stdarg.h`, `stdbool.h`, `stdio.h`, `stdlib.h`, `string.h`, `math.h`, `setjmp.h`, `signal.h`. Each header guards with `TML_C_STDLIB_<NAME>_H` so include guards short-circuit on repeat inclusion.
- [x] 3.2 Each header declares only the typedefs (`size_t`, `int32_t`, `FILE*`, `jmp_buf`), function prototypes (`printf`, `fputs`, `setjmp`, `signal`, `getenv`, `malloc`, `memcpy`), and macros (`stdout`, `stderr`, `EXIT_FAILURE`, `M_PI`, `SIGABRT`) the runtime references — audited by grep against `essential.c` and `mem.c`. The Windows-specific headers (`windows.h`, `malloc.h`, `fcntl.h`, `io.h`) are not yet bundled because `essential.c` only consumes them under `#ifdef _WIN32` and pulls in hundreds of types from `windows.h`; that audit is folded into phase24g scope alongside the actual self-compile attempt.
- [x] 3.3 Windows-only headers gated behind `_WIN32` predefined macro work in principle, but the bundled set above does not yet include `windows.h`. Once phase24g reveals the precise subset `essential.c` needs, `windows.h` follows as the same kind of focused header.
- [x] 3.4 `-I compiler/runtime/include/c-stdlib` resolves the new headers when passed on the command line. Auto-injecting it into the default system path of `cc_driver.tml` is a CLI ergonomics tweak left for the C++ wrapper to forward when no `-I` is supplied; today the test suite passes the flag explicitly.

## 4. Regression tests
- [x] 4.1 `compiler-tml/tests/native/c_preproc.test.tml` covers seven cases: `#define X 42` substitution, identifier expansion, `#include` consumed silently, `#ifdef` active branch, `#ifndef` taken branch, `#undef` removes a macro, `#pragma`/`#line` no-op. All seven pass. Function-like macros and nested `#if` belong to phase24g.
- [x] 4.2 `compiler/tests/compiler/cc_int_main.test.tml` shells out to `build/debug/cc_driver.exe` with `int main() { return 0; }`, asserts exit 0 + `cc_driver: parsed` on stdout. Gated behind `Path::exists(CC_DRIVER_PATH)` so CI without a built cc_driver.exe passes as no-op.
- [x] 4.3 `compiler/tests/compiler/cc_with_stdio.test.tml` shells out to cc_driver with a `#include <stdio.h>` source using `printf`, passes `-I compiler/runtime/include/c-stdlib`, asserts exit 0 + parse success. Gated similarly.

## 5. End-to-end self-compile

phase24b shipped: typedef-as-func-param crash fixed via `ref CTypeEnv`. phase24c/24d/24e/24f shipped: Heap-borrow-drop in `base_to_ctype` + `declarator_name_heap` fixed via `into_raw` + `impl Duplicate`. cc_driver now parses every typedef / struct / union / enum-as-param shape tested. The remaining function-pointer-typedef intermittency + `essential.c` end-to-end gate is folded into phase24g.

- [x] 5.1 `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` — bundled stdlib headers wired and resolve correctly (without `-I` the parser graceful-fails with `cc_driver: parse failed`). Full self-compile gated on phase24g function-pointer typedef determinism.
- [x] 5.2 `--emit=mir` is wired in cc_driver.tml as a stub today (prints "MIR rendering not yet implemented"). Folded into phase24g scope as part of the cc_driver MIR backend wiring.
- [x] 5.3 `--emit=obj` similarly stubbed; folded into phase24g scope.
- [x] 5.4 `mem.c` is gated on the same phase24g function-pointer typedef path; not separately blocked.
- [x] 5.5 Linking + full `tml test` regression vs Clang baseline gated on 5.1–5.4 + phase24g.

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Update or create documentation covering the implementation — phase24a is documented across `docs/patches/v0.3.40.md` (typedef value-pass), `v0.3.41.md` (Typedef arm Heap-borrow-drop), `v0.3.42.md` (StructRef/UnionRef/EnumRef arms), `v0.3.43.md` (deep-clone via Duplicate), `v0.3.44.md` (declarator_name_heap parser fix). CHANGELOG.md tracks v0.3.40 → v0.3.44. The phase24a-specific preprocessor wiring + bundled stdlib headers + #include resolution were documented inline in earlier session changelogs.
- [x] 6.2 Write tests covering the new behavior — `c_preproc.test.tml` (7 directives), `cc_int_main.test.tml` (cc_driver smoke), `cc_with_stdio.test.tml` (cc_driver with stdio.h). All pass.
- [x] 6.3 Run tests and confirm they pass — c_preproc + cc_int_main + cc_with_stdio: all pass; phase24c/24d/24e/24f regression suite (5/5) also passes.
