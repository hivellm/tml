# Tasks: Bootstrap Borrow Checker

## Progress: 80% (24/30 tasks complete)

## 1. Setup Phase
- [x] 1.1 Create `src/borrow/` directory structure
- [x] 1.2 Set up CMake configuration for borrow module
- [x] 1.3 Create base header files with include guards

## 2. Control Flow Graph Phase
- [x] 2.1 Implement CFG node types (BasicBlock, Edge)
- [x] 2.2 Implement CFG builder from TAST
- [x] 2.3 Handle if/else branching in CFG
- [x] 2.4 Handle loop constructs in CFG
- [x] 2.5 Handle when/match branching in CFG
- [x] 2.6 Handle early returns and breaks

## 3. Ownership Tracking Phase
- [x] 3.1 Implement ownership state (Owned, Moved, Borrowed)
- [x] 3.2 Implement move detection
- [x] 3.3 Implement copy type detection
- [x] 3.4 Implement partial move tracking (for structs)
- [x] 3.5 Implement use-after-move detection

## 4. Borrow Tracking Phase
- [x] 4.1 Implement borrow creation tracking
- [x] 4.2 Implement borrow scope tracking
- [x] 4.3 Implement shared borrow rules
- [x] 4.4 Implement mutable borrow rules
- [ ] 4.5 Implement reborrow handling
- [ ] 4.6 Implement two-phase borrows

## 5. Lifetime Analysis Phase
- [x] 5.1 Implement lifetime region representation
- [x] 5.2 Implement lifetime constraint generation
- [x] 5.3 Implement lifetime outlives checking
- [x] 5.4 Implement non-lexical lifetimes (NLL)
- [ ] 5.5 Implement lifetime elision rules
- [ ] 5.6 Handle function lifetime parameters

## 6. Path Analysis Phase
- [x] 6.1 Implement place/path representation
- [x] 6.2 Implement path overlap detection
- [x] 6.3 Implement field access paths
- [x] 6.4 Implement index access paths

## 7. Error Detection Phase
- [x] 7.1 Detect simultaneous mutable borrows
- [x] 7.2 Detect mutable + shared borrow conflicts
- [x] 7.3 Detect use-after-move errors
- [x] 7.4 Detect dangling reference errors
- [x] 7.5 Detect assignment to borrowed value

## 8. Testing Phase
- [x] 8.1 Write unit tests for CFG construction
- [x] 8.2 Write unit tests for ownership tracking
- [x] 8.3 Write unit tests for borrow rules
- [x] 8.4 Write unit tests for lifetime analysis
- [x] 8.5 Write integration tests with error cases
- [ ] 8.6 Verify test coverage ≥95%

## 9. Documentation Phase
- [x] 9.1 Document public API in header files
- [x] 9.2 Update CHANGELOG.md with borrow checker implementation

## Implementation Notes

**Completed**: Borrow checker fully modularized into 5 modules:
- `checker_core.cpp` - Main borrow checker logic and CFG
- `checker_env.cpp` - Borrow environment and state tracking
- `checker_expr.cpp` - Expression borrow checking
- `checker_ops.cpp` - Operation-specific borrow rules
- `checker_stmt.cpp` - Statement borrow checking

**Features**:
- ✅ Control flow graph (CFG) construction
- ✅ Ownership tracking (owned, moved, borrowed)
- ✅ Borrow rules (shared and mutable)
- ✅ Non-lexical lifetimes (NLL)
- ✅ Path analysis for field/index access
- ✅ Use-after-move detection
- ✅ Borrow conflict detection

**Known Issues**:
- ⚠️ 9 failing tests related to complex structure handling
- ⚠️ Missing reborrow handling (4.5)
- ⚠️ Missing two-phase borrow support (4.6)
- ⚠️ Missing lifetime elision rules (5.5)
- ⚠️ Missing function lifetime parameters (5.6)

**Status**: Fully functional with core features, needs advanced borrow patterns.

**Next Steps**:
- [ ] Fix 9 failing complex structure tests
- [ ] Implement reborrow handling
- [ ] Implement two-phase borrows
- [ ] Implement lifetime elision rules
- [ ] Increase test coverage to 95%+
