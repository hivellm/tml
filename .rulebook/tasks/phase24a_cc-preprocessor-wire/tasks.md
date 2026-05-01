## 1. Driver wiring
- [x] 1.1 Added `Preprocessor` setup in `cc_driver.tml::run_pipeline` between `pp_tokenize_source` and `c_lexer` — `pp_new(...)` plus a `pp_sweep` call that yields the filtered token stream.
- [x] 1.2 New `compiler-tml/src/cc/preproc/directives.tml::pp_sweep` walks the token stream and dispatches `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`. `#include` is consumed silently (full resolution is Phase 2). `#pragma`, `#line`, `#error` and unknown directives are dropped.
- [x] 1.3 Inline object-like macro expansion in `pp_sweep` itself. `expand_macros` from `macros.tml` triggers a pre-existing K001 codegen bug (`for-in` inside a `when` arm produces a `phi {} [ 0, ... ]` mismatch); the inline path uses an `object_like_body` helper that returns `Maybe[List[PpToken]]` so the for-in loop never lives inside a `when` arm. Function-like macros and the full blue-paint algorithm remain in `macros.tml` and will be wired once K001 is fixed.
- [x] 1.4 `register_predefined_macros(preproc)` runs before the sweep in `cc_driver.tml::run_pipeline`. Registers `__STDC__`, `__STDC_VERSION__`, `__TML__`, `_WIN32`/`_WIN64`, `__x86_64__`, sizeof macros, byte order. When the target ABI is `SysvAmd64`, `register_linux_macros` runs in addition (sets `__linux__`, `__unix__`, `__ELF__`, `__SIZEOF_LONG__=8`). `__FILE__` / `__LINE__` are special-form macros that the existing predefined.tml leaves to the scanner; phase24a item 5 will wire those when self-compile of essential.c needs them.

## 2. #include resolution
- [ ] 2.1 Implement `pp_resolve_include(name: Str, is_system: I64, current_file: Str, search_paths: List[Str]) -> Maybe[Str]` returning the resolved absolute path
- [ ] 2.2 Implement `pp_include_file(pp, path)` that reads the file, recurses through `pp_tokenize_source`, runs the directive sweep on the new tokens, and splices the result in place of the `#include` directive
- [ ] 2.3 Detect and reject circular `#include` chains (track an active-file set on the Preprocessor)
- [ ] 2.4 Add `-I <path>` and `-isystem <path>` flags to `cc_driver.tml` and forward them as the search path

## 3. Minimal C stdlib stubs
- [ ] 3.1 Create `compiler/runtime/include/c-stdlib/` with stubs for `<stdint.h>`, `<stddef.h>`, `<stdarg.h>`, `<stdbool.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<math.h>`, `<setjmp.h>`, `<signal.h>`, `<malloc.h>`, `<fcntl.h>`, `<io.h>`, `<unistd.h>`
- [ ] 3.2 Each stub declares only the typedefs, function prototypes, and macros that `essential.c` and `mem.c` actually reference (audit by grep)
- [ ] 3.3 Conditional Windows-specific headers (`<windows.h>`, `<malloc.h>`, `<fcntl.h>`, `<io.h>`) gated behind `_WIN32`
- [ ] 3.4 Add `compiler/runtime/include/c-stdlib/` to the default system include path of `cc_driver.tml`

## 4. Regression tests
- [x] 4.1 `compiler-tml/tests/native/c_preproc.test.tml` covers seven cases: `#define X 42` substitution, identifier expansion, `#include` consumed silently, `#ifdef` active branch, `#ifndef` taken branch, `#undef` removes a macro, `#pragma`/`#line` no-op. All seven pass. Function-like macros and nested `#if` belong to Phase 2.
- [ ] 4.2 Add `compiler/tests/compiler/cc_int_main.test.tml` — `tml cc` an `int main() { return 0; }` source and confirm exit 0
- [ ] 4.3 Add `compiler/tests/compiler/cc_with_stdio.test.tml` — `tml cc` a source that uses `printf("hello\n")` (after `#include <stdio.h>`) and confirm parse + lower succeed (full link gated on phase24 Phase 4)

## 5. End-to-end self-compile
- [ ] 5.1 `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` exits 0
- [ ] 5.2 Same with `--emit=mir` exits 0
- [ ] 5.3 Same with `--emit=obj` produces an `.obj` byte-for-byte compatible with the Clang baseline (symbol set, sizes, alignments)
- [ ] 5.4 Repeat 5.1–5.3 for `compiler/runtime/memory/mem.c`
- [ ] 5.5 Link the TML-compiled `essential.o` + `mem.o` into the runtime archive; full `tml test` suite passes with zero regressions vs the Clang baseline

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 6.1 Update or create documentation covering the implementation
- [ ] 6.2 Write tests covering the new behavior
- [ ] 6.3 Run tests and confirm they pass
