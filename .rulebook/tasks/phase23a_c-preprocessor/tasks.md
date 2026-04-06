# Tasks: C Preprocessor — C Preprocessor in TML

**Status**: Planned (0/20)
**Depends on**: phase22d (ERA 3 complete — linker available), phase13b (TML lexer patterns)
**Blocks**: phase23b (C frontend needs preprocessed token stream)
**Duration**: 3–4 weeks
**Risk**: Medium-High — the blue-paint rescan-prevention algorithm and ## pasting interaction with rescanning are subtle correctness traps

---

## Phase 1: Include Handling (4 items)

- [ ] 1.1 Create `compiler-tml/src/cc/preproc/mod.tml` — `Preprocessor` type: holds `include_paths: List[Str]` (system and user paths), `defines: HashMap[Str, MacroDef]`, `include_stack: List[IncludeFrame]` (for circular include detection), `output: List[PpToken]` (the token stream produced); implement `Preprocessor.new(system_paths: List[Str], user_paths: List[Str]) -> Preprocessor`
- [ ] 1.2 Implement `#include "file.h"` — search user paths first, then system paths; resolve relative paths relative to the directory of the including file (the top of `include_stack`); push a new `IncludeFrame` with the resolved path and start scanning the included file
- [ ] 1.3 Implement `#include <file.h>` — search system paths only in order; emit `Error::IncludeNotFound` if not found in any path; detect circular includes by checking the resolved path against all frames in `include_stack`; emit `Error::CircularInclude` if detected
- [ ] 1.4 Implement include guards: detect the canonical guard pattern (`#ifndef GUARD_H` / `#define GUARD_H` / ... / `#endif`) and skip re-including a guarded file; also support `#pragma once` as an alternative guard mechanism; store seen guards in `HashMap[Str, Bool]` keyed by resolved file path

## Phase 2: Macro Expansion (5 items)

- [ ] 2.1 Create `compiler-tml/src/cc/preproc/macros.tml` — `MacroDef` enum: `ObjectLike(List[PpToken])` and `FunctionLike(List[Str], Bool, List[PpToken])` where the `Bool` is `is_variadic` and the `List[Str]` is the parameter name list; implement `#define NAME replacement` and `#define NAME(params) replacement`; implement `#undef NAME`
- [ ] 2.2 Implement object-like macro expansion: when scanning produces an identifier that matches a non-blue-painted `MacroDef::ObjectLike`, replace it with the macro body token list; push the expanded tokens back onto the scan queue for rescanning; mark the macro name as blue-painted during the rescan to prevent infinite recursion
- [ ] 2.3 Implement function-like macro expansion: after matching a `MacroDef::FunctionLike` identifier, collect the argument list (balanced parentheses, one argument per comma, variadic `...` argument collects all remaining tokens); substitute each parameter occurrence in the body with its argument token list; handle `__VA_ARGS__` for variadic macros
- [ ] 2.4 Implement variadic macros (`#define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)`): `##__VA_ARGS__` with an empty argument list suppresses the preceding comma (GNU extension, used by GCC/Clang and required by real-world headers); `__VA_OPT__(x)` (C++20 / C23) expands to `x` if `__VA_ARGS__` is non-empty, empty otherwise
- [ ] 2.5 Implement the complete blue-paint (paint-it-blue) algorithm per C11 §6.10.3.4: each token carries a `blue_set: HashSet[Str]` of macro names that caused it to be produced; during expansion, any macro name in a token's `blue_set` is not eligible for expansion; after rescanning, the blue set of the original token is unioned into the produced tokens; this prevents both direct and indirect infinite macro recursion

## Phase 3: Conditional Compilation (3 items)

- [ ] 3.1 Create `compiler-tml/src/cc/preproc/conditionals.tml` — parse and evaluate `#if <constant-expr>`, `#ifdef NAME`, `#ifndef NAME`, `#elif <constant-expr>`, `#else`, `#endif`; maintain a condition stack (each entry: `active: Bool`, `ever_active: Bool`) to correctly handle nested conditions
- [ ] 3.2 Implement constant expression evaluator for `#if`: the expression may contain integer literals, character literals, `defined(NAME)` / `defined NAME`, arithmetic operators (`+ - * / % << >> ~ ^ & |`), comparison operators (`== != < > <= >=`), logical operators (`&& || !`), and the ternary operator `?:`; all arithmetic is done in 64-bit signed integers per C11 §6.10.1; macros in the expression are expanded before evaluation
- [ ] 3.3 Implement skip mode: when a `#if`/`#elif`/`#else` branch is not taken, skip tokens until the matching `#endif`/`#elif`/`#else`, tracking nesting depth to handle nested `#if`s correctly; in skip mode, `#include` directives are NOT processed and macros are NOT expanded — only the nesting depth changes

## Phase 4: Token Operations (4 items)

- [ ] 4.1 Create `compiler-tml/src/cc/preproc/token_ops.tml` — implement the `#` (stringification) operator: when a `#` precedes a parameter name in a function-like macro body, the argument tokens are converted to a string literal; whitespace between tokens is collapsed to a single space; the result is a valid C string literal token
- [ ] 4.2 Implement the `##` (token pasting) operator: when `##` appears in a macro body between two tokens, the tokens are concatenated to form a new token; the resulting token must be a valid preprocessing token (identifier, integer constant, etc.) — an invalid paste is undefined behavior per the C standard but in practice should emit a diagnostic; re-lex the pasted result to get a single token
- [ ] 4.3 Implement the interaction between `##` and rescanning: a pasted token is subject to macro expansion during rescanning; the blue-paint algorithm applies to pasted tokens as well; a pasted token whose name is in the blue set of either source token is blue-painted with the union of both source tokens' blue sets
- [ ] 4.4 Implement `#line <lineno> "<file>"` and `#error <message>` directives: `#line` updates the current file and line tracking used for `__FILE__` and `__LINE__`; `#error` emits a compile error with the given message and stops preprocessing of the current file; `#warning` (GCC/Clang extension) emits a warning but continues

## Phase 5: Predefined Macros (2 items)

- [ ] 5.1 Implement standard predefined macros: `__FILE__` (current file path as string literal), `__LINE__` (current line number as integer), `__DATE__` (compilation date as `"MMM DD YYYY"` string literal), `__TIME__` (compilation time as `"HH:MM:SS"` string literal), `__STDC__` = 1, `__STDC_VERSION__` = 201710L (C17), `__STDC_HOSTED__` = 1
- [ ] 5.2 Implement compiler-identification macros: `__TML__` = 1 (identifies TML's C frontend), `__TML_MAJOR__`, `__TML_MINOR__`, `__TML_PATCHLEVEL__`; also provide GCC compatibility macros (`__GNUC__` = 4, `__GNUC_MINOR__` = 2, `__GNUC_PATCHLEVEL__` = 1) because many headers check for GCC to enable features; on Windows also define `_WIN32`, `_WIN64`; on Linux define `__linux__`, `__unix__`; on macOS define `__APPLE__`, `__MACH__`

## Phase 6: Testing (2 items)

- [ ] 6.1 Test: preprocess `compiler/runtime/core/essential.c` and `compiler/runtime/memory/mem.c` using TML's preprocessor with the same include paths and defines as the current build; compare the output token stream against `gcc -E` output on the same files; any difference in the token sequence (ignoring whitespace tokens) is a bug
- [ ] 6.2 Test: preprocess the 20 most-used system headers (`stdio.h`, `stdlib.h`, `string.h`, `stdint.h`, `stddef.h`, `stdbool.h`, `limits.h`, `math.h`, `errno.h`, `assert.h`, etc.) from the platform SDK; verify the output is parseable by the phase 23b C parser; no infinite loops, no crashes, no incorrect token count
