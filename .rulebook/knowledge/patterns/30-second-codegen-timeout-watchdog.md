# 30-Second Codegen Timeout Watchdog

**Category**: testing
**Tags**: testing, llvm, timeout, reliability

## Description

A separate thread runs qctx.codegen_unit() with a 30-second watchdog timer. If LLVM hangs (known to occur with certain IR patterns involving recursive types or infinite loops in optimization passes), the suite is skipped rather than blocking the whole test run. This is essential for test system reliability.

## When to Use

Always wrap LLVM codegen invocations in test compilation with a timeout. Applied in testing_compile.cpp line 381.
