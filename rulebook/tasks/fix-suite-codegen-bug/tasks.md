## 1. Investigation
- [ ] 1.1 Analyze suite_execution.cpp for function declaration merging logic
- [ ] 1.2 Dump LLVM IR from failing suite compiler_tests_17 to identify wrong signatures
- [ ] 1.3 Check meta cache files for duplicate/conflicting declarations
- [ ] 1.4 Identify which function (repeat[T] vs repeat_char) is causing the conflict

## 2. Root Cause Fix
- [ ] 2.1 Fix symbol resolution in suite merging to prefer concrete signatures
- [ ] 2.2 Deduplicate function declarations when combining multiple test modules
- [ ] 2.3 Verify codegen uses correct function signature for calls
- [ ] 2.4 Ensure meta cache loading picks most specific (non-generic) version

## 3. Validation & Testing
- [ ] 3.1 Run full test suite: `tml test --coverage --no-cache`
- [ ] 3.2 Verify all 1084 test files pass (no IR errors)
- [ ] 3.3 Generate coverage report at `build/coverage/coverage.html`
- [ ] 3.4 Run individual test files to ensure no regressions
- [ ] 3.5 Test suite mode with different thread counts

## 4. Documentation
- [ ] 4.1 Update MEMORY.md with fix details and resolution
- [ ] 4.2 Add comment to suite_execution.cpp explaining the deduplication logic
- [ ] 4.3 Create test case that exercises the fixed code path
