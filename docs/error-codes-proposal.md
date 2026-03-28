# Error Codes Expansion Proposal

> Generated: 2026-03-28 | Updated: full pipeline audit

## Current State — REAL Numbers

### Codes per pipeline stage

| Pipeline Stage | Prefix | Codes with IDs | Untagged errors | Total error sites |
|----------------|--------|---------------|-----------------|-------------------|
| Preprocessor | — | **0** | ~18 | 18 |
| Lexer | L | 17 | 0 | 17 |
| Parser | P | 27 | 0 | 27 |
| Type Checker | T | 79 | **63** | 142 |
| Borrow Checker | B | 18 (in builder_helpers) | **0 in checker itself** | 18 |
| HIR Lowering | — | **0** | ~2 | 2 |
| THIR Lowering | — | **0** | ~1 | 1 |
| MIR Building | — | 2 | ~21 | 23 |
| Legacy Codegen | C | 35 | **8** | 43 |
| LLVM Backend | — | **0** | ~17 (raw LLVM strings) | 17 |
| Linker (LLD) | — | **0** | ~10 (raw LLD strings) | 10 |
| Query System | — | **0** | ~39 | 39 |
| Module Loading | — | 2 | ~63 | 65 |
| Semantic | S | 7 | 0 | 7 |
| Reflection | R | 1 | 0 | 1 |
| Format/Lint | — | **0** | ~28 | 28 |
| Testing | — | **0** | ~10 | 10 |
| General | E | 7 | 0 | 7 |
| Warnings | W | 4 | 0 | 4 |
| **Total** | | **197** | **~270** | **~460** |

**Only 43% of error sites have codes. 57% are untagged.**

### Error codes summary

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

---

## Part 2: Pipeline Stages With ZERO or FEW Error Codes

### Codes per pipeline stage (current state)

| Pipeline Stage | Prefix | Codes | Untagged Errors | Status |
|----------------|--------|-------|-----------------|--------|
| Preprocessor | — | **0** | ~18 | **ZERO codes** |
| Lexer | L | 17 | 0 | OK |
| Parser | P | 27 | 0 | OK |
| Type Checker | T | 79 | **63** | Needs tagging |
| Borrow Checker | B | 18 | **~15 BorrowError types** | Needs expansion |
| HIR Lowering | — | **0** | ~33 | **ZERO codes** |
| THIR Lowering | — | **0** | ~1 | **ZERO codes** |
| MIR Building | — | **2** | ~29 | Nearly zero |
| MIR Codegen | C | 35 | ~5 | OK |
| LLVM Backend | — | **0** | ~72 | **ZERO codes** |
| Query System | — | **0** | ~41 | **ZERO codes** |
| Module Loading | — | **2** | ~122 | Nearly zero |
| Linker (LLD) | — | **0** | ~20 | **ZERO codes** |
| Semantic | S | 7 | 0 | Low coverage |
| Format/Lint | — | **0** | ? | **ZERO codes** |
| Testing | — | **0** | ? | **ZERO codes** |

**7 pipeline stages have ZERO error codes. 4 more have < 3 codes.**

---

### PP001-PP010: Preprocessor Errors (NEW prefix)

| Code | Description |
|------|-------------|
| PP001 | Unknown preprocessor directive |
| PP002 | Unterminated #if/#ifdef block |
| PP003 | #else without matching #if |
| PP004 | #endif without matching #if |
| PP005 | #elif after #else |
| PP006 | Undefined preprocessor symbol in #ifdef |
| PP007 | Nested #if depth exceeded |
| PP008 | Invalid preprocessor expression |
| PP009 | Circular #include detected |
| PP010 | File not found in #include |

### H001-H015: HIR Lowering Errors (NEW prefix)

| Code | Description |
|------|-------------|
| H001 | Unsupported expression type in HIR lowering |
| H002 | Failed to resolve type during HIR lowering |
| H003 | Monomorphization failed for generic function '{name}' |
| H004 | Monomorphization depth exceeded (recursive generic) |
| H005 | Type parameter '{name}' could not be resolved to concrete type |
| H006 | Field index resolution failed for struct '{name}' |
| H007 | Variant index resolution failed for enum '{name}' |
| H008 | Closure capture analysis failed |
| H009 | Unsupported pattern in HIR lowering |
| H010 | Desugaring failed: for loop over non-iterable type |
| H011 | Desugaring failed: if-let to when conversion |
| H012 | Behavior method resolution failed in HIR |
| H013 | Associated type could not be resolved |
| H014 | Generic instantiation produced invalid type |
| H015 | Const evaluation failed during HIR lowering |

### M001-M020: MIR Building Errors (expand from 2 to 20)

| Code | Description |
|------|-------------|
| M001 | Unsupported expression in MIR builder |
| M002 | Unsupported statement in MIR builder |
| M003 | Failed to build MIR for function '{name}' |
| M004 | MIR type mismatch: expected '{expected}', found '{found}' |
| M005 | MIR variable not found: '{name}' |
| M006 | MIR basic block not found: '{label}' |
| M007 | MIR terminator missing in block '{label}' |
| M008 | Invalid MIR instruction operand |
| M009 | MIR phi node type mismatch |
| M010 | Unsupported control flow in MIR |
| M011 | MIR call to undefined function '{name}' |
| M012 | MIR struct field access on non-struct type |
| M013 | MIR enum variant access on non-enum type |
| M014 | MIR closure environment capture failed |
| M015 | MIR pass error: mem2reg failed |
| M016 | MIR pass error: dead code elimination removed live code |
| M017 | MIR pass error: block merge produced invalid graph |
| M018 | MIR generic instantiation failed |
| M019 | MIR sret convention mismatch |
| M020 | MIR ABI mismatch for extern function |

### K001-K015: LLVM Backend Errors (NEW prefix — "kompile")

| Code | Description |
|------|-------------|
| K001 | LLVM IR parsing failed |
| K002 | LLVM module verification failed |
| K003 | LLVM target machine creation failed |
| K004 | LLVM object file emission failed |
| K005 | LLVM optimization pass failed |
| K006 | LLVM type mismatch in generated IR |
| K007 | LLVM function signature mismatch |
| K008 | LLVM invalid GEP indices |
| K009 | LLVM undefined value reference |
| K010 | LLVM invalid bitcast |
| K011 | LLVM sret/byval convention error |
| K012 | LLVM calling convention mismatch |
| K013 | LLVM target data layout error |
| K014 | LLVM debug info generation failed |
| K015 | LLVM inline assembly error |

### Q001-Q010: Query System Errors (NEW prefix)

| Code | Description |
|------|-------------|
| Q001 | Query cycle detected: '{query}' depends on itself |
| Q002 | Query cache invalidation error |
| Q003 | Incremental compilation fingerprint mismatch |
| Q004 | Query result type mismatch |
| Q005 | Source file not found for query |
| Q006 | Module dependency resolution failed |
| Q007 | Compilation unit boundary violation |
| Q008 | Stale query result detected |
| Q009 | Parallel query execution race condition |
| Q010 | Query timeout exceeded |

### D001-D015: Module/Dependency Loading Errors (NEW prefix)

| Code | Description |
|------|-------------|
| D001 | Module not found: '{path}' |
| D002 | Circular module dependency: '{a}' ↔ '{b}' |
| D003 | Module binary format version mismatch |
| D004 | Module metadata (.tml.meta) corrupt or invalid |
| D005 | Module binary (.tml.bin) corrupt or invalid |
| D006 | Duplicate module definition: '{name}' |
| D007 | Module import resolution failed: '{path}' |
| D008 | Re-export of private symbol '{name}' |
| D009 | Module ABI incompatibility (compiled with different compiler version) |
| D010 | Module preload failed: cache directory inaccessible |
| D011 | Symbol collision: '{name}' defined in multiple modules |
| D012 | Module load timeout |
| D013 | Missing module dependency: '{name}' requires '{dep}' |
| D014 | Module path normalization failed |
| D015 | Glob import (`*`) conflict: ambiguous symbol '{name}' |

### N001-N010: Linker Errors (NEW prefix)

| Code | Description |
|------|-------------|
| N001 | Linker: undefined symbol '{name}' |
| N002 | Linker: duplicate symbol '{name}' |
| N003 | Linker: library not found: '{path}' |
| N004 | Linker: object file format error |
| N005 | Linker: permission denied writing output |
| N006 | Linker: output file too large |
| N007 | Linker: incompatible object file (wrong architecture) |
| N008 | Linker: missing entry point |
| N009 | Linker: relocation overflow |
| N010 | Linker: C runtime library not found |

### B018-B030: Borrow Checker Expansion

| Code | Description |
|------|-------------|
| B018 | Use of moved value: '{name}' (already exists as BorrowError, needs code) |
| B019 | Double mutable borrow of '{name}' |
| B020 | Mutable borrow while immutably borrowed: '{name}' |
| B021 | Immutable borrow while mutably borrowed: '{name}' |
| B022 | Return of reference to local variable '{name}' |
| B023 | Closure captures moved value '{name}' |
| B024 | Closure capture conflict: '{name}' captured both mutably and immutably |
| B025 | Partially moved value: field '{field}' of '{name}' already moved |
| B026 | Reborrow outlives origin: '{name}' |
| B027 | Ambiguous return lifetime in function '{name}' |
| B028 | Temporary value dropped while still borrowed |
| B029 | Cannot move out of borrowed reference |
| B030 | Borrow extends beyond scope of owner |

### F001-F010: Formatter/Linter Errors (NEW prefix)

| Code | Description |
|------|-------------|
| F001 | Indentation mismatch: expected {n} spaces, found {m} |
| F002 | Trailing whitespace |
| F003 | Missing newline at end of file |
| F004 | Line exceeds maximum length ({n} > {max}) |
| F005 | Mixed tabs and spaces |
| F006 | Unused import detected (lint) |
| F007 | Naming convention violation: '{name}' should be snake_case |
| F008 | Naming convention violation: '{name}' should be PascalCase |
| F009 | Doc comment missing for public function '{name}' |
| F010 | Redundant parentheses |

### X001-X010: Testing System Errors (NEW prefix)

| Code | Description |
|------|-------------|
| X001 | Test compilation failed |
| X002 | Test execution timeout ({n}s exceeded) |
| X003 | Test process crashed (exit code {code}) |
| X004 | Test assertion failed: {message} |
| X005 | @should_panic test did not panic |
| X006 | Test suite NDJSON protocol error |
| X007 | Test runtime archive (.lib) not found |
| X008 | Test cache corruption detected |
| X009 | Test discovery found no test files |
| X010 | Coverage data collection failed |

---

### C036-C050: Legacy Codegen — Untagged Errors

8 codegen errors emit `report_error()` without a code. Plus new codes for common failure modes:

| Code | Description |
|------|-------------|
| C036 | @no_mangle cannot be used with generic functions |
| C037 | unwrap_or_else requires a closure or function reference |
| C038 | Unsupported expression in codegen |
| C039 | Unsupported statement in codegen |
| C040 | Failed to resolve function for call |
| C041 | Struct field access on non-struct type in codegen |
| C042 | Enum variant access on non-enum type in codegen |
| C043 | Closure capture codegen failed |
| C044 | Generic instantiation produced invalid LLVM type |
| C045 | sret convention mismatch in function call |
| C046 | ABI mismatch for extern function call |
| C047 | Void function returning non-void value |
| C048 | Array bounds exceeded in codegen |
| C049 | Integer overflow in constant evaluation |
| C050 | Bitcast between incompatible types |

### S010-S025: Semantic Analysis Expansion (currently only 7 codes)

| Code | Description |
|------|-------------|
| S014 | Unused variable '{name}' |
| S015 | Unused import '{module}' |
| S016 | Unreachable code after return/break/panic |
| S017 | Variable '{name}' shadows outer scope binding |
| S018 | @deprecated: function '{name}' is deprecated |
| S019 | Implicit integer narrowing: I64 → I32 without cast |
| S020 | Empty when/match arm with no body |
| S021 | Redundant pattern: arm already covered by previous arm |
| S022 | Missing when/match arms (non-exhaustive) |
| S023 | Division by zero in constant expression |
| S024 | Large stack allocation (> 1MB local variable) |
| S025 | Infinite loop detected (loop without break/return) |

### L021-L030: Lexer Expansion

| Code | Description |
|------|-------------|
| L021 | Unterminated string literal |
| L022 | Unterminated character literal |
| L023 | Unterminated block comment |
| L024 | Invalid escape sequence '\\{c}' |
| L025 | Invalid unicode escape '\\u{...}' |
| L026 | Numeric literal overflow |
| L027 | Invalid numeric literal suffix |
| L028 | Invalid binary literal (expected 0 or 1) |
| L029 | Invalid octal literal (expected 0-7) |
| L030 | Invalid hex literal (expected 0-9, a-f) |

### P066-P080: Parser Expansion

| Code | Description |
|------|-------------|
| P066 | Expected type annotation after ':' |
| P067 | Expected expression, found '{token}' |
| P068 | Expected '}' to close block started at line {n} |
| P069 | Expected ')' to close parentheses started at line {n} |
| P070 | Expected ']' to close bracket started at line {n} |
| P071 | Unexpected token after end of expression |
| P072 | Maximum nesting depth exceeded |
| P073 | Invalid decorator syntax |
| P074 | Expected function name, found keyword '{kw}' |
| P075 | Unexpected end of file |
| P076 | Expected ',' or '}' in struct literal |
| P077 | Expected ',' or ')' in function arguments |
| P078 | Duplicate field '{name}' in struct literal |
| P079 | Invalid pattern in let binding |
| P080 | Expected 'func' keyword in behavior declaration |

## Complete Summary

| Category | Prefix | Current | Proposed New | Total After |
|----------|--------|---------|-------------|-------------|
| Preprocessor | PP | **0** | +10 | 10 |
| Lexer | L | 17 | +10 (L021-L030) | 27 |
| Parser | P | 27 | +15 (P066-P080) | 42 |
| Type Checker | T | 79+63 untagged | +70 (T091-T160) | 149 |
| Borrow Checker | B | 18 | +13 (B018-B030) | 31 |
| HIR Lowering | H | **0** | +15 (H001-H015) | 15 |
| MIR Building | M | 2 | +18 (M001-M020) | 20 |
| Codegen | C | 35+8 untagged | +15 (C036-C050) | 50 |
| Semantic | S | 7 | +12 (S014-S025) | 19 |
| LLVM Backend | K | **0** | +15 (K001-K015) | 15 |
| Query System | Q | **0** | +10 (Q001-Q010) | 10 |
| Module Loading | D | 2 | +13 (D001-D015) | 15 |
| Linker | N | **0** | +10 (N001-N010) | 10 |
| Reflection | R | 1 | +4 (R002-R005) | 5 |
| Formatter/Lint | F | **0** | +10 (F001-F010) | 10 |
| Testing | X | **0** | +10 (X001-X010) | 10 |
| Warnings | W | 4 | +11 (W005-W015) | 15 |
| General | E | 7 | 0 | 7 |
| **Total** | | **197 tagged + ~270 untagged** | **+261 new codes** | **460 (zero untagged)** |

## Implementation Priority

### Phase A: Tag existing untagged errors (pure mechanical — no new logic)
1. **T091-T160**: 63 type checker errors → add code parameter to `error()` calls
2. **C036-C050**: 8 codegen errors → add code to `report_error()` calls

### Phase B: User-facing error improvements (HIGH value)
3. **D001-D015**: Module loading — currently shows raw "module not found" without guidance
4. **N001-N010**: Linker — currently shows raw LLD output, not actionable
5. **K001-K015**: LLVM backend — currently shows raw LLVM parse errors, not actionable
6. **L021-L030**: Lexer — unterminated strings/comments lack specific codes
7. **P066-P080**: Parser — "Expected X, found Y" needs more specific codes

### Phase C: Developer experience (MEDIUM value)
8. **S014-S025**: Semantic warnings — unused vars, unreachable code, shadowing
9. **W005-W015**: Warnings — overlap with S-codes, for non-semantic warnings
10. **B018-B030**: Borrow checker — richer diagnostics for ownership errors
11. **H001-H015**: HIR — debug generic instantiation failures
12. **M001-M020**: MIR — debug codegen pipeline

### Phase D: Internal diagnostics (LOW value — for compiler devs)
13. **PP001-PP010**: Preprocessor — rarely hits users
14. **Q001-Q010**: Query system — cycle detection, cache errors
15. **F001-F010**: Formatter/Lint — optional tooling
16. **X001-X010**: Testing — test runner diagnostics
17. **R002-R005**: Reflection — already works, better messages
