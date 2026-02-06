# Tasks: Implement Backtrace Library

**Status**: In Progress (40%)

## Phase 1: FFI Runtime Foundation

- [x] 1.1.1 Create compiler/runtime/backtrace.h with type definitions
- [x] 1.1.2 Implement backtrace_capture() for Windows (RtlCaptureStackBackTrace)
- [x] 1.1.3 Implement backtrace_resolve() for Windows (DbgHelp)
- [x] 1.1.4 Add DbgHelp initialization (SymInitialize)
- [x] 1.1.5 Implement backtrace_capture() for Unix (backtrace + dladdr fallback)
- [x] 1.1.6 Implement backtrace_resolve() for Unix (dladdr)
- [x] 1.1.7 Add CMakeLists.txt entries for backtrace runtime
- [x] 1.1.8 Link dbghelp.lib on Windows builds (via #pragma comment)

## Phase 2: Library Directory Structure

- [x] 2.1.1 Create lib/backtrace/src directory
- [x] 2.1.2 Create lib/backtrace/tests directory
- [x] 2.1.3 Create mod.tml with module exports
- [ ] 2.1.4 Add backtrace to compiler library discovery

## Phase 3: Core Types Implementation

- [x] 3.1.1 Implement BacktraceSymbol type (name, filename, lineno, colno)
- [x] 3.1.2 Implement BacktraceFrame type (ip, symbol_info, resolved)
- [x] 3.1.3 Implement Backtrace type (handle, frames, resolved)
- [x] 3.1.4 Add FFI declarations for backtrace_capture
- [x] 3.1.5 Add FFI declarations for backtrace_resolve

## Phase 4: Capture Implementation

- [x] 4.1.1 Implement Backtrace::capture() - basic capture
- [x] 4.1.2 Implement Backtrace::capture_from(skip) - with frame skip
- [x] 4.1.3 Handle MAX_FRAMES limit (default 128)
- [x] 4.1.4 Store raw instruction pointers

## Phase 5: Symbolization Implementation

- [x] 5.1.1 Implement Backtrace::resolve() - lazy resolution
- [x] 5.1.2 Implement Frame::symbol() -> Maybe[Symbol]
- [ ] 5.1.3 Handle multiple symbols per frame (inlined functions)
- [x] 5.1.4 Cache resolved symbols (in frame list)

## Phase 6: Formatting

- [x] 6.1.1 Implement Backtrace::to_string()
- [x] 6.1.2 Implement BacktraceFrame::to_string()
- [x] 6.1.3 Implement BacktraceSymbol::to_string()
- [x] 6.1.4 Add frame numbering (0, 1, 2, ...)
- [x] 6.1.5 Format with file:line information
- [x] 6.1.6 Handle missing symbol info gracefully

## Phase 7: Compiler Integration

- [ ] 7.1.1 Add --backtrace flag to build command
- [ ] 7.1.2 Add --no-backtrace flag to disable
- [ ] 7.1.3 Enable backtrace by default in debug builds
- [ ] 7.1.4 Link backtrace runtime when enabled
- [ ] 7.1.5 Define BACKTRACE preprocessor symbol

## Phase 8: Panic Handler Integration

- [ ] 8.1.1 Modify core panic handler to capture backtrace
- [ ] 8.1.2 Skip panic/assert internal frames
- [ ] 8.1.3 Format backtrace in panic output
- [ ] 8.1.4 Handle nested panics (prevent infinite recursion)

## Phase 9: Test Framework Integration

- [ ] 9.1.1 Add backtrace to assert_eq failure output
- [ ] 9.1.2 Add backtrace to assert failure output
- [ ] 9.1.3 Skip test framework internal frames
- [ ] 9.1.4 Make backtrace optional via test config

## Phase 10: Library Tests

- [x] 10.1.1 Test basic capture (frames exist)
- [x] 10.1.2 Test frame skip functionality
- [x] 10.1.3 Test symbol resolution
- [x] 10.1.4 Test deep recursion (20+ frames)
- [ ] 10.1.5 Test cross-library symbol resolution
- [x] 10.1.6 Test formatting output

## Phase 11: Documentation

- [x] 11.1.1 Add docstrings to all public types
- [x] 11.1.2 Add docstrings to all public functions
- [x] 11.1.3 Add usage examples in module doc
- [ ] 11.1.4 Document platform-specific behavior
- [ ] 11.1.5 Update docs/INDEX.md with backtrace section

## Phase 12: Performance Optimization

- [ ] 12.1.1 Benchmark capture overhead
- [ ] 12.1.2 Benchmark resolve overhead
- [ ] 12.1.3 Implement symbol caching
- [ ] 12.1.4 Optimize string allocation
- [ ] 12.1.5 Consider lazy frame list allocation
