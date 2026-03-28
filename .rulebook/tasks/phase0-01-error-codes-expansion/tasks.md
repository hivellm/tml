# Tasks: Error Codes Expansion — 197 → 460 Codes

**Status**: Proposed
**Priority**: HIGH
**Phase**: 0 — Infrastructure (blocks all other phases — better errors = faster debugging)

## Phase A: Tag Existing Untagged Errors (mechanical — no new logic)

> 71 errors. Just add the code string parameter to existing `error()` / `report_error()` calls.

### A.1 Type Checker — 63 untagged errors → T091-T160

- [ ] A.1.1 T091-T100: Access & visibility — private member, unknown field/method/variant/struct/behavior, missing field, duplicate variable (10 codes in `checker/expr.cpp`, `expr_call_method.cpp`, `stmt.cpp`)
- [ ] A.1.2 T111-T117: Pattern matching — destructure mismatch, tuple arity, enum pattern, when arm types (7 codes in `checker/control.cpp`, `stmt.cpp`)
- [ ] A.1.3 T121-T132: Type mismatch specializations — const decl, let-else, return, pointer write, args count, for loop, unsigned negative, integer overflow, try operator (12 codes in `checker/stmt.cpp`, `expr.cpp`, `core.cpp`)
- [ ] A.1.4 T136-T140: OOP/class — circular inheritance, override non-virtual, override signature, abstract methods (5 codes in `checker/core_oop.cpp`)
- [ ] A.1.5 T146-T154: Decorators — @Controller, @flags, @value, @pool, @link, @extern ABI (9 codes in `checker/core.cpp`, `decl_struct.cpp`)
- [ ] A.1.6 T156-T160: Misc — union literal, struct update base, pointer offset, function error (5 codes in `checker/expr.cpp`, `stmt.cpp`)
- [ ] A.1.7 Verify: `tml check` on all test files still produces same errors (just with codes now)

### A.2 Legacy Codegen — 8 untagged errors → C036-C043

- [ ] A.2.1 Add codes to 8 `report_error()` calls without code parameter in `compiler/src/codegen/llvm/`
- [ ] A.2.2 Verify: existing codegen tests still pass

## Phase B: User-Facing Error Improvements (HIGH value)

> 63 new codes. Wrap raw error strings with structured error codes.

### B.1 Module Loading — D001-D015

- [ ] B.1.1 D001 `Module not found` — wrap "Module 'X' not found" in `env_module_load.cpp`
- [ ] B.1.2 D002 `Circular module dependency` — detect and tag
- [ ] B.1.3 D003-D005 `Binary/meta format errors` — wrap corrupt file messages
- [ ] B.1.4 D006-D008 `Duplicate/private/import resolution` — tag resolution failures
- [ ] B.1.5 D009-D015 `ABI, preload, collision, glob conflicts` — remaining module errors
- [ ] B.1.6 Add D001-D015 entries to `compiler/src/cli/explain/`

### B.2 Linker — N001-N010

- [ ] B.2.1 Wrap LLD error output in structured N-codes in `lld_linker.cpp`
- [ ] B.2.2 N001 undefined symbol, N002 duplicate symbol, N003 library not found
- [ ] B.2.3 N004-N010 format error, permission, architecture, entry point, relocation
- [ ] B.2.4 Add N001-N010 entries to `compiler/src/cli/explain/`

### B.3 LLVM Backend — K001-K015

- [ ] B.3.1 Wrap LLVM error strings in structured K-codes in `llvm_backend.cpp`
- [ ] B.3.2 K001 IR parse failed, K002 verification failed, K003 target creation
- [ ] B.3.3 K004-K008 object emission, optimization, type mismatch, GEP, undefined
- [ ] B.3.4 K009-K015 bitcast, sret, calling convention, data layout, debug info, inline asm
- [ ] B.3.5 Add K001-K015 entries to `compiler/src/cli/explain/`

### B.4 Lexer Expansion — L021-L030

- [ ] B.4.1 L021-L025 unterminated string/char/comment, invalid escape, invalid unicode escape
- [ ] B.4.2 L026-L030 numeric overflow, invalid suffix, invalid binary/octal/hex literal
- [ ] B.4.3 Add L021-L030 entries to `compiler/src/cli/explain/lexer_errors.cpp`

### B.5 Parser Expansion — P066-P080

- [ ] B.5.1 P066-P070 expected type/expression/close-brace/paren/bracket
- [ ] B.5.2 P071-P075 unexpected token, max nesting, invalid decorator, keyword as name, unexpected EOF
- [ ] B.5.3 P076-P080 expected comma, duplicate field, invalid pattern, expected func in behavior
- [ ] B.5.4 Add P066-P080 entries to `compiler/src/cli/explain/`

## Phase C: Developer Experience (MEDIUM value)

> 68 new codes. Semantic analysis, borrow checker, HIR, MIR.

### C.1 Semantic Analysis — S014-S025

- [ ] C.1.1 S014-S017 unused variable, unused import, unreachable code, variable shadowing
- [ ] C.1.2 S018-S021 @deprecated, implicit narrowing, empty arm, redundant pattern
- [ ] C.1.3 S022-S025 non-exhaustive match, division by zero, large stack, infinite loop
- [ ] C.1.4 Add S014-S025 entries to `compiler/src/cli/explain/`
- [ ] C.1.5 Implement detection logic for S014 (unused var) and S016 (unreachable code)

### C.2 Borrow Checker Expansion — B018-B030

- [ ] C.2.1 Ensure all 13 BorrowError types have corresponding B-codes in builder_helpers
- [ ] C.2.2 B028-B030 temporary dropped while borrowed, cannot move from ref, borrow beyond scope
- [ ] C.2.3 Add B018-B030 entries to `compiler/src/cli/explain/borrow_errors.cpp`

### C.3 HIR Lowering — H001-H015

- [ ] C.3.1 Add error reporting infrastructure to `compiler/src/hir/` (currently silent failures)
- [ ] C.3.2 H001-H005 unsupported expr, type resolution, monomorphization failure/depth, type param
- [ ] C.3.3 H006-H010 field/variant index, closure capture, unsupported pattern, for/if-let desugar
- [ ] C.3.4 H011-H015 behavior method resolution, associated type, invalid generic, const eval
- [ ] C.3.5 Add H001-H015 entries to `compiler/src/cli/explain/`

### C.4 MIR Building — M001-M020

- [ ] C.4.1 Add error codes to existing MIR error points (21 untagged sites)
- [ ] C.4.2 M001-M010 unsupported expr/stmt, function build fail, type/var/block mismatch, terminator, phi
- [ ] C.4.3 M011-M020 undefined call, struct/enum access, closure env, pass errors, generic, ABI
- [ ] C.4.4 Add M001-M020 entries to `compiler/src/cli/explain/`

### C.5 Codegen Expansion — C044-C050

- [ ] C.5.1 C044-C050 generic instantiation, sret, ABI, void return, array bounds, overflow, bitcast
- [ ] C.5.2 Add C044-C050 entries to `compiler/src/cli/explain/codegen_errors.cpp`

### C.6 Warnings — W005-W015

- [ ] C.6.1 W005-W008 unused variable, unused import, unreachable code, shadowed variable
- [ ] C.6.2 W009-W012 deprecated, implicit narrowing, possible div-by-zero, unused function
- [ ] C.6.3 W013-W015 empty arm, contract always true/false, large stack
- [ ] C.6.4 Add W005-W015 entries to `compiler/src/cli/explain/`

## Phase D: Internal Diagnostics (LOW value — for compiler devs)

> 59 new codes. Preprocessor, query, format, testing, reflection.

### D.1 Preprocessor — PP001-PP010

- [ ] D.1.1 Add error code infrastructure to `compiler/src/preprocessor/`
- [ ] D.1.2 PP001-PP010 unknown directive, unterminated #if, #else without #if, etc.
- [ ] D.1.3 Add PP001-PP010 entries to `compiler/src/cli/explain/`

### D.2 Query System — Q001-Q010

- [ ] D.2.1 Q001 cycle detection, Q002-Q005 cache/fingerprint/type/source errors
- [ ] D.2.2 Q006-Q010 dependency, boundary, stale, race, timeout
- [ ] D.2.3 Add Q001-Q010 entries to `compiler/src/cli/explain/`

### D.3 Formatter/Linter — F001-F010

- [ ] D.3.1 Add error code infrastructure to `compiler/src/format/`
- [ ] D.3.2 F001-F010 indentation, trailing whitespace, line length, naming, doc comments
- [ ] D.3.3 Add F001-F010 entries to `compiler/src/cli/explain/`

### D.4 Testing — X001-X010

- [ ] D.4.1 X001-X005 compile fail, timeout, crash, assertion, should_panic
- [ ] D.4.2 X006-X010 NDJSON error, runtime archive, cache corrupt, no tests, coverage fail
- [ ] D.4.3 Add X001-X010 entries to `compiler/src/cli/explain/`

### D.5 Reflection — R002-R005

- [ ] D.5.1 R002-R005 impl_name bounds, method_name bounds, interface bounds, no metadata
- [ ] D.5.2 Add R002-R005 entries to `compiler/src/cli/explain/`

## Validation

- [ ] V.1 Run full test suite — zero regressions
- [ ] V.2 `tml explain` works for ALL 460 codes
- [ ] V.3 No error site in any pipeline stage emits a message without a code
- [ ] V.4 Update docs/error-codes-proposal.md with final implementation status
- [ ] V.5 Update CHANGELOG
