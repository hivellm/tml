## 1. Driver wiring
- [ ] 1.1 Add `Preprocessor` setup in `cc_driver.tml::run_pipeline` between `pp_tokenize_source` and `c_lexer`
- [ ] 1.2 Run a directive sweep handling `#define`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#error`, `#pragma` (no-op), `#line` (no-op)
- [ ] 1.3 Call `expand_macros` after the directive sweep so identifier tokens get rewritten
- [ ] 1.4 Wire `predefined.tml` so the standard predefined macros (`__FILE__`, `__LINE__`, `__STDC__`, `_WIN32` when targeting Windows) are registered before the sweep starts

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
- [ ] 4.1 Add `compiler-tml/tests/native/c_preproc.test.tml` with cases for: `#define X 42` substitution, `#define ADD(a,b) ((a)+(b))` function-like macro, `#ifdef`/`#ifndef` masking, nested `#if`, `#include "..."` for relative paths, `#include <...>` for system path
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
