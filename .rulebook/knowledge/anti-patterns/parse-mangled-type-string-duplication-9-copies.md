# parse_mangled_type_string Duplication (9+ copies)

**Category**: codegen
**Tags**: codegen, duplication, tech-debt, legacy

## Description

The function parse_mangled_type_string is duplicated 9+ times across call.cpp, method.cpp, call_generic_struct.cpp, llvm_struct_expr.cpp, infer.cpp, unary.cpp, method_static_dispatch.cpp, impl.cpp, generic.cpp, runtime_modules.cpp. Each copy is slightly different — some handle mutref_, others don't. Bug fixes to one copy don't propagate to others. This is a maintenance hazard.

## When NOT to Use

Never copy this function again. Instead, consolidate into a single implementation in llvm_utils.cpp and call it everywhere.
