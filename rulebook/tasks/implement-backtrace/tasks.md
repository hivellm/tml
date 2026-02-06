# Tasks: Implement Backtrace Library

**Status**: Complete (97%)

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
- [x] 2.1.4 Add backtrace to compiler library discovery

## Phase 3: Core Types Implementation

- [x] 3.1.1 Implement BacktraceSymbol type (name, filename, lineno, colno)
- [x] 3.1.2 Implement BacktraceFrame type (ip, symbol_info, resolved)
- [x] 3.1.3 Implement Backtrace type (handle, frames, resolved)
- [x] 3.1.4 Add FFI declarations for backtrace_capture
- [x] 3.1.5 Add FFI declarations for backtrace_resolve
- [x] 3.1.6 Implement codegen workaround for Maybe[Str] struct field access

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

- [x] 7.1.1 Add --backtrace flag to run command
- [x] 7.1.2 Add CompilerOptions::backtrace flag
- [x] 7.1.3 Generate tml_enable_backtrace_on_panic() call in main
- [x] 7.1.4 Link backtrace runtime (backtrace.c always compiled)
- [x] 7.1.5 Declare tml_enable_backtrace_on_panic in runtime.cpp

## Phase 8: Panic Handler Integration

- [x] 8.1.1 Modify core panic handler to capture backtrace
- [x] 8.1.2 Skip panic/assert internal frames
- [x] 8.1.3 Format backtrace in panic output
- [x] 8.1.4 Handle nested panics (prevent infinite recursion)

## Phase 9: Test Framework Integration

- [x] 9.1.1 Add backtrace to assert_eq failure output
- [x] 9.1.2 Add backtrace to assert failure output
- [x] 9.1.3 Skip test framework internal frames
- [x] 9.1.4 Make backtrace optional via test config

## Phase 10: Library Tests (32 tests total)

### Backtrace Capture Tests (5 tests)
- [x] 10.1.1 test_capture_returns_backtrace
- [x] 10.1.2 test_capture_from_skip
- [x] 10.1.3 test_to_string_produces_output
- [x] 10.1.4 test_resolve_is_idempotent
- [x] 10.1.5 test_capture_backtrace_convenience

### BacktraceSymbol Tests (15 tests)
- [x] 10.2.1 test_empty_symbol_fields
- [x] 10.2.2 test_new_full_fields
- [x] 10.2.3 test_new_with_different_linenos
- [x] 10.2.4 test_with_name_fields
- [x] 10.2.5 test_has_name_true
- [x] 10.2.6 test_has_name_false
- [x] 10.2.7 test_has_location_true
- [x] 10.2.8 test_has_location_false_no_file
- [x] 10.2.9 test_has_location_false_no_line
- [x] 10.2.10 test_to_string_empty
- [x] 10.2.11 test_to_string_name_only
- [x] 10.2.12 test_to_string_with_file
- [x] 10.2.13 test_to_string_with_line
- [x] 10.2.14 test_to_string_full
- [x] 10.2.15 test_max_lineno

### BacktraceFrame Tests (12 tests)
- [x] 10.3.1 test_new_creates_unresolved_frame
- [x] 10.3.2 test_new_stores_ip
- [x] 10.3.3 test_with_symbol_creates_resolved_frame
- [x] 10.3.4 test_with_symbol_stores_ip
- [x] 10.3.5 test_symbol_returns_nothing_for_new_frame
- [x] 10.3.6 test_symbol_returns_just_for_resolved_frame
- [x] 10.3.7 test_to_string_unresolved_frame
- [x] 10.3.8 test_to_string_resolved_frame
- [x] 10.3.9 test_format_unresolved_frame
- [x] 10.3.10 test_format_resolved_frame
- [x] 10.3.11 test_null_ip
- [x] 10.3.12 test_multiple_frames_independent

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
