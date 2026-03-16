# Tasks: Improve Test Infrastructure

**Status**: ARCHIVED — Completed and pending items redistributed.

- **Phase 1 (diagnostic tests)**: Partially complete. Remaining items (B001-B010, S001-S010, C001-C005 diagnostic tests) are ongoing test-writing work, not blocked.
- **Phase 5 (coverage CI gate)**: Complete via rewrite-test-system.
- **Phases 7-8 (cross-platform CI, benchmarks)**: Merged into `zig-inspired-test-migration` Phases 6-7.
- **Phases 2-4, 6 (edge cases, regression convention, cross-feature tests, fuzzing)**: Standalone test quality work — can be done anytime via write-tests skill.

## Phase 1: Diagnostic / Error-Checking Tests

- [x] 1.1.1 Design `@expect-error` directive format and parsing in test runner
- [x] 1.1.2 Implement error-matching logic in `diagnostic_execution.cpp`
- [x] 1.1.3 Add MCP test tool support for diagnostic test mode
- [x] 1.2.1 Write diagnostic tests for type checker errors (T001-T020) - 11 error codes covered
- [ ] 1.2.2 Write diagnostic tests for borrow checker errors (B001-B010)
- [x] 1.2.3 Write diagnostic tests for parser/syntax errors (P001-P010) - 2 error codes covered
- [ ] 1.2.4 Write diagnostic tests for semantic errors (S001-S010)
- [ ] 1.2.5 Write diagnostic tests for codegen errors (C001-C005)
- [x] 1.3.1 Verify error message content matches expected patterns
- [ ] 1.3.2 Verify error line/column positions are accurate

## Phase 5: Coverage as CI Gate — COMPLETE

- [x] 5.1.1-5.1.6 All done via rewrite-test-system

## Phases 2-4, 6: Test Quality (not archived — do anytime)

Remaining test-writing items are standalone and can be done incrementally:
- Phase 2: Edge case / boundary tests (8 items)
- Phase 3: Regression test convention (5 items)
- Phase 4: Cross-feature interaction tests (8 items)
- Phase 6: Expanded fuzzing (6 items)

## Phases 7-8: Merged into zig-inspired-test-migration

- Phase 7 → `zig-inspired-test-migration` Phase 6 (Cross-Platform CI)
- Phase 8 → `zig-inspired-test-migration` Phase 7 (Performance Benchmarks)
