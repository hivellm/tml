# Fat Pointer Thin/Capture Branch for Closure Calls

**Category**: codegen
**Tags**: codegen, closure, fat-pointer, indirect-call

## Description

Every call through a function pointer stored in a struct field emits icmp null + br + phi (5+ instructions) to distinguish thin (non-capturing, env=null) from fat (capturing, env!=null) closure pointers. The thin path calls the function directly; the fat path passes the captured environment as the first argument. This is correct but adds overhead on every indirect call.

## When to Use

When calling function pointers that may or may not be closures (the common case in TML where Func and closures share the same representation). Implemented in call.cpp:192-272.

## When NOT to Use

When the compiler can statically prove the callee is a non-capturing function (optimization opportunity: elide the branch).
