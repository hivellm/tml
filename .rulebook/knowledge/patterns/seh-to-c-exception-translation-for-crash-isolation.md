# SEH-to-C++ Exception Translation for Crash Isolation

**Category**: testing
**Tags**: testing, crash-isolation, windows, seh

## Description

On Windows, wrap compilation calls with Structured Exception Handling (SEH) that translates access violations and stack overflows into C++ exceptions. This gives per-suite crash isolation without process termination — a crashed suite is reported as failed without aborting the coordinator.

## When to Use

When running untrusted or potentially-crashing code in-process. Applied in compile_suite_safe() in testing_compile.cpp.
