# Tasks: Bootstrap Borrow Checker

## Progress: 0% (0/30 tasks complete)

## 1. Setup Phase
- [ ] 1.1 Create `src/borrow/` directory structure
- [ ] 1.2 Set up CMake configuration for borrow module
- [ ] 1.3 Create base header files with include guards

## 2. Control Flow Graph Phase
- [ ] 2.1 Implement CFG node types (BasicBlock, Edge)
- [ ] 2.2 Implement CFG builder from TAST
- [ ] 2.3 Handle if/else branching in CFG
- [ ] 2.4 Handle loop constructs in CFG
- [ ] 2.5 Handle when/match branching in CFG
- [ ] 2.6 Handle early returns and breaks

## 3. Ownership Tracking Phase
- [ ] 3.1 Implement ownership state (Owned, Moved, Borrowed)
- [ ] 3.2 Implement move detection
- [ ] 3.3 Implement copy type detection
- [ ] 3.4 Implement partial move tracking (for structs)
- [ ] 3.5 Implement use-after-move detection

## 4. Borrow Tracking Phase
- [ ] 4.1 Implement borrow creation tracking
- [ ] 4.2 Implement borrow scope tracking
- [ ] 4.3 Implement shared borrow rules
- [ ] 4.4 Implement mutable borrow rules
- [ ] 4.5 Implement reborrow handling
- [ ] 4.6 Implement two-phase borrows

## 5. Lifetime Analysis Phase
- [ ] 5.1 Implement lifetime region representation
- [ ] 5.2 Implement lifetime constraint generation
- [ ] 5.3 Implement lifetime outlives checking
- [ ] 5.4 Implement non-lexical lifetimes (NLL)
- [ ] 5.5 Implement lifetime elision rules
- [ ] 5.6 Handle function lifetime parameters

## 6. Path Analysis Phase
- [ ] 6.1 Implement place/path representation
- [ ] 6.2 Implement path overlap detection
- [ ] 6.3 Implement field access paths
- [ ] 6.4 Implement index access paths

## 7. Error Detection Phase
- [ ] 7.1 Detect simultaneous mutable borrows
- [ ] 7.2 Detect mutable + shared borrow conflicts
- [ ] 7.3 Detect use-after-move errors
- [ ] 7.4 Detect dangling reference errors
- [ ] 7.5 Detect assignment to borrowed value

## 8. Testing Phase
- [ ] 8.1 Write unit tests for CFG construction
- [ ] 8.2 Write unit tests for ownership tracking
- [ ] 8.3 Write unit tests for borrow rules
- [ ] 8.4 Write unit tests for lifetime analysis
- [ ] 8.5 Write integration tests with error cases
- [ ] 8.6 Verify test coverage ≥95%

## 9. Documentation Phase
- [ ] 9.1 Document public API in header files
- [ ] 9.2 Update CHANGELOG.md with borrow checker implementation
