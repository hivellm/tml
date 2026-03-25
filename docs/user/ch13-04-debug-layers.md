# Debug Layers

When a test fails, it can be difficult to determine which compilation layer caused the problem. The `--debug-layers` flag automatically emits intermediate representations (HIR, MIR, LLVM IR) for failing tests, along with diagnosis hints.

## Usage

```bash
tml test --debug-layers                       # All tests with debug output on failure
tml test --debug-layers --suite=core/str      # Specific suite
tml test --debug-layers --filter=my_test      # Specific test
```

## What It Does

When a test fails at runtime (compiles successfully but assertion fails or crashes), `--debug-layers` automatically:

1. Re-compiles the failing test's source file with `--emit-hir`
2. Re-compiles with `--emit-mir`
3. Re-compiles with `--emit-ir`
4. Extracts the function-scoped output for the failing test
5. Appends all three layers plus diagnosis hints to the test error

## Output Format

```
FAIL: test_str_repeat (lib/core/tests/str/repeat.test.tml)
  Expected: "abcabc"
  Got:      ""

=== HIR (--debug-layers) ===
func test_str_repeat() -> Unit {
    let result: Text = "abc".repeat(2)
    assert_eq(result, "abcabc")
}

=== MIR (--debug-layers) ===
fn test_str_repeat() -> Unit {
  bb0:
    %1 = const_str "abc"
    %2 = const_i64 2
    %3 = call @str_repeat(%1, %2) -> %Text
    store %3 -> _result
    ...
}

=== LLVM IR (--debug-layers) ===
define void @test_str_repeat() {
entry:
  %0 = call { ptr, i64 } @tml_str_repeat(ptr @.str.abc, i64 3, i64 2)
  ...
}

=== DIAGNOSIS HINTS ===
Layer: RUNTIME or LIBRARY
Symptom: Assertion failure with all compilation layers looking correct
Possible causes:
  - Logic error in TML library code
  - C runtime function returning wrong value
  - Memory layout mismatch between TML and C runtime
Focus function: test_str_repeat
```

## Diagnosis Hints

The compiler analyzes the error and emitted layers to suggest the likely bug layer:

| Hint Layer | Meaning |
|-----------|---------|
| PARSER or TYPE SYSTEM | HIR generation failed — source didn't parse or type-check |
| HIR → MIR LOWERING | HIR succeeded but MIR failed — monomorphization or desugaring bug |
| CODEGEN (MIR → LLVM IR) | MIR succeeded but IR failed — type layout or ABI mismatch |
| CODEGEN (calling convention) | sret + void call pattern — return value corruption |
| RUNTIME or LIBRARY | All layers look correct — logic error or C runtime bug |

## MCP Integration

When using the TML MCP server (e.g., from Claude Code), pass the `debug_layers` parameter:

```json
{
  "tool": "test",
  "params": {
    "suite": "core/str",
    "debug_layers": true
  }
}
```

## When to Use

- Test fails with an assertion error and you don't know which layer is wrong
- Test crashes (exit code != 0) and you need to see the generated code
- Comparing what the compiler generates vs what you expect
- Investigating ABI mismatches between TML and C runtime

## Performance Note

`--debug-layers` re-compiles each failing test's source file three times (HIR, MIR, IR). This adds ~10-30 seconds per failing file. Use it only when debugging, not for routine test runs.
