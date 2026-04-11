# Ambiguity Review — Language Ergonomics Findings vs TML Design Principles

**Date**: 2026-04-11
**References**: ADR-008, RFC-0001, RFC-0002, docs/grammar/tml.peg

## TML Design Constraints (Non-Negotiable)

| Constraint | Source | Summary |
|------------|--------|---------|
| **LL(1) grammar** | ADR-008 | Every production determined by single-token lookahead |
| **Keywords over symbols** | RFC-0002 | `do`, `when`, `to`/`through`, `and`/`or` |
| **Mandatory type annotations** | ADR-008 | All declarations must have explicit types |
| **No implicit coercions** | RFC-0001 | `as` keyword required for all casts |
| **`to`/`through` for ranges** | RFC-0002 §1.4, §3.3 | Not `..`/`..=` |
| **LLM-centric** | ADR-008 | Token-by-token determinism |
| **Deterministic desugaring** | RFC-0002 | Surface → IR must be unambiguous |

---

## Finding-by-Finding Verdict

### F01: For-each Loops & Ranges — OK (ALREADY IN SPEC)

**Verdict**: No ambiguity. **Already specified in RFC-0002 §2.3 and §3.4.**

The RFC already defines:
```peg
ForExpr <- 'for' Pattern 'in' Expr Block
```

And `for` is already a reserved keyword (RFC-0002 §1.1). The desugaring rules
for `for i in 0 to 10` and `for item in collection` are fully specified in §3.4.

**However**: The finding proposed `..` and `..=` range syntax — this **VIOLATES**
TML principles. TML uses `to` (exclusive) and `through` (inclusive):

| Finding proposed | TML correct syntax |
|------------------|--------------------|
| `0..n` | `0 to n` |
| `0..=n` | `0 through n` |

**Action**: Update F01 to use `to`/`through` instead of `..`/`..=`.
This is a **compiler implementation gap** (the syntax is spec'd but not yet
implemented in the C++ compiler), not a language design gap.

---

### F02: Struct Update Syntax — NEEDS CARE

**Verdict**: The `..expr` syntax has **potential LL(1) ambiguity**.

`..` is already a reserved operator in TML (RFC-0002 §1.4: `..  ...`).
However, inside a struct literal `{ field: val, ..expr }`, the parser sees `..`
after a `,` inside `{ }`. At that position:

- `..` is unambiguous because the only other thing that can follow `,` inside
  a struct literal is an identifier (field name). `..` is not an identifier.
- Single-token lookahead: after `,`, peek `..` → struct update; peek `Ident` → field.

**OK with LL(1).** No ambiguity.

The alternative `with` keyword is also safe but adds a 58th keyword. Given `..`
is already reserved and unambiguous here, `..expr` is cleaner.

**Action**: F02 is safe. No changes needed.

---

### F03: Pattern Guards — OK (ALREADY IN SPEC)

**Verdict**: No ambiguity. **Already specified in RFC-0002 §2.5.**

The PEG grammar already defines:
```peg
AndPattern <- PrimaryPattern ('if' Expr)?
```

Pattern guards with `if` after a pattern are part of the spec. This is a
compiler implementation gap, not a language design issue.

`if let` is also already in the grammar (RFC-0002 §2.3):
```peg
IfExpr <- ... / 'if' 'let' Pattern '=' Expr 'then' Expr ('else' Expr)?
```

**Action**: F03 is safe. Compiler needs to implement what's already spec'd.

---

### F04: Operator Overloading — OK, NO SYNTAX CHANGE

**Verdict**: No ambiguity. Operator overloading is a **semantic** change, not
a **syntactic** one.

The parser already handles `a + b` as `BinaryExpr(Add, a, b)`. Operator
overloading changes the **type checker** and **codegen** to resolve `+` on
non-primitive types to a behavior method call. The grammar doesn't change.

`a + b` is LL(1) today and remains LL(1) with operator overloading.

**Action**: F04 is safe. No syntax changes, purely type-checker + codegen.

---

### F05: Derive Macros — NEEDS CARE

**Verdict**: `@derive(...)` uses the existing decorator syntax. No ambiguity.

RFC-0002 §3.7 already defines decorator syntax:
```tml
@test
@timeout(5000)
func my_test() { ... }
```

`@derive(Serialize)` follows this exact pattern. The `@` token at statement
position is unambiguous (not used anywhere else). LL(1) safe.

However: **`@repr(u8)` on enums** requires the compiler to track layout
attributes and feed them into codegen. This is a semantic addition, no grammar
change.

**Action**: F05 is safe. Uses existing decorator grammar.

---

### F06: Bool Struct Fields — OK, NO SYNTAX CHANGE

**Verdict**: No ambiguity. This is a **codegen fix** (i1 → i8 in struct layouts).
No syntax or grammar changes needed.

**Action**: F06 is safe. Pure compiler internals.

---

### F07: Closure Shorthand — AMBIGUITY RISK

**Verdict**: **Multiple sub-proposals have different risk levels.**

**A. Type inference for closure parameters** — VIOLATES mandatory type annotations.

ADR-008 states: "Mandatory type annotations on all declarations."
RFC-0002 §2.3 shows closure syntax requires parameter types in the grammar.

Allowing `do(x) { x + 1 }` without type annotation for `x` breaks the
LL(1) parsing guarantee because the parser can no longer determine the
parameter type at parse time. The type checker would need to infer it
from context (bidirectional type inference).

**However**: This is a type-checker concern, not a parser concern. The parser
can still parse `do(x) { ... }` as a closure with untyped parameter — the
grammar production is:

```peg
ClosureParam <- 'mut'? Ident (':' Type)?
```

If `: Type` is optional, the parser sees `Ident` then peeks `,` or `)` and
knows there's no type. **LL(1) is preserved.**

The real question is whether **mandatory type annotations** is absolute or
limited to `let` declarations. Closures in a typed context (e.g., passed to
a function with known signature) are a reasonable exception.

**Risk**: Medium. Technically LL(1)-safe but relaxes a design principle.

**B. Implicit return for single-expression closures** — SAFE.

`do(x: I64) { x + 1 }` where the body is a single expression treated as
return value. This is a semantic rule, not a grammar change. The parser
still parses the block normally; the type-checker/codegen decides whether
the last expression is an implicit return.

**C. Placeholder syntax `_ > 0`** — AMBIGUITY.

`_` is already the wildcard pattern (RFC-0002 §2.5). Using `_` as a
placeholder in expressions creates ambiguity:

```tml
list.filter(_ > 0)    // Is _ a wildcard pattern or placeholder?
let _ = value         // Already means "discard"
```

At expression position, `_` would need to be both "discard binding" and
"anonymous closure parameter". This is **NOT LL(1)** — the parser cannot
determine the meaning of `_` without looking at context.

**Action**: F07-A is debatable (type inference for closure params). F07-B is
safe. F07-C (`_` placeholder) **MUST BE REMOVED** — creates ambiguity.

---

### F08: Wildcard Match Arms — OK (ALREADY IN SPEC)

**Verdict**: No ambiguity. `_` in pattern position is already in the grammar:

```peg
WildcardPattern <- '_'
```

**Action**: F08 is safe. Already spec'd.

---

### F09: Module Circular Imports — OK, NO SYNTAX CHANGE

**Verdict**: No ambiguity. Module resolution is a **semantic** phase (happens
after parsing). Allowing circular imports between sibling modules changes the
module resolver, not the grammar.

**Multi-import syntax** `use mod::{A, B, C}`:
- After `::`, peek `{` → multi-import; peek `Ident` → single path.
- **LL(1) safe.** Single-token lookahead distinguishes the two cases.

**Glob import** `use mod::*`:
- After `::`, peek `*` → glob; peek `Ident` → named import.
- **LL(1) safe.**

**Action**: F09 is safe. Both syntax additions are LL(1).

---

### F10: Lowlevel Abstractions — MIXED

**Verdict**:

**A. Inline lowlevel expressions** (`lowlevel ptr_read[I64](...)`) — AMBIGUITY.

Currently `lowlevel` is always followed by `{`. Removing the braces makes
the parser unable to determine where the lowlevel expression ends:

```tml
let x = lowlevel ptr_read[I64](p) + 1
//      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// Is "+ 1" inside or outside the lowlevel?
```

The `{ }` block delimiter is what makes `lowlevel` LL(1)-parseable.
Removing it creates unbounded lookahead to find the expression boundary.

**MUST KEEP `{ }` for lowlevel blocks.**

**B. Pointer offset syntax** — Safe if done as a method/function, not new syntax.

**C. @packed struct** — Safe. Uses existing decorator syntax (`@packed`).

**Action**: F10-A (inline lowlevel) **MUST BE REMOVED** — breaks LL(1).
F10-B and F10-C are safe.

---

### F11: String Slicing Syntax — AMBIGUITY RISK

**Verdict**: `s[2..5]` uses `..` which conflicts with TML's `to`/`through`
range keywords.

TML's range syntax is `2 to 5` (exclusive) and `2 through 5` (inclusive).
Proposing `s[2..5]` introduces a **second range syntax** (`..`) that contradicts
the existing design choice.

The correct TML form would be:
```tml
let sub = s[2 to 5]       // slice from 2 to 5 (exclusive)
let sub = s[2 through 5]  // slice from 2 to 5 (inclusive)
```

Inside `[`, the parser sees `Expr`, which can contain `to`/`through` as
range constructors. This is **LL(1) safe** — `to` and `through` are keywords,
unambiguous at any position.

However: `s[2 to 5]` requires `Index` behavior with `Range` argument.
That's F04 (operator overloading) applied to `[]`. Dependency, not ambiguity.

**Action**: F11 must use `to`/`through`, not `..`/`..=`. Update the finding.

---

### F12: Generic Monomorphization — OK, NO SYNTAX CHANGE

**Verdict**: No ambiguity. Monomorphization is a compiler backend optimization.
No grammar changes.

**Action**: F12 is safe.

---

### F13: Safe Numeric Conversions — AMBIGUITY RISK

**Verdict**: **Implicit widening violates "no implicit coercions".**

RFC-0001 and ADR-008 require explicit `as` for all type conversions.
Allowing implicit `I32 → I64` widening means:

```tml
let x: I64 = some_i32_value  // Would this compile without `as`?
func foo(n: I64) { }
foo(42)  // 42 is I32 by default — implicit coercion to I64?
```

This creates a class of silent coercions that contradicts the "explicit
conversions" principle. It also makes integer literal inference context-dependent
(is `42` an I32 or I64?), which complicates the type checker.

**Safe parts**: `.abs()`, `.min()`, `.max()` as built-in methods on integer
types — these are method calls, not coercions. `Into`/`TryInto` behaviors
are also fine (explicit method calls).

**Action**: Remove implicit widening from F13. Keep `.abs()` and conversion
behaviors. Explicit `as` must remain for all numeric conversions.

---

### F14: Trait Aliases — OK, NO AMBIGUITY

**Verdict**: `behavior Numeric = Add + Sub + Mul` introduces no parsing
ambiguity.

After `behavior Ident`, the parser peeks:
- `{` → behavior definition (existing)
- `=` → behavior alias (new)

Single-token lookahead. **LL(1) safe.**

**Action**: F14 is safe.

---

### F15: Destructuring Assignment — OK (ALREADY IN SPEC)

**Verdict**: Struct patterns in `let` are already in the grammar:

```peg
LetStmt <- 'let' 'mut'? Pattern (':' Type)? '=' Expr ';'?
StructPattern <- TypePath '{' FieldPatterns? '}'
```

`let ParsedExpr { expr: left, pos: p } = ...` is parseable today by the
grammar spec. This is a compiler implementation gap.

**Action**: F15 is safe. Already spec'd.

---

## Summary

| Finding | Verdict | Issue |
|---------|---------|-------|
| F01 Loop/Iterator | **ALREADY SPEC'D** | Uses `..` — must use `to`/`through` |
| F02 Struct Update | **SAFE** | `..expr` is LL(1) |
| F03 Pattern Guards | **ALREADY SPEC'D** | `if` guard already in grammar |
| F04 Operator Overload | **SAFE** | Semantic only, no grammar change |
| F05 Derive Macros | **SAFE** | Uses existing `@decorator` syntax |
| F06 Bool Fields | **SAFE** | Codegen fix, no syntax |
| F07 Closure Shorthand | **MIXED** | A: debatable, B: safe, C: **AMBIGUOUS** (`_` conflict) |
| F08 Wildcard Match | **ALREADY SPEC'D** | `_` already in pattern grammar |
| F09 Module Cycles | **SAFE** | Semantic + LL(1)-safe syntax additions |
| F10 Lowlevel | **MIXED** | A: **BREAKS LL(1)** (no braces), B-C: safe |
| F11 String Slicing | **MUST FIX** | Uses `..` — must use `to`/`through` |
| F12 Monomorphization | **SAFE** | Backend optimization |
| F13 Numeric Conv. | **MIXED** | Implicit widening **VIOLATES** no-coercion rule |
| F14 Trait Aliases | **SAFE** | LL(1)-safe `=` after `behavior Name` |
| F15 Destructuring | **ALREADY SPEC'D** | Struct patterns already in grammar |

## Required Changes to Findings

### Must fix (violates principles):
1. **F01**: Replace `0..n` / `0..=n` with `0 to n` / `0 through n`
2. **F07-C**: Remove `_` placeholder syntax entirely (ambiguous with wildcard)
3. **F10-A**: Remove inline `lowlevel` (breaks LL(1) block boundary)
4. **F11**: Replace `s[2..5]` with `s[2 to 5]`
5. **F13**: Remove implicit widening; keep only `.abs()` and `Into`/`TryInto`

### Reclassify (already in spec, just not implemented):
6. **F01**: for-in loops → compiler implementation task, not language design
7. **F03**: Pattern guards → compiler implementation task
8. **F08**: Wildcard `_` → compiler implementation task
9. **F15**: Destructuring → compiler implementation task
