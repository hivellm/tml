# Tasks: Function Contracts (Pre/Post Conditions)

**Status**: Phase 1 Complete (parser), Phases 2-4 pending
**Priority**: MEDIUM

## Phase 1: Grammar and Parser — DONE

- [x] 1.1.1 Add `pre:` and `post:` grammar rules — `ContractClause` struct in `ast_decls.hpp`
- [x] 1.1.2 Implement parser support — contextual keywords in `parser_decl.cpp` (50 lines)
- [x] 1.1.3 Store contracts in AST FuncDecl nodes — `std::vector<ContractClause> contracts`
- [x] 1.1.4 Parser verified: `func divide(a, b) pre: b != 0 { ... }` compiles and runs

## Phase 2: Type Checker Integration — Pending

- [ ] 2.1.1 Type-check contract expressions in function scope
- [ ] 2.1.2 Validate post-condition result binding variable
- [ ] 2.1.3 Ensure contract expressions return Bool
- [ ] 2.1.4 Add type checker tests

## Phase 3: Codegen — Pending

- [ ] 3.1.1 Generate runtime assertion code for `pre:` conditions at function entry
- [ ] 3.1.2 Generate runtime assertion code for `post:` conditions before return
- [ ] 3.1.3 Add `--contracts=off` flag to disable runtime checks
- [ ] 3.1.4 Runtime: `tml_panic_contract` added to `essential.c` (65 lines)
- [ ] 3.1.5 Add codegen tests

## Phase 4: Tests and Documentation — Pending

- [ ] 4.1.1-4.1.4 Integration tests, spec updates, examples

## Notes

Currently contracts parse but are silently ignored at type-check and codegen. The pre/post expressions are stored in the AST but not evaluated at runtime. This is safe (no wrong behavior) but the feature is incomplete until codegen emits the assertions.
