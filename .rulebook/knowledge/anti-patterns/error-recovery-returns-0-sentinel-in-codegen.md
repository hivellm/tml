# Error Recovery Returns "0" Sentinel in Codegen

**Category**: codegen
**Tags**: codegen, error-handling, tech-debt

## Description

On codegen failure, report_error() is called and the function returns "0" (a valid LLVM integer constant). IR generation continues after errors, potentially producing invalid IR that LLVM rejects with confusing messages. Found in 9 places in call.cpp, 5 in method.cpp.

## When NOT to Use

Never continue IR generation after a fatal codegen error. Either abort the current function's codegen or use a proper error-propagation mechanism (Result type, exception) to halt gracefully.
