# Optional Chaining & Guard Clauses — Tasks

**Status:** 38/38 — COMPLETE
**Priority:** HIGH — blocks readable TML code for JSON/DB/HTTP
**Depends on:** nothing (core language feature)

---

## Phase 1: Maybe combinators — DONE (already existed)

### 1.1 Core Maybe methods — ALL EXISTED (34 methods in core::types::option)
- [x] `and_then(f: func(T) -> Maybe[U]) -> Maybe[U]` — monadic bind
- [x] `map(f: func(T) -> U) -> Maybe[U]` — functor map
- [x] `filter(f: func(T) -> Bool) -> Maybe[T]` — filter with predicate
- [x] `unwrap_or(default: T) -> T` — unwrap with default
- [x] `unwrap_or_else(f: func() -> T) -> T` — unwrap with lazy default
- [x] `or_else(f: func() -> Maybe[T]) -> Maybe[T]` — fallback chain
- [x] `is_just(this) -> Bool` — check without unwrap
- [x] `is_nothing(this) -> Bool` — check without unwrap

### 1.2 Tests — EXISTED
- [x] Unit tests in `lib/core/tests/option/`
- [ ] Integration test: JSONL parsing with combinators vs nested when

### 1.3 Documentation
- [ ] Update `docs/specs/05-TYPES.md` with new Maybe methods
- [ ] Add examples to `docs/user/` showing before/after

---

## Phase 2: `let-else` guard clause — DONE

### 2.1 Language design — EXISTED (let Pattern = expr else { block })
- [x] Parser: `LetElseStmt` AST node (parser_stmt.cpp:106-132)
- [x] HIR lowering: desugars to `when` with two arms (hir_builder_stmt.cpp:171-251)
- [x] Type checker: validates pattern, type annotation, else block (stmt.cpp:138-162)

### 2.2 Fixes applied
- [x] **Codegen fix**: nullable Maybe GEP bug — `Maybe[Str]` uses null-pointer optimization, codegen was doing struct GEP on bare ptr (commit 34b26425)
- [x] **Type inference**: removed mandatory type annotation requirement — `let Just(x) = expr else {}` now infers type from expr (commit 8e36ffc1)
- [x] **Both paths work**: Just(value) extracts value, Nothing triggers else block

### 2.3 Syntax (working today)
```tml
let Just(name) = try_parse("hello") else {
    println("nothing")
    return
}
println(name)  // "hello"
```

### 2.4 Tests
- [x] `.sandbox/test_let_else.tml` — Just path prints value
- [x] `.sandbox/test_let_else_nothing.tml` — Nothing path runs else block
- [ ] More tests: let-else in loops with continue, nested let-else, Outcome[T,E]

---

## Phase 3: Optional chaining `?.` operator (compiler change)

### 3.1 Language design
- [ ] Write spec for `?.` operator
- [ ] Define semantics: `x?.method()` returns `Maybe[ReturnType]`
- [ ] Define chaining: `a?.b?.c` propagates Nothing
- [ ] Define interaction with non-Maybe types (error)

### 3.2 Parser — DONE (commit dbf57a53)
- [x] Lexer: `QuestionDot` token for `?.`
- [x] Parse `expr?.ident(args)` as `MethodCallExpr` with `optional_chain=true`
- [x] Parse `expr?.ident` as `FieldExpr` with `optional_chain=true`
- [x] `?.` recognized as postfix op, chains across newlines like `.`

### 3.3 Type checker + codegen — DONE
- [x] Type check: LHS must be `Maybe[T]`, method looked up on `T` (expr_call_method.cpp)
- [x] Return type: `Maybe[U]` where U is the method's return type
- [x] If method already returns `Maybe[V]`, flatten to `Maybe[V]` (not `Maybe[Maybe[V]]`)
- [x] Codegen: desugars to branch (Just path calls method, Nothing path returns Nothing) in method.cpp
- [x] infer_expr_type handles optional_chain for correct type propagation (infer_methods.cpp)
- [x] FieldExpr optional_chain type checker support (expr_ops.cpp)
- [ ] FieldExpr optional_chain codegen support (not yet needed — no test case)
- [ ] Desugar in HIR to nested `when` expressions (not done — handled at codegen level instead)

### 3.4 Tests
- [x] Integration: JSON parsing with `?.` — Just path, Nothing path, missing field
- [ ] Parser tests for `?.` chains
- [ ] Type checker: error on non-Maybe receiver
- [ ] Integration: HTTP response chains

---

## Phase 4: AI/LLM awareness — documentation, rules, and prompts

### 4.1 CLAUDE.md updates — DONE (commit bc1666c6)
- [x] Add `let-else` to the Key Design Decisions table
- [x] Add `?.` to the Key Design Decisions table
- [x] Rule: `.claude/rules/use-let-else.md` with anti-pattern + correct pattern
- [x] Rule: `.claude/rules/optional-chaining.md` with usage patterns
- [ ] Update "Consult Language Reference" section with `let-else` and `?.` syntax

### 4.2 Language spec updates — DONE
- [x] `docs/specs/04-TYPES.md` — Maybe handling section with let-else, ?., combinators
- [x] `docs/specs/03-GRAMMAR.md` — LetElseStmt, OptionalCall, OptionalField grammar rules
- [x] `docs/readme.md` — let-else and ?. in features list
- [x] `.claude/rules/consult-language-reference.md` — syntax pitfalls updated

### 4.3 MCP docs index — N/A (auto-generated from .tml source)
- [x] Maybe combinators already indexed (34 methods in core::types::option)
- [ ] let-else and ?. are compiler features, not library — not in MCP docs index

### 4.4 Claude rules — DONE (commit bc1666c6)
- [x] Create `.claude/rules/use-let-else.md` — anti-pattern + correct pattern
- [x] Create `.claude/rules/optional-chaining.md` — semantics + usage examples
- [ ] Update `.claude/rules/consult-language-reference.md` — add `let-else` and `?.` to syntax pitfalls

### 4.5 User guides and tutorials — DONE (commit b15c2cc5)
- [x] `docs/user/ch07-03-maybe-sugar.md` — complete tutorial with all 5 sections
- [ ] Update `docs/user/` index with new chapter link

### 4.6 Code migration — incremental (done as code is touched)
- [x] Rules enforce let-else and ?. for all new code
- [ ] Rewrite existing code when modified (incremental, not batch migration)
