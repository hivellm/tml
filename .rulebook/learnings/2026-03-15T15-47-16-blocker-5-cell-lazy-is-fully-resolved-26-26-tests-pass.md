# BLOCKER #5 cell/lazy is FULLY RESOLVED — 26/26 tests pass
**Source**: manual
**Date**: 2026-03-15
**Tags**: blocker, resolved, cell-lazy, llvm-types
The %struct.I32__Fn unsized type error was fixed across multiple sites in llvm_types.cpp: line 119 maps FuncType 'Fn' to '{ ptr, ptr }', lines 614-660 check every struct field for FuncType and use fat pointer, line 819-822 llvm_type_from_semantic maps FuncType to '{ ptr, ptr }', line 1071-1073 mangle_type maps FuncType to 'Fn'. LazyCell[I32, F] now correctly instantiates with the fat pointer field representation. No action needed.