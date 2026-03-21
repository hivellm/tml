# String-Based Type Encoding in Legacy Codegen

**Category**: codegen
**Tags**: codegen, legacy, tech-debt, types

## Description

Type information is encoded as LLVM type strings ("%struct.Maybe__I32") and decoded by string parsing (find/substr/starts_with). generic.cpp has 64 string ops for this; call.cpp has 60. This is error-prone and slow relative to carrying types::TypePtr through the pipeline. The MIR path correctly carries typed references — this is a legacy-only problem.

## When NOT to Use

Never encode type information as strings when a typed representation (TypePtr) is available. This pattern exists only because the legacy path was built before the type system was fully integrated with codegen.
