# Error Codes Expansion Proposal

> Generated: 2026-03-28

## Current State

| Category | Prefix | Count | Range | Description |
|----------|--------|-------|-------|-------------|
| Lexer | L | 17 | L001-L020 | Tokenization errors |
| Parser | P | 27 | P001-P065 | Syntax errors |
| Type Checker | T | 79 | T001-T090 | Type errors |
| Borrow Checker | B | 18 | B001-B099 | Ownership/borrow errors |
| Codegen | C | 35 | C001-C035 | LLVM IR generation errors |
| Semantic | S | 7 | S001-S013 | Semantic analysis |
| Reflection | R | 1 | R001 | Reflection intrinsics |
| General | E | 7 | E001-E031 | General compiler errors |
| Warnings | W | 4 | W001-W004 | Non-fatal warnings |
| **Total** | | **197** | | |

**Problem: 63 type checker errors have NO code** — they emit generic messages like "Type mismatch: expected X, found Y" without a trackable code. This makes debugging harder because `tml explain T???` can't help.

---

## Proposed New Codes

### T091-T110: Access & Visibility Errors

| Code | Message Pattern | Current (no code) |
|------|----------------|-------------------|
| T091 | Cannot access private member '{name}' on type '{type}' | `Cannot access private member '` |
| T092 | Cannot access protected member '{name}' — only accessible from subclasses | `Cannot access protected member '` |
| T093 | Unknown field '{name}' on type '{type}' | `Unknown field '`, `Unknown field: ` |
| T094 | Unknown method '{name}' on type '{type}' | `Unknown method '` |
| T095 | Unknown variant '{name}' on enum '{type}' | `Unknown variant '` |
| T096 | Unknown struct type '{name}' | `Unknown struct type '` |
| T097 | Unknown behavior '{name}' | `Unknown behavior '` |
| T098 | Missing field '{name}' in struct literal | `Missing field '` |
| T099 | Duplicate definition of variable '{name}' | `Duplicate definition of variable '` |
| T100 | Field '{name}' — further info | `Field '` (generic) |

### T111-T120: Pattern Matching Errors

| Code | Message Pattern | Current (no code) |
|------|----------------|-------------------|
| T111 | Cannot destructure non-tuple type '{type}' with tuple pattern | `Cannot destructure non-tuple type` |
| T112 | Cannot destructure non-struct type '{type}' with struct pattern | `Cannot destructure non-struct type` |
| T113 | Cannot destructure non-array type '{type}' with array pattern | `Cannot destructure non-array type` |
| T114 | Tuple pattern has wrong number of elements: expected {n}, got {m} | `Tuple pattern has ` |
| T115 | Pattern expects enum type, but got '{type}' | `Pattern expects enum type` |
| T116 | Enum variant '{name}' — variant error | `Enum variant '`, `Variant '` |
| T117 | When arm type mismatch: expected '{expected}', found '{found}' | `When arm type mismatch:` |

### T121-T135: Type Mismatch Specializations

Currently many different situations all emit "Type mismatch: expected X, found Y" — making it impossible to know the context.

| Code | Message Pattern | Current (no code) |
|------|----------------|-------------------|
| T121 | Type mismatch in const declaration: expected '{expected}', found '{found}' | `Type mismatch in const declaration:` |
| T122 | Type mismatch in const initializer: expected '{expected}', found '{found}' | `Type mismatch in const initializer:` |
| T123 | Type mismatch in let-else: expected '{expected}', found '{found}' | `Type mismatch in let-else:` |
| T124 | Type mismatch in pointer write: expected '{expected}', found '{found}' | `Type mismatch in pointer write:` |
| T125 | Return type mismatch: expected '{expected}', found '{found}' | `Return type mismatch:` |
| T126 | Wrong number of arguments: expected {n}, got {m} | `Wrong number of arguments:` |
| T127 | Parameter type mismatch: expected '{expected}', found '{found}' | `Parameter ` |
| T128 | For loop requires iterable type, found '{type}' | `For loop requires slice or collection` |
| T129 | Cannot assign negative value to unsigned type '{type}' | `Cannot assign negative value` |
| T130 | Integer literal overflow for type '{type}' | `Integer literal ` |
| T131 | Try operator (!) requires Outcome[T,E] or Maybe[T], got '{type}' | `try operator (!) can only be used` |
| T132 | Let-else requires explicit type annotation | `TML requires explicit type annotation` |

### T136-T145: OOP / Class Errors

| Code | Message Pattern | Current (no code) |
|------|----------------|-------------------|
| T136 | Circular inheritance detected: class '{name}' | `Circular inheritance` |
| T137 | Cannot override non-virtual method '{name}' | `Cannot override non-virtual method` |
| T138 | Override method '{name}' signature mismatch | `Override method '`, `Cannot override method '` |
| T139 | Non-abstract class '{name}' must implement all abstract methods | `Non-abstract class '` |
| T140 | Class '{name}' — class-specific error | `Class '` (generic) |

### T146-T155: Decorator Errors

| Code | Message Pattern | Current (no code) |
|------|----------------|-------------------|
| T146 | @Controller is only valid on type declarations | `@Controller is only valid` |
| T147 | @flags enum variant must have explicit value | `@flags enum variant '` |
| T148 | @flags underlying type must be U8/U16/U32/U64 | `@flags underlying type must be` |
| T149 | @flags requires underlying type specification | `@flags(` |
| T150 | @pool and @value are mutually exclusive | `@pool and @value are mutually exclusive` |
| T151 | @value class must have specific structure | `@value class '` |
| T152 | @link path not found | `@link path '` |
| T153 | Invalid @extern ABI | `Invalid @extern ABI '` |
| T154 | @decorator — generic decorator error | `@` (generic) |

### T156-T160: Misc Type Errors

| Code | Message Pattern | Current (no code) |
|------|----------------|-------------------|
| T156 | Union literal can only initialize one field | `Union literal can only initialize` |
| T157 | Union literal requires exactly one field | `Union literal requires exactly one` |
| T158 | Struct update base type mismatch | `Struct update base has type '` |
| T159 | Pointer offset() requires I32 or I64 argument | `Pointer offset() requires` |
| T160 | Function '{name}' — function-specific error | `Function '` (generic) |

### R002-R010: Reflection Errors (expand from R001)

| Code | Message Pattern | Description |
|------|----------------|-------------|
| R002 | impl_count index out of bounds | impl_name with invalid index |
| R003 | method_name index out of bounds | method_name with invalid index |
| R004 | interface_method_name index out of bounds | interface intrinsic bounds |
| R005 | Type has no reflection metadata | @derive(Reflect) not applied |

### W005-W015: New Warnings

| Code | Message Pattern | Description |
|------|----------------|-------------|
| W005 | Unused variable '{name}' | Variable declared but never read |
| W006 | Unused import '{module}' | Import not referenced |
| W007 | Unreachable code after return | Dead code after return/panic |
| W008 | Shadowed variable '{name}' | Variable shadows outer scope |
| W009 | Deprecated function '{name}' | @deprecated decorator |
| W010 | Implicit integer narrowing | I64 assigned to I32 without cast |
| W011 | Division by zero possible | Divisor could be zero |
| W012 | Unused function '{name}' | Private function never called |
| W013 | Empty when arm | When branch with no body |
| W014 | Contract always true/false | Pre-condition is trivially true |
| W015 | Large stack allocation | Local variable > 1MB |

---

## Summary

| Category | Current | Proposed New | Total After |
|----------|---------|-------------|-------------|
| T (Type) | 79 + 63 untagged | +70 (T091-T160) | 149 |
| R (Reflect) | 1 | +4 (R002-R005) | 5 |
| W (Warning) | 4 | +11 (W005-W015) | 15 |
| Others | 113 | 0 | 113 |
| **Total** | **197 + 63 untagged** | **+85** | **282 (zero untagged)** |

## Implementation Priority

1. **HIGH**: Tag the 63 untagged type checker errors (T091-T160) — pure mechanical, no behavior change
2. **MEDIUM**: Add new warnings (W005-W015) — improves developer experience
3. **LOW**: Expand reflection codes (R002-R005) — already works, just better messages
