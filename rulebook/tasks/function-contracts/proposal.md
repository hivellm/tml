# Proposal: Function Contracts (Pre/Post Conditions)

## Summary

Add `pre:` and `post:` clauses to function declarations for runtime-checked preconditions and postconditions. This was originally part of the TML v1.0 spec but was removed because it was never implemented. It is the most practically valuable of the removed LLM-specific features.

## Motivation

Contracts provide formal documentation of function requirements that is machine-verifiable:

```tml
func sqrt(x: F64) -> F64
pre: x >= 0.0
post(result): result >= 0.0
{
    return x.sqrt_impl()
}

func divide(a: I32, b: I32) -> I32
pre: b != 0
{
    return a / b
}
```

Benefits:
- **Self-documenting**: Contracts make function requirements explicit
- **LLM-friendly**: LLMs can read contracts to understand function requirements without reading implementation
- **Runtime safety**: Catches bugs early with clear error messages
- **Testability**: Contracts serve as built-in property tests

## Design

### Syntax

```ebnf
Contract   = PreCond? PostCond?
PreCond    = 'pre' ':' Expr
PostCond   = 'post' ('(' Ident ')')? ':' Expr
```

### Semantics

- `pre:` expressions are evaluated at function entry; panic if false
- `post(result):` expressions are evaluated before each return; panic if false
- The `result` binding in `post` refers to the return value
- Contracts are inherited by behavior implementations
- Contracts can be disabled with `--contracts=off` for production builds

### Verification Modes

```toml
# tml.toml
[contracts]
mode = "runtime"   # check at runtime (default in debug)
mode = "off"       # disable (default in release)
```

## Priority

Low - this is a nice-to-have feature that adds safety but is not blocking any current use cases. Implement after core language features are stable.
