---
name: test-suite
description: Run tests for a specific suite/module only. Use when the user says "testa core/str", "test std/json", "roda os testes de str", or wants to verify a specific module. Much faster than full suite.
user-invocable: true
allowed-tools: mcp__tml__test, Read, Grep, Glob
argument-hint: suite name like core/str, std/json, std/collections
---

## Suite Test Workflow

Run tests for a SPECIFIC suite module. This is dramatically faster than a full test run.

### Parse Arguments

`$ARGUMENTS` should be a suite name like:
- `core/str`, `core/fmt`, `core/num`, `core/iter`, `core/ops`, `core/alloc`
- `std/json`, `std/collections`, `std/crypto`, `std/io`, `std/sync`, `std/net`, `std/zlib`
- `compiler/compiler`, `compiler/runtime`, `compiler/borrow`

If the argument looks like a module name without prefix (e.g., just "str" or "json"), infer:
- `str`, `fmt`, `num`, `iter`, `ops`, `alloc`, `hash`, `mem`, `slice`, `ptr`, `char`, `option`, `result`, `error`, `convert`, `cmp`, `types` → `core/<name>`
- `json`, `collections`, `crypto`, `io`, `sync`, `net`, `zlib`, `file`, `os`, `regex`, `text`, `time`, `random`, `math`, `glob`, `thread` → `std/<name>`
- `compiler`, `borrow`, `runtime`, `preprocessor`, `thir` → `compiler/<name>`

### Execute

Call `mcp__tml__test` with:
- `suite`: The resolved suite name
- `no_cache`: true (always fresh for targeted testing)
- `fail_fast`: true (stop early on failures)
- `structured`: true (for clean results)

### Report

- Report passed/failed counts
- If failures, show which tests failed
- Note: this runs in suite mode (multiple tests compiled into one DLL), which matches how the full test suite runs — so results are representative