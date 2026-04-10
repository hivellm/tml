# Tasks: Frontend Integration — Wire TML Lexer/Parser into Compiler

**Status**: In Progress (8/18)
**Depends on**: phase13b (TML lexer), phase13c (TML parser), phase12f (hybrid pipeline framework)
**Blocks**: Era 1 Phase 2 (type checker porting)
**Duration**: 2–3 weeks
**Risk**: Medium — integration may reveal serialization edge cases

### Codegen fixes landed (prerequisite for Phase 3)

Five codegen bugs were found and fixed while integrating the frontend:

1. **try.cpp `current_block_`** (71ce608a) — PHI node predecessor stale after try-operator ok_block
2. **Heap[T] field drops** (42a7a85e) — use-after-free from auto-dropping copied Heap values (no move semantics)
3. **when-arm binding drops** (ab15c5d4) — double-free of enum scrutinee payload via arm binding aliases
4. **Enum alignment sizing** (21412f6d) — `compute_llvm_type_byte_size` and `calc_type_size` didn't account for struct field alignment padding → FuncDecl (152 bytes) stored in 136-byte Decl enum data field → buffer overrun. Fixed with alignment-aware computation in enum.cpp, llvm_types.cpp.
5. **Try-operator error propagation** (21412f6d) — `current_ret_type_` temporarily overwritten by let-statement struct literal resolution → try-operators panic instead of propagating errors. Fixed by using `func_ret_type_` (immutable). Also: extract error value before `emit_all_drops()` to prevent use-after-free.

Additional infrastructure: alloca hoisting pass (generate.cpp), `stack-probe-size` attribute for Windows, AST format switch ("MOD " → "AST ").

**REMAINING CRASH**: Function bodies still segfault at runtime — likely another enum/struct sizing mismatch in a different type (Expr, Stmt, or Pattern). Same root cause pattern as #4 above. Needs systematic audit of all enum types with struct-valued variants.

---

## Phase 1: TML Frontend Binary (4 items) — DONE

- [x] 1.1 Create `compiler-tml/src/main_frontend.tml` — standalone binary that reads .tml source, lexes, parses, serializes AST to stdout
- [x] 1.2 Wire: read source file path from CLI args → load file → tokenize → parse → serialize Module → write to stdout
- [x] 1.3 Error handling: if lex/parse fails, output error in structured format (JSON) to stderr, exit code 1
- [x] 1.4 Compile and verify: `tml build compiler-tml/src/main_frontend.tml` produces working binary

## Phase 2: C++ Deserializer Integration (4 items) — DONE

- [x] 2.1 `compiler/src/query/query_core.cpp` — Add ParseModuleTml subprocess integration for `--stage=parser:tml`
- [x] 2.2 Implement: spawn TML frontend process, pipe source path as arg, capture stdout as binary AST
- [x] 2.3 Deserialize binary AST to C++ `Module` struct using phase12e `serial::read_ast()`
- [x] 2.4 Wire `--stage=parser:tml` flag — CLI parsing already existed, query dispatch added

## Phase 3: Differential Testing — Full Suite (5 items)

**BLOCKED**: Frontend binary crashes on files with function bodies (segfault). Root cause: enum data field sizing doesn't account for alignment padding in all code paths. Const-only and use-only files parse and serialize successfully. Fix needed: systematic audit of `compute_llvm_type_byte_size` for all enum types with struct-valued variants (Decl fixed, Expr/Stmt/Pattern may also be affected).

- [ ] 3.1 Run test suite with `--stage=parser:tml` — record all failures
- [ ] 3.2 For each failure: categorize as lexer bug / parser bug / serialization bug / deserialization bug
- [ ] 3.3 Fix each bug in TML lexer/parser — one commit per fix
- [ ] 3.4 Re-run full suite — iterate until zero failures
- [ ] 3.5 IR-diff: compare LLVM IR output for ALL test files (C++ path vs TML frontend path) — must be identical

## Phase 4: Performance Validation (3 items)

- [ ] 4.1 Benchmark: compile 50 stdlib modules with C++ frontend, measure total time
- [ ] 4.2 Benchmark: compile same 50 modules with TML frontend (`--stage=parser:tml`), measure total time
- [ ] 4.3 Verify overhead < 5% — if exceeded, profile serialization/deserialization and optimize

## Phase 5: Switchover (2 items)

- [ ] 5.1 After all tests pass and performance validated: make `--stage=parser:tml` the default
- [ ] 5.2 Keep C++ lexer/parser as fallback with `--stage=parser:cpp` flag — do NOT delete yet

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
