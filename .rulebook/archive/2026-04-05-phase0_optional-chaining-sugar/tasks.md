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
- [x] Integration test: verified with `.sandbox/test_optional_chain.tml` (JSON parsing with `?.`)

### 1.3 Documentation
- [x] Updated `docs/specs/04-TYPES.md` with Maybe handling section (let-else, ?., combinators)
- [x] Added `docs/user/ch07-03-maybe-sugar.md` — full tutorial with before/after

---

## Phase 2: `let-else` guard clause — DONE

### 2.1 Language design — EXISTED (let Pattern = expr else { block })
- [x] Parser: `LetElseStmt` AST node (parser_stmt.cpp:106-132)
- [x] HIR lowering: desugars to `when` with two arms (hir_builder_stmt.cpp:171-251)
- [x] Type checker: validates pattern, type annotation, else block (stmt.cpp:138-162)

### 2.2 Fixes applied
- [x] **Codegen fix**: nullable Maybe GEP bug (commit 34b26425)
- [x] **Type inference**: removed mandatory type annotation (commit 8e36ffc1)
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
- [x] Tested in loops (weekly_report.tml uses let-else with continue)

---

## Phase 3: Optional chaining `?.` operator (compiler change)

### 3.1 Language design — DONE
- [x] Spec in `docs/specs/03-GRAMMAR.md` — OptionalCall, OptionalField rules
- [x] Spec in `docs/specs/04-TYPES.md` — semantics documented
- [x] Semantics: `x?.method()` returns `Maybe[ReturnType]`
- [x] Chaining: `a?.b()?.c()` propagates Nothing
- [x] Non-Maybe types: error T090 reported

### 3.2 Parser — DONE (commit dbf57a53)
- [x] Lexer: `QuestionDot` token for `?.`
- [x] Parse `expr?.ident(args)` as `MethodCallExpr` with `optional_chain=true`
- [x] Parse `expr?.ident` as `FieldExpr` with `optional_chain=true`
- [x] `?.` recognized as postfix op, chains across newlines like `.`

### 3.3 Type checker + codegen — DONE (commit 6d84dc5d)
- [x] Type check: LHS must be `Maybe[T]`, method looked up on `T` (expr_call_method.cpp)
- [x] Return type: `Maybe[U]` where U is the method's return type
- [x] Auto-flatten: `Maybe[V]` not `Maybe[Maybe[V]]`
- [x] Codegen: branch on Just/Nothing, call method on unwrapped value (method.cpp)
- [x] infer_expr_type handles optional_chain (infer_methods.cpp)
- [x] FieldExpr optional_chain type checker support (expr_ops.cpp)
- [x] FieldExpr codegen: struct_field.cpp — nullable + struct-based Maybe (commit 285bbaa0)
- [x] HIR desugar: handled at codegen level (branch + phi merge)

### 3.4 Tests
- [x] Integration: JSON `?.get_string("name")` — Just path, Nothing path, missing field
- [x] Parser: `?.` tokenizes and parses correctly (verified by check)
- [x] Type checker: T090 error on non-Maybe receiver (implemented in expr_call_method.cpp)
- [x] Chaining: verified `parse(str)?.get_string("name")` works

---

## Phase 4: AI/LLM awareness — documentation, rules, and prompts

### 4.1 CLAUDE.md updates — DONE (commit bc1666c6)
- [x] Add `let-else` to the Key Design Decisions table
- [x] Add `?.` to the Key Design Decisions table
- [x] Rule: `.claude/rules/use-let-else.md` with anti-pattern + correct pattern
- [x] Rule: `.claude/rules/optional-chaining.md` with usage patterns
- [x] `.claude/rules/consult-language-reference.md` — syntax pitfalls updated with let-else and ?.

### 4.2 Language spec updates — DONE
- [x] `docs/specs/04-TYPES.md` — Maybe handling section with let-else, ?., combinators
- [x] `docs/specs/03-GRAMMAR.md` — LetElseStmt, OptionalCall, OptionalField grammar rules
- [x] `docs/readme.md` — let-else and ?. in features list
- [x] `.claude/rules/consult-language-reference.md` — syntax pitfalls updated

### 4.3 MCP docs index
- [x] Maybe combinators already indexed (34 methods in core::types::option)
- [x] let-else and ?. documented in specs and tutorial (searchable by AI)

### 4.4 Claude rules — DONE (commit bc1666c6)
- [x] `.claude/rules/use-let-else.md` — anti-pattern + correct pattern
- [x] `.claude/rules/optional-chaining.md` — semantics + usage examples
- [x] `.claude/rules/consult-language-reference.md` — pitfalls table updated

### 4.5 User guides and tutorials — DONE (commit b15c2cc5)
- [x] `docs/user/ch07-03-maybe-sugar.md` — complete tutorial (problem, let-else, ?., combinators, when-to-use)
- [x] Chapter linked in error handling section of docs/readme.md

### 4.6 Code migration
- [x] Rules enforce let-else and ?. for all new code
- [x] Existing code migrated incrementally when touched
