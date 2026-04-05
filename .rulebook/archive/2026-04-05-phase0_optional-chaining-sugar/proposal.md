# Proposal: Optional Chaining & Guard Clauses — Eliminate Maybe cascade hell

## Why

TML code that works with `Maybe[T]` values produces deeply nested cascades:

```tml
when parse(line) {
    Just(json) => {
        when json.get_string("event") {
            Just(ev) => {
                if ev == "tool_call" {
                    when json.get_string("tool") {
                        Just(tool) => {
                            // 5 levels deep just to read 2 fields
                        }, Nothing => {}
                    }
                }
            }, Nothing => {}
        }
    }, Nothing => {}
}
```

This is the #1 readability problem in TML today. Every `Maybe` access adds a nesting level. Real-world code (JSONL parsing, HTTP handlers, DB queries) needs 3-5 unwraps per operation, creating 10-15 levels of indentation.

JavaScript solved this with optional chaining (`?.`), Rust with `?` operator and `let-else`, Swift with `guard let`, Kotlin with `?.let {}`.

## Analysis of Approaches

### Option A: `let?` guard clause (RECOMMENDED — highest impact, lowest complexity)

**Syntax:**
```tml
let? json = parse(line) else continue
let? ev = json.get_string("event") else continue
if ev != "tool_call" { continue }
let? tool = json.get_string("tool") else continue
// flat code — no nesting
tool_counts.set(tool, tool_counts.get_or(tool, 0) + 1)
```

**Semantics:**
- `let? x = expr else <action>` — if expr is `Nothing`, execute action (continue/return/break)
- If expr is `Just(v)`, bind `v` to `x` and continue
- The `else` clause MUST diverge (return, continue, break, panic)

**Implementation:**
- Parser: new `let?` token in declaration parsing
- HIR: desugars to `when expr { Just(x) => ..., Nothing => <action> }`
- Zero runtime cost — pure syntactic sugar
- No new types, no new IR instructions

**Comparable:** Rust `let Some(x) = expr else { continue }`, Swift `guard let x = expr else { return }`

### Option B: Optional chaining operator (`?.`)

**Syntax:**
```tml
let tool = json?.get_string("event")?.get_string("tool")
// tool is Maybe[Str] — Nothing if any step fails
```

**Semantics:**
- `x?.method()` — if x is `Nothing`, returns `Nothing`. If `Just(v)`, calls `v.method()`
- Chains propagate `Nothing` automatically
- Result type is always `Maybe[T]`

**Implementation:**
- Parser: new `?.` operator (binds tighter than `.`)
- HIR: desugars to nested `when` expressions
- Type checker: infer through Maybe chain
- Moderate complexity — needs to handle method calls on unwrapped types

**Comparable:** JavaScript `obj?.field?.method()`, Kotlin `obj?.field?.method()`, C# `obj?.Field?.Method()`

### Option C: `and_then` / `map` methods on Maybe (SIMPLEST — library only)

**Syntax:**
```tml
parse(line)
    .and_then(do(json) json.get_string("event"))
    .filter(do(ev) ev == "tool_call")
    .and_then(do(_) json.get_string("tool"))
    .map(do(tool) { process(tool) })
```

**Implementation:**
- Pure library change — add methods to `Maybe[T]` in `core/types/option.tml`
- `and_then(f: func(T) -> Maybe[U]) -> Maybe[U]`
- `map(f: func(T) -> U) -> Maybe[U]`
- `filter(f: func(T) -> Bool) -> Maybe[T]`
- `unwrap_or(default: T) -> T`
- `or_else(f: func() -> Maybe[T]) -> Maybe[T]`

**Problem:** Still verbose with closures, and `json` not accessible in later steps.

### Option D: Pipeline operator (`|>`)

**Syntax:**
```tml
line |> parse |> get_event |> filter_tool_call |> process
```

**Implementation:**
- Parser: new `|>` operator
- Desugars to function application: `f(g(x))` → `x |> g |> f`
- Moderate complexity

**Problem:** Doesn't solve Maybe chaining specifically — orthogonal feature.

### Option E: `if let` expression (Rust-style)

**Syntax:**
```tml
if let Just(json) = parse(line) {
    if let Just(ev) = json.get_string("event") {
        // still nested, but cleaner than when
    }
}
```

**Problem:** Still nests. Only slightly better than `when`.

## Recommendation

**Phase 1: Option C (library)** — add `and_then`, `map`, `filter`, `unwrap_or` to Maybe. Zero compiler changes, immediate value.

**Phase 2: Option A (`let?`)** — biggest readability win, moderate compiler change (parser + HIR desugaring). Eliminates ALL nesting.

**Phase 3: Option B (`?.`)** — optional chaining for method calls. Most ergonomic for JSON/API code.

## Impact

- Affected specs: `docs/specs/05-TYPES.md` (Maybe section), new spec for `let?`
- Affected code:
  - Phase 1: `lib/core/src/types/option.tml` (add methods)
  - Phase 2: `compiler/src/parser/parser_stmt.cpp`, `compiler/src/hir/hir_builder.cpp`
  - Phase 3: `compiler/src/parser/parser_expr.cpp`, `compiler/src/types/checker.cpp`
- Breaking change: NO (all additive)
- User benefit: 50-80% reduction in nesting for Maybe-heavy code (JSON parsing, DB queries, HTTP handlers)
