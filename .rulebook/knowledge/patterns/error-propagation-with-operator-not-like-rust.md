# Error Propagation with ! Operator (not ? like Rust)

**Category**: language
**Tags**: language, error-handling, syntax

## Description

TML uses ! (postfix) for error propagation, not ? like Rust. On Outcome: Err propagates up, Ok unwraps. On Maybe in Outcome-returning function: Nothing becomes Err(Error.NothingValue). In non-Outcome function: ! panics on error. Supports inline recovery: 'expr! else default_value' and 'expr! else do(err) { recovery }'. Block-level: 'catch { ... } else do(err) { ... }'.

## When to Use

When writing or reviewing TML code with error handling. The ! is deliberately more visible than ? to prevent missed error points in LLM-generated code.
