# Proposal: Next Compiler Features

## Why

Implement critical missing features for TML compiler including closures, where clauses, better error messages, and quality-of-life improvements for module system and type constraints.

## What Changes

1. **Closures**: Implement environment capture, closure types, and code generation
2. **Where Clauses**: Enforce type constraints in function signatures
3. **LLVM Runtime**: Better linking for runtime functions
4. **Use Groups**: Support grouped imports like `use std::{option, result}`
5. **Named Enum Fields**: Allow struct-like enum variants
6. **Error Messages**: Improve compiler diagnostics with better context
7. **String Interpolation**: Support `"value: {x}"` syntax
8. **Inline Modules**: Allow `mod name { ... }` syntax
9. **Doc Generation**: Generate documentation from doc comments

## Implementation Tasks

See tasks.md for detailed checklist.
