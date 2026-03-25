# Tasks: Function Contracts (Pre/Post Conditions)

**Status**: Planning (0%) - Future feature

## Phase 1: Grammar and Parser

- [ ] 1.1.1 Add `pre:` and `post:` grammar rules to function declarations
- [ ] 1.1.2 Implement parser support for contract clauses
- [ ] 1.1.3 Store contracts in AST FuncDecl nodes
- [ ] 1.1.4 Add parser tests for contract syntax

## Phase 2: Type Checker Integration

- [ ] 2.1.1 Type-check contract expressions in function scope
- [ ] 2.1.2 Validate post-condition result binding variable
- [ ] 2.1.3 Ensure contract expressions return Bool
- [ ] 2.1.4 Add type checker tests

## Phase 3: Codegen

- [ ] 3.1.1 Generate runtime assertion code for `pre:` conditions at function entry
- [ ] 3.1.2 Generate runtime assertion code for `post:` conditions before return
- [ ] 3.1.3 Add `--contracts=off` flag to disable runtime checks
- [ ] 3.1.4 Add codegen tests

## Phase 4: Tests and Documentation

- [ ] 4.1.1 Write integration tests for contracts
- [ ] 4.1.2 Update docs/specs/03-GRAMMAR.md with contract grammar
- [ ] 4.1.3 Update docs/specs/05-SEMANTICS.md with contract semantics
- [ ] 4.1.4 Add contract examples to docs/specs/14-EXAMPLES.md
