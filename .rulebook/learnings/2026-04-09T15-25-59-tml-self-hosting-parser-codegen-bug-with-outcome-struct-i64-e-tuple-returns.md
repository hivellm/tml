# TML self-hosting parser: codegen bug with Outcome[(struct, I64), E] tuple returns
**Source**: manual
**Date**: 2026-04-09
**Related Task**: phase13c_tml-parser
**Tags**: codegen, bug, outcome, tuple, llvm, parser, k001
When a function returns `Outcome[(StructType, I64), E]` (tuple inside Outcome), and the caller uses `!` to unwrap it then accesses `.0` to get the struct, LLVM codegen generates an `i32` (the discriminant) instead of the struct payload. This is K001: "defined with type 'i32' but expected '%struct.TypePath'".

**Fix**: Replace ALL `Outcome[(X, I64), E]` tuple returns with named structs: `type ParsedX { val: X, pos: I64 }` and return `Ok(ParsedX { val: ..., pos: p })`. Callers use `result.val` and `result.pos` instead of `.0` and `.1`.

**Scope**: This bug affects ANY Outcome that wraps a tuple containing a non-trivial struct type. Simple tuples not inside Outcome (bare `(A, B)` returns) may work fine but should also be converted to named structs for safety.

**Diagnostic**: The bug manifests at the LLVM IR level only when the result's fields are actually accessed. Pattern matching `when result { Ok(_) => ... }` without field access does NOT trigger the bug, making it hard to isolate.

**Pattern to avoid**: `-> Outcome[(TypePath, I64), ParseError]`  
**Pattern to use**: `-> Outcome[ParsedTypePath, ParseError]` with `pub type ParsedTypePath { path: TypePath, pos: I64 }`