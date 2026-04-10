# Tasks: Frontend Integration — Wire TML Lexer/Parser into Compiler

**Status**: In Progress (8/18)
**Depends on**: phase13b (TML lexer), phase13c (TML parser), phase12f (hybrid pipeline framework)
**Blocks**: Era 1 Phase 2 (type checker porting)
**Duration**: 2–3 weeks
**Risk**: Medium — integration may reveal serialization edge cases

---

## Phase 1: TML Frontend Binary (4 items)

- [x] 1.1 Create `compiler-tml/src/main_frontend.tml` — standalone binary that reads .tml source, lexes, parses, serializes AST to stdout
- [x] 1.2 Wire: read source file path from CLI args → load file → tokenize → parse → serialize Module → write to stdout
- [x] 1.3 Error handling: if lex/parse fails, output error in structured format (JSON) to stderr, exit code 1
- [x] 1.4 Compile and verify: `tml build compiler-tml/src/main_frontend.tml` produces working binary

## Phase 2: C++ Deserializer Integration (4 items)

- [x] 2.1 `compiler/src/query/query_context.cpp` — Add `ParseModuleTml` query that invokes TML frontend binary
- [x] 2.2 Implement: spawn TML frontend process, pipe source path as arg, capture stdout as binary AST
- [x] 2.3 Deserialize binary AST to C++ `Module` struct using phase12e deserializer
- [x] 2.4 Wire `--stage=parser:tml` flag to dispatch to `ParseModuleTml` instead of `ParseModule`

## Phase 3: Differential Testing — Full Suite (5 items)

**RESOLVED**: Parser crash fixed (commit 42a7a85e) — root cause was use-after-free from Heap[T] field drops without move semantics. Parser now returns errors gracefully instead of crashing.
**REMAINING**: Format mismatch — TML serializer writes "MOD " format but C++ `read_ast` expects "AST " format. Need to use `compiler::serial::ast::write_module` (TmlModule mirror types) or write a conversion layer.

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
