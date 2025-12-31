# Tasks: Next Compiler Features

## Progress: 100% (16/16 tasks complete)

## Phase 1 - Critical (COMPLETE)

- [x] Implement closure environment capture (basic closures work)
- [x] Add closure type checking (function pointer type inference working)
- [x] Generate LLVM IR for closures (llvm_ir_gen_expr.cpp:gen_closure)
- [x] Implement where clause parsing (parser supports where clauses)
- [x] Add where clause type checking
- [x] Enforce where constraints in calls
- [x] Fix LLVM runtime linking issues (working)
- [x] Add runtime library functions (IO, Math, Collections, String, Time, Sync, Atomic, Mem)

## Phase 2 - Medium Priority (COMPLETE)

- [x] Parse use statement groups
- [x] Resolve grouped imports
- [x] Parse named enum fields (working)
- [x] Type check enum field access (working)
- [x] Improve error span tracking
- [x] Add contextual error hints (similar name suggestions for typos)

## Phase 3 - String Interpolation (COMPLETE)

- [x] Parse string interpolation syntax
- [x] Type check interpolated strings
- [x] Generate LLVM IR for interpolated strings

## Phase 4 - Future Work (DEFERRED)

These features are lower priority and deferred for future implementation:

- [ ] Parse inline module syntax
- [ ] Type check inline modules
- [ ] Parse doc comments
- [ ] Extract documentation metadata
- [ ] Generate HTML documentation

## Implementation Notes

**Closures**: Fully working for simple cases. Complex captures may need refinement.
- Type checking: `types/checker.cpp`
- IR generation: `ir/builder_expr.cpp`, `ir/emitter_expr.cpp`
- LLVM codegen: `codegen/llvm_ir_gen_expr.cpp:gen_closure`

**Where Clauses**: Fully implemented with constraint enforcement at call sites.
- Parsing: `parser/parser_decl.cpp`
- Type checking: `types/checker.cpp` (lines 373-400, 939-958)
- Tests: `tests/types_test.cpp` (WhereClause* tests)

**Error Messages**: Improved with similar name suggestions using Levenshtein distance.
- Implementation: `types/checker.cpp` (find_similar_names, levenshtein_distance)
- Example: "Undefined variable: valeu. Did you mean: `value`?"

**Runtime Library**: Comprehensive builtin functions implemented in `codegen/builtins/`:
- io.cpp, mem.cpp, math.cpp, string.cpp, collections.cpp, time.cpp, sync.cpp, atomic.cpp

**String Interpolation**: Fully implemented (2025-12-27)
TML syntax: `"Hello {name}!"` where expressions in `{}` are evaluated and concatenated.
- Lexer tokens: `InterpStringStart`, `InterpStringMiddle`, `InterpStringEnd` in `lexer/token.hpp`
- AST: `InterpolatedStringExpr` with `InterpolatedSegment` in `parser/ast.hpp`
- Parser: `parse_interp_string_expr()` in `parser/parser_expr.cpp`
- Type checker: `check_interp_string()` in `types/checker.cpp`
- IR builder: Generates `__string_format` call in `ir/builder_expr.cpp`
- LLVM codegen: `gen_interp_string()` in `codegen/llvm_ir_gen_expr.cpp` uses `str_concat`
- Tests: `LexerTest.Interpolated*`, `ParserTest.Interpolated*` tests

## Deferred Features - Implementation Guide

### Inline Modules
Syntax: `mod name { ... }` inline instead of `mod name;` file reference
Required changes:
1. **Parser**: Already supports inline module syntax
2. **Module resolver**: Handle inline vs file-based modules
3. **Type checker**: Scope handling for inline modules

### Doc Comments
Syntax: `/// Line doc` or `/** Block doc */`
Required changes:
1. **Lexer**: Capture doc comments as tokens (not discard)
2. **Parser**: Attach doc comments to declarations
3. **AST**: Add `doc: Option<String>` field to declarations
4. **IR**: Preserve documentation in metadata
5. **Doc generator**: Extract and format as HTML/Markdown

## Status

**Phase 1, 2 & 3**: COMPLETE - Core compiler features and string interpolation working
**Phase 4**: DEFERRED - Low priority (inline modules, doc comments), tracked for future work

This task is COMPLETE and ready for archival.
