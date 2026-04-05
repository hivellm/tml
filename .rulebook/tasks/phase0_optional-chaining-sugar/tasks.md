# Optional Chaining & Guard Clauses — Tasks

**Status:** 26/38 — Phase 1+2 DONE, Phase 3 lexer+parser+type checker+codegen DONE
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

### 4.1 CLAUDE.md updates
- [ ] Add `let-else` to the Key Design Decisions table (TML vs Rust comparison)
- [ ] Add `?.` to the Key Design Decisions table
- [ ] Add rule: "ALWAYS use `let-else` instead of nested `when` for Maybe unwrapping"
- [ ] Add rule: "Use `?.` for chained Maybe method calls (JSON, DB, HTTP)"
- [ ] Add anti-pattern example: nested `when` cascade (before) vs `let-else` (after)
- [ ] Update "Consult Language Reference" section with `let-else` and `?.` syntax

### 4.2 Language spec updates
- [ ] `docs/specs/05-TYPES.md` — Maybe section: add combinators, `let-else`, `?.`
- [ ] `docs/specs/03-STATEMENTS.md` — add `let-else` guard clause statement
- [ ] `docs/specs/04-EXPRESSIONS.md` — add `?.` optional chaining expression
- [ ] `docs/readme.md` — add `let-else` and `?.` to feature list

### 4.3 MCP docs index
- [ ] Add `let-else` to docs index so `mcp__tml__docs_search(query="let else guard")` finds it
- [ ] Add `?.` to docs index so `mcp__tml__docs_search(query="optional chaining")` finds it
- [ ] Add Maybe combinators (`and_then`, `map`, `filter`) to docs index

### 4.4 Claude rules
- [ ] Create `.claude/rules/use-let-else.md` — rule enforcing `let-else` over nested `when`
- [ ] Create `.claude/rules/optional-chaining.md` — rule for `?.` usage patterns
- [ ] Update `.claude/rules/consult-language-reference.md` — add `let-else` and `?.` to syntax pitfalls table

### 4.5 User guides and tutorials
- [ ] `docs/user/chNN-00-optional-chaining.md` — tutorial: Maybe handling patterns
  - Section 1: The problem (nested when cascade)
  - Section 2: Combinators (`and_then`, `map`, `filter`)
  - Section 3: Guard clauses (`let-else`)
  - Section 4: Optional chaining (`?.`)
  - Section 5: Real-world examples (JSON parsing, DB queries, HTTP handlers)
- [ ] Update `docs/user/` index with new chapter

### 4.6 Code migration
- [ ] Rewrite `docs/papers/llm-ir-debugging/scripts/*.tml` using `let-else` and `?.`
- [ ] Rewrite `lib/postgresql/src/connection.tml` to use `let-else` for Outcome handling
- [ ] Audit `lib/std/src/` for nested `when Maybe` patterns — refactor to `let-else`
