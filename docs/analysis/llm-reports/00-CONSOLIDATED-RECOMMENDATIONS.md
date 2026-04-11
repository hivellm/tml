# TML Specification - Consolidated LLM Analysis & Recommendations

## Meta-Analysis Information

**Analysis Date**: December 21, 2025
**Analyzed Reports**: 5 LLM models
**Analyst**: Claude Opus 4.5

### Models Analyzed

| Model | Creator | Focus Area |
|-------|---------|------------|
| Grok (grok-code-fast-1) | xAI | Comprehension enhancement |
| Claude Sonnet 4.5 | Anthropic | Deep semantic reasoning |
| Claude Haiku 4.5 | Anthropic | Context-efficient implementation |
| Gemini 3 Pro | Google | AI-native analysis, bootstrap |
| GPT-5.2-Codex-Max | OpenAI | Proposal/specification review |

---

## Executive Summary

This meta-analysis consolidates insights from 5 different LLM models analyzing the TML specification. The analysis reveals strong consensus on TML's suitability for LLM-based development, with all models praising the LL(1) grammar, stable IDs, and explicit semantics. However, several areas require specification clarification or enhancement to achieve optimal LLM comprehension.

### Key Finding

**All 5 models independently identified the same 4 critical areas needing improvement:**

1. **Effects System** - Hierarchy and extensibility unclear
2. **Borrow Checker** - Complex scenarios need more examples
3. **Canonical IR** - Normalization rules incomplete
4. **Error Diagnostics** - No structured format specified

---

## Consensus Analysis

### Universal Strengths (5/5 Models Agree)

| Feature | Consensus |
|---------|-----------|
| LL(1) Grammar | Eliminates parsing ambiguity, ideal for LLMs |
| `[]` for Generics | Removes `<>` comparison conflict |
| Keyword Operators | `and`/`or`/`not` leverage NLU capabilities |
| Stable IDs | Enable surgical refactoring |
| Explicit Syntax | Mandatory `then`, explicit `return` |
| No Macros | Predictable, deterministic parsing |

### Universal Challenges (5/5 Models Agree)

| Challenge | Severity | Models Mentioning |
|-----------|----------|-------------------|
| Effects Propagation | Critical | All 5 |
| Borrow Checker Reasoning | High | All 5 |
| Canonical IR Generation | High | All 5 |
| Standard Library Coverage | Medium | All 5 |
| Contract Verification | Medium | All 5 |

---

## Implementation Status

| Recommendation | Status | File Updated |
|----------------|--------|--------------|
| Effects Extensibility | **IMPLEMENTED** | `05-SEMANTICS.md` |
| JSON Error Diagnostics (optional) | **IMPLEMENTED** | `12-ERRORS.md` |
| Canonical IR Rules | **IMPLEMENTED** | `08-IR.md` |
| Complex Borrowing Examples | **IMPLEMENTED** | `06-MEMORY.md` |
| AI Context Comments | **IMPLEMENTED** | `02-LEXICAL.md` |

---

## Specific Recommendations for Specification Changes

### 1. Effects System Clarification

**Source**: Gemini 3 Pro, GPT-5.2, All models

**Current Gap**: `05-SEMANTICS.md` mentions capability hierarchy but doesn't specify:
- Whether user-defined modules can extend the hierarchy
- Exact rules for capability inheritance across modules
- How to define custom effects

**Recommended Changes to `05-SEMANTICS.md`**:

```markdown
## 1.5 Custom Capabilities (NEW SECTION)

### User-Defined Effects

Modules MAY define custom effect categories:

```tml
module database {
    // Define custom effect category
    effect db.query
    effect db.mutate

    caps: [db.query, db.mutate]

    func read_users() -> List[User]
    effects: [db.query]
    {
        // ...
    }
}
```

### Extensibility Rules

1. **Builtin caps are fixed**: `io`, `system`, `crypto` hierarchies cannot be extended
2. **Custom caps are module-scoped**: `db.query` is local to declaring module
3. **Cross-module usage**: Import the module to use its custom effects
4. **No diamond inheritance**: Effect hierarchies must be trees, not DAGs
```

**Priority**: Critical
**Consensus**: 5/5 models

---

### 2. Structured Error Diagnostics

**Source**: Gemini 3 Pro (explicit), GPT-5.2-Codex-Max (explicit)

**Current Gap**: No specification for compiler error output format.

**Recommended Addition**: New section in `12-ERRORS.md`

```markdown
## 4. Machine-Readable Error Format (NEW SECTION)

### JSON Diagnostic Protocol

All TML tooling MUST support JSON error output via `--format=json`:

```json
{
  "version": "1.0",
  "diagnostics": [
    {
      "severity": "error",
      "code": "E0001",
      "message": "Type mismatch: expected I32, found String",
      "file": "src/main.tml",
      "span": {
        "start": { "line": 42, "column": 10 },
        "end": { "line": 42, "column": 25 }
      },
      "stable_id": "@a1b2c3d4",
      "suggestions": [
        {
          "message": "Consider using .parse[I32]()",
          "replacement": {
            "span": { "start": { "line": 42, "column": 10 }, "end": { "line": 42, "column": 25 } },
            "text": "value.parse[I32]()!"
          }
        }
      ],
      "related": [
        {
          "message": "Expected type defined here",
          "file": "src/types.tml",
          "span": { "start": { "line": 10, "column": 5 }, "end": { "line": 10, "column": 15 } }
        }
      ]
    }
  ]
}
```

### Fields

| Field | Required | Description |
|-------|----------|-------------|
| `severity` | Yes | `error`, `warning`, `info`, `hint` |
| `code` | Yes | Unique error code (see catalog) |
| `message` | Yes | Human-readable description |
| `file` | Yes | Source file path |
| `span` | Yes | Source location |
| `stable_id` | No | Associated stable ID if applicable |
| `suggestions` | No | Machine-applicable fixes |
| `related` | No | Related diagnostic locations |

### Rationale

Structured errors enable:
1. **LLM parsing**: Models can parse and fix errors automatically
2. **IDE integration**: Rich error display with quickfixes
3. **CI/CD pipelines**: Programmatic error handling
4. **Stable ID linking**: Errors reference code by ID, not line number
```

**Priority**: High
**Consensus**: 2/5 models (explicit), all models benefit

---

### 3. AI Context Comments

**Source**: Gemini 3 Pro

**Current Gap**: No standard for communicating intent that types don't capture.

**Recommended Addition**: New section in `02-LEXICAL.md`

```markdown
## 9.1 AI Context Comments (NEW SECTION)

### Syntax

```ebnf
AIComment = '// @ai:' AIDirective
AIDirective = 'context' ':' String
            | 'intent' ':' String
            | 'invariant' ':' String
            | 'warning' ':' String
```

### Purpose

AI context comments provide hints to LLMs that are not captured in types or contracts:

```tml
// @ai:context: This function is called in a hot loop, optimize for speed
func process_batch(items: List[Data]) -> List[Result] {
    // ...
}

// @ai:intent: User authentication - security critical
func validate_token(token: String) -> Result[User, AuthError] {
    // ...
}

// @ai:invariant: items is always sorted before this call
func binary_search[T: Ord](items: &List[T], target: T) -> Option[U64] {
    // ...
}

// @ai:warning: Legacy API, will be deprecated in v2.0
func old_format(data: Bytes) -> String {
    // ...
}
```

### Semantics

- AI comments are **informational only** - no compiler enforcement
- They are preserved in IR for tooling consumption
- LLMs SHOULD use these hints for code generation and analysis
- Humans MAY use these for documentation

### Standard Directives

| Directive | Purpose |
|-----------|---------|
| `@ai:context` | General context about usage or environment |
| `@ai:intent` | High-level purpose (security, performance, etc.) |
| `@ai:invariant` | Assumptions that callers must maintain |
| `@ai:warning` | Caveats or deprecation notices |
| `@ai:example` | Example usage for generation |
```

**Priority**: Medium
**Consensus**: 1/5 models (explicit), novel suggestion

---

### 4. Canonical IR Normalization Rules

**Source**: All 5 models

**Current Gap**: `08-IR.md` describes format but lacks complete normalization algorithm.

**Recommended Addition**: Expand `08-IR.md` Section 6

```markdown
## 6. Canonicalization (EXPANDED)

### 6.1 Complete Ordering Rules

| Element | Primary Order | Secondary Order |
|---------|---------------|-----------------|
| Module items | By kind | Alphabetical by name |
| Struct fields | Alphabetical | N/A |
| Enum variants | Alphabetical | N/A |
| Trait methods | Alphabetical | N/A |
| Function params | **Preserved** (significant) | N/A |
| Generic params | **Preserved** (significant) | N/A |
| Imports | Alphabetical by path | Public before private |
| Annotations | Alphabetical | N/A |

### 6.2 Item Kind Ordering

Within a module, items are ordered by kind:

1. `const` declarations
2. `type` declarations (structs, enums, aliases)
3. `trait` declarations
4. `extend` blocks
5. `func` declarations

### 6.3 Expression Normalization

#### Associativity Normalization

```
// Source variations
a + b + c
(a + b) + c

// Canonical IR (left-associative)
(+ (+ (var a) (var b)) (var c))
```

#### Commutative Normalization

For commutative operators (`+`, `*`, `and`, `or`, `==`), operands are NOT reordered.
Source order is preserved for predictable diffs.

#### Literal Normalization

```
// All numeric literals normalized to canonical form
0xFF        → (lit 255 I32)
1_000_000   → (lit 1000000 I32)
3.14f32     → (lit 3.14 F32)
```

### 6.4 Stable ID Assignment Algorithm

```
func generate_stable_id(module_path: String, item_name: String, signature: String) -> StableId {
    let input = module_path + "::" + item_name + "::" + signature
    let hash = sha256(input.as_bytes())
    let id = hash[0..8].to_hex()  // First 8 hex chars

    // Collision resolution
    if id in existing_ids {
        let seq = 1
        loop {
            let candidate = id[0..6] + format("{:02x}", seq)
            if candidate not in existing_ids {
                return "@" + candidate
            }
            seq += 1
        }
    }
    return "@" + id
}
```

### 6.5 Determinism Guarantee

Given identical source semantics, the IR output MUST be byte-for-byte identical:

```bash
# This MUST always produce identical output
tml ir src/lib.tml > a.ir
tml ir src/lib.tml > b.ir
diff a.ir b.ir  # No differences
```

### 6.6 Semantic Preservation Properties

The following transformations are semantic-preserving and SHOULD be applied:

1. **Whitespace normalization**: All formatting removed
2. **Comment removal**: Comments not preserved in IR (except @ai: directives)
3. **Field reordering**: Alphabetical, preserves construction semantics
4. **Import sorting**: Alphabetical, no semantic change
5. **Constant folding**: Literal-only expressions evaluated

The following are NOT applied (would change semantics):

1. **Dead code elimination**: Preserved for debugging
2. **Expression reordering**: Side effects must be preserved
3. **Function inlining**: Preserved for separate compilation
```

**Priority**: High
**Consensus**: 5/5 models

---

### 5. Borrow Checker Examples

**Source**: All 5 models

**Current Gap**: `06-MEMORY.md` has rules but insufficient complex examples.

**Recommended Addition**: New subsection in `06-MEMORY.md`

```markdown
## 5. Complex Borrowing Scenarios (NEW SECTION)

### 5.1 Closure Capture

```tml
func example() {
    var data = List.new()
    data.push(1)

    // Closure captures &mut data
    let modifier = do() {
        data.push(2)  // Mutable borrow captured
    }

    // ERROR: Cannot borrow data while closure holds &mut
    // print(data.len())

    modifier()  // Closure executes, releases borrow
    print(data.len())  // OK: borrow released
}
```

### 5.2 Struct Field Borrowing

```tml
type Container {
    items: List[I32],
    metadata: String,
}

func process(c: &mut Container) {
    // Can borrow different fields simultaneously
    let items_ref = &mut c.items   // Borrows items field
    let meta_ref = &c.metadata     // Borrows metadata field (OK - different fields)

    items_ref.push(42)
    print(meta_ref)
}
```

### 5.3 Reborrowing

```tml
func reborrow_example() {
    var data = String.from("hello")
    let r1 = &mut data

    // Reborrow: temporarily create new reference from existing
    helper(r1)     // r1 is reborrowed as &mut inside helper
    r1.push("!")   // OK: reborrow ended, r1 still valid
}

func helper(s: &mut String) {
    s.push(" world")
}
```

### 5.4 Return Reference Lifetime

```tml
// ERROR: Ambiguous lifetime - which input does output reference?
func longest(a: &String, b: &String) -> &String {
    if a.len() > b.len() then a else b
}

// SOLUTION 1: Return owned value
func longest_owned(a: &String, b: &String) -> String {
    if a.len() > b.len() then a.clone() else b.clone()
}

// SOLUTION 2: Take single reference (if applicable)
func longest_single(items: &List[String]) -> &String {
    // Lifetime tied to items - unambiguous
    items.iter().max_by(do(a, b) a.len().cmp(b.len())).unwrap()
}
```

### 5.5 NLL (Non-Lexical Lifetimes)

```tml
func nll_example() {
    var data = vec![1, 2, 3]

    let r = &data[0]
    print(r)          // Last use of r

    // In lexical lifetimes: ERROR (r still in scope)
    // In NLL (TML): OK (r not used after this point)
    data.push(4)
}
```
```

**Priority**: High
**Consensus**: 5/5 models

---

### 6. Specification Test Matrix

**Source**: GPT-5.2-Codex-Max, Grok

**Recommendation**: Add appendix to specification

```markdown
# Appendix A: Specification Conformance Test Matrix (NEW DOCUMENT)

## Purpose

This matrix maps every specification rule to its corresponding test cases, enabling:
1. Compiler validation against specification
2. LLM comprehension verification
3. Regression testing on spec changes

## Matrix Format

| Spec Section | Rule ID | Description | Test File | Status |
|--------------|---------|-------------|-----------|--------|
| 02-LEXICAL.md:4.1 | LEX-INT-001 | Integer literal parsing | tests/lexer/integers.tml | Required |
| 02-LEXICAL.md:4.1 | LEX-INT-002 | Underscore separators | tests/lexer/integers.tml | Required |
| 03-GRAMMAR.md:5.1 | PARSE-EXPR-001 | Precedence hierarchy | tests/parser/precedence.tml | Required |
| 05-SEMANTICS.md:1.3 | SEM-CAP-001 | Capability inheritance | tests/semantics/caps.tml | Required |
| 06-MEMORY.md:4.3 | MEM-BORROW-001 | Multiple immutable borrows | tests/borrow/immutable.tml | Required |
| ... | ... | ... | ... | ... |

## Coverage Requirements

- **Core Language**: 100% coverage required
- **Standard Library**: 95% coverage required
- **Edge Cases**: Best effort

## Validation Protocol

```bash
# Run full conformance suite
tml test --conformance

# Run specific spec section
tml test --conformance --section "05-SEMANTICS"

# Generate coverage report
tml test --conformance --coverage-report
```
```

**Priority**: Medium
**Consensus**: 2/5 models (explicit)

---

### 7. Compressed Specification Format

**Source**: Claude Haiku 4.5

**Recommendation**: Add machine-readable spec summary

```markdown
# Appendix B: Machine-Readable Specification Summary (NEW DOCUMENT)

## Purpose

Compressed specification for context-limited LLMs and tooling.

## Format: `tml-spec.yaml`

```yaml
version: "1.0"
keywords:
  declarations: [module, import, public, private, func, type, trait, extend, let, var, const]
  control: [if, then, else, when, loop, in, while, break, continue, return, catch]
  logical: [and, or, not]
  values: [true, false, this, This]

operators:
  arithmetic: ["+", "-", "*", "/", "%", "**"]
  comparison: ["==", "!=", "<", ">", "<=", ">="]
  bitwise: ["&", "|", "^", "~", "<<", ">>"]
  assignment: ["=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="]
  other: ["->", "..", "..=", "!", "&", "?"]

types:
  primitives: [Bool, I8, I16, I32, I64, I128, U8, U16, U32, U64, U128, F32, F64, String, Char]
  syntax:
    generic: "Type[T]"
    reference: "&Type | &mut Type"
    function: "func(T, U) -> R"
    array: "[Type; N]"
    tuple: "(T, U)"
    optional: "Type?"

grammar_patterns:
  function: "func NAME[GENERICS](PARAMS) -> TYPE { BODY }"
  struct: "type NAME { FIELD: TYPE, ... }"
  enum: "type NAME = VARIANT | VARIANT(TYPE) | ..."
  if_expr: "if COND then EXPR else EXPR"
  when_expr: "when EXPR { PATTERN -> EXPR, ... }"
  loop: "loop [in ITER | while COND] { BODY }"
  error_prop: "EXPR! [else FALLBACK]"

effects:
  builtin_hierarchy:
    io: [io.file, io.file.read, io.file.write, io.network, io.network.tcp, io.network.http, io.process, io.time]
    system: [system.ffi, system.unsafe, system.alloc]
    crypto: [crypto.random, crypto.hash, crypto.encrypt]
  syntax:
    module_caps: "caps: [EFFECT, ...]"
    function_effects: "effects: [EFFECT, ...]"

ownership:
  rules:
    - "Single owner per value"
    - "Move on binding (unless Copy)"
    - "Multiple & borrows allowed"
    - "Single &mut borrow, exclusive"
  copy_types: [Bool, Char, I8-I128, U8-U128, F32, F64, "tuples of Copy", "arrays of Copy"]

contracts:
  syntax:
    precondition: "pre: CONDITION"
    postcondition: "post(RESULT): CONDITION"
```

This compressed format (~2KB) enables context-limited models to maintain core language knowledge.
```

**Priority**: Low (tooling enhancement)
**Consensus**: 1/5 models (explicit)

---

## Implementation Priority Matrix

| Recommendation | Priority | Effort | Impact | Models |
|----------------|----------|--------|--------|--------|
| Effects Extensibility Clarification | Critical | Low | High | 5/5 |
| JSON Error Diagnostics | High | Medium | High | 2/5 |
| Canonical IR Rules Expansion | High | Medium | High | 5/5 |
| Borrow Checker Examples | High | Low | High | 5/5 |
| AI Context Comments | Medium | Low | Medium | 1/5 |
| Spec Test Matrix | Medium | High | Medium | 2/5 |
| Compressed Spec Format | Low | Low | Low | 1/5 |

---

## Immediate Action Items

### Must Do (Before v1.0)

1. **Clarify Effects Extensibility** in `05-SEMANTICS.md`
   - Add section 1.5 on custom capabilities
   - Define extensibility rules

2. **Add JSON Error Format** to `12-ERRORS.md`
   - Define diagnostic JSON schema
   - Include stable ID references

3. **Expand Canonical IR Rules** in `08-IR.md`
   - Complete ordering algorithm
   - Stable ID generation pseudocode
   - Determinism guarantees

4. **Add Borrow Checker Examples** to `06-MEMORY.md`
   - Closure capture scenarios
   - Struct field borrowing
   - Lifetime ambiguity resolution

### Should Do (v1.1)

5. **Add AI Context Comments** to `02-LEXICAL.md`
   - Define `@ai:` directive syntax
   - Specify standard directives

6. **Create Spec Test Matrix** as appendix
   - Map all rules to test cases
   - Define coverage requirements

### Consider (Future)

7. **Compressed Spec Format** for tooling
   - YAML/JSON machine-readable summary
   - Enables context-limited LLM support

---

## Conclusion

The TML specification is fundamentally sound and well-designed for LLM consumption. All 5 models praised the core design decisions (LL(1) grammar, stable IDs, explicit syntax). However, the specification would benefit from:

1. **Greater precision** in the effects system (extensibility rules)
2. **Machine-readable error format** for tooling integration
3. **More comprehensive examples** especially for borrowing
4. **Complete canonicalization algorithm** for IR generation

Implementing these recommendations will significantly improve LLM comprehension and code generation accuracy across all model families.

---

**Document Version**: 1.0
**Meta-Analyst**: Claude Opus 4.5
**Analysis Date**: December 21, 2025
**Input Reports**: 5 LLM analysis reports
**Status**: Recommendations Ready for Review
