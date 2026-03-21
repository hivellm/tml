# Closures Always Heap-Allocate Capture Environment

**Category**: codegen
**Tags**: codegen, closure, performance, memory

## Description

closure.cpp:372 always calls @malloc for closure capture environments, even when the closure's lifetime is bounded to the current stack frame. Rust can stack-allocate non-escaping closures. Additionally, collect_codegen_captures() is an incomplete AST visitor — missing StructInitExpr, TupleExpr, CastExpr, LoopExpr, TryExpr, nested closures, WhenExpr arms.

## When NOT to Use

For non-escaping closures (closure doesn't outlive the current function), the capture environment should be stack-allocated. Heap allocation should only be used when the closure escapes.
