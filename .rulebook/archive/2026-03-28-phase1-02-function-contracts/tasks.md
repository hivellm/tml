# Tasks: Function Contracts (Pre/Post Conditions)

**Status**: Complete — Phases 1-4 done (pre-conditions enforced at runtime, post-conditions type-checked but codegen deferred)
**Priority**: MEDIUM

## Phase 1: Grammar and Parser — DONE

- [x] 1.1.1 Add `pre:` and `post:` grammar rules — `ContractClause` struct in `ast_decls.hpp`
- [x] 1.1.2 Implement parser support — contextual keywords in `parser_decl.cpp` (50 lines)
- [x] 1.1.3 Store contracts in AST FuncDecl nodes — `std::vector<ContractClause> contracts`
- [x] 1.1.4 Parser verified: `func divide(a, b) pre: b != 0 { ... }` compiles and runs

## Phase 2: Type Checker Integration — DONE

- [x] 2.1.1 Type-check contract expressions in function scope — added to `check_func_body()` in `core.cpp`
- [x] 2.1.2 Validate post-condition result binding variable — binds result name in new scope with return type
- [x] 2.1.3 Ensure contract expressions return Bool — error T090 if not Bool
- [x] 2.1.4 Type checker verified: contracts type-check in function parameter scope

## Phase 3: Codegen — DONE (pre-conditions)

- [x] 3.1.1 Generate runtime assertion code for `pre:` conditions at function entry — `if (!cond) { panic("contract violation: pre-condition failed in 'name'") }`
- [ ] 3.1.2 Post-condition codegen deferred — requires intercepting all return paths (complex, low priority)
- [ ] 3.1.3 `--contracts=off` flag deferred — trivial to add later as a codegen flag
- [x] 3.1.4 Runtime: reuses existing `panic()` — no new C code needed
- [x] 3.1.5 Codegen tests: `compiler/tests/compiler/contracts/pre_condition.test.tml` (4 tests)

## Phase 4: Tests and Documentation — DONE

- [x] 4.1.1 Integration test: `pre_condition.test.tml` — 4 tests (single pre, multiple pre, divide, boundary)
- [x] 4.1.2 Verified pre-condition violation panics with exit code 127
- [x] 4.1.3 Contracts silently ignored in legacy codegen (safe fallback)
- [x] 4.1.4 Post-condition syntax parses and type-checks but is not enforced at runtime yet

## Notes

Currently contracts parse but are silently ignored at type-check and codegen. The pre/post expressions are stored in the AST but not evaluated at runtime. This is safe (no wrong behavior) but the feature is incomplete until codegen emits the assertions.
