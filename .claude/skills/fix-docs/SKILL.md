---
name: fix-docs
description: Detect and fix documentation violations across C++ compiler and TML library code. Scans for missing ///, undocumented public items, wrong comment styles, and auto-fixes using parallel agents. Use when the user says "fix docs", "audit docs", "documenta", "corrige docs", or wants to improve documentation coverage.
user-invocable: true
argument-hint: "[optional scope — e.g. 'compiler', 'lib/core', 'lib/std', 'lib/std/src/db', 'all']"
---

**Delegation**: Use the Agent tool to dispatch a `team-lead` agent with `model: sonnet` for Phase 2 (fix). Phase 1 (audit) runs in the main conversation via script.

## Documentation Standard

Every public item MUST have `///` documentation following Rust-style conventions.

### TML Files

```tml
//! Module-level documentation (first lines of file).
//!
//! Describes the module's purpose and key types.

/// One-line summary of the function.
///
/// Optional elaboration with design context.
pub func example(param: Str) -> I64 {
```

### C++ Files

```cpp
/// One-line summary — WHAT it does, not HOW.
///
/// Optional elaboration.
///
/// # Arguments
/// * `param1` — description.
///
/// # Returns
/// Success case. Failure case.
auto example(std::string param) -> int;
```

### Exemplar Files (gold standard)

**TML:**
- `lib/core/src/str/mod.tml` — Best module-level + function docs
- `lib/std/src/db/driver/connection.tml` — Best behavior documentation
- `lib/std/src/collections/list.tml` — Best type + method docs
- `lib/core/src/fmt/mod.tml` — Best trait/behavior docs

**C++:**
- `compiler/include/types/env.hpp` — Best class + method docs
- `compiler/include/mir/mir.hpp` — Best enum + struct docs
- `compiler/include/query/query_key.hpp` — Best concise docs

## Violation Types

| Code | Description | Applies To |
|------|-------------|------------|
| V1 | Missing file header (`///` or `//!`) | C++ & TML |
| V2 | Undocumented public class/struct | C++ |
| V3 | Undocumented public method | C++ |
| V5 | Undocumented macro | C++ |
| V7 | Wrong comment style (`/* */` or `//!` before class) | C++ |
| V9 | Undocumented static function >10 lines | C++ (.cpp) |
| V11 | Undocumented TML function | TML |
| V12 | Undocumented TML type/enum | TML |
| V13 | Undocumented TML behavior | TML |
| V14 | Missing TML module doc (`//!`) | TML |

## Phase 1: Automated Audit (NO agents — use script or manual scan)

### Option A: Run the TML audit script (if it compiles)

```
mcp__tml__run with file="scripts/audit_docs.tml"
```

### Option B: Manual scan with Grep (reliable fallback)

For TML files — find undocumented public functions:
```bash
# Find pub func without preceding ///
grep -n "pub func " lib/core/src/**/*.tml lib/std/src/**/*.tml | head -50
```

For C++ files — find undocumented classes:
```bash
grep -n "^class \|^struct " compiler/include/**/*.hpp | head -50
```

### Scope Selection (from $ARGUMENTS)

| Argument | Scan Targets |
|----------|-------------|
| (none) or `all` | All C++ headers/sources + all TML lib code |
| `compiler` | `compiler/include/**/*.hpp` + `compiler/src/**/*.cpp` |
| `lib/core` | `lib/core/src/**/*.tml` |
| `lib/std` | `lib/std/src/**/*.tml` |
| `lib/std/src/db` | Only `lib/std/src/db/**/*.tml` |
| Any path | Scan that specific path recursively |

### Audit Output

Produce a violation summary:
1. **Total files scanned**
2. **Files with violations** (count)
3. **Violations by type** (V1: N, V11: N, etc.)
4. **Top 10 worst files** (most violations)
5. **Full violation list** grouped by directory

## Phase 2: Fix (parallel agents via team-lead)

Based on the audit results, split files-to-fix into groups:

- **Group A**: `compiler/include/` — C++ headers
- **Group B**: `compiler/src/` — C++ sources
- **Group C**: `lib/core/src/` — Core TML library
- **Group D**: `lib/std/src/` — Std TML library

Only spawn agents for groups that have violations. Each agent receives:
1. The exact list of files + line numbers + violation types
2. Instructions to read the file, read one exemplar, then fix

### Agent Instructions (per group)

Each fix agent MUST:

1. **Read the file** before editing
2. **Read at least one exemplar** from the same category to match the style
3. **Fix only violations** — do NOT rewrite docs that are already compliant
4. **Edit sequentially** — one file at a time
5. **Preserve existing good docs** — never downgrade quality
6. **Use `mcp__tml__check`** after editing each TML file to verify no parse errors

### Fix Templates by Violation Type

**V1/V14 fix (missing file/module header):**
```tml
//! Module description — what this module provides.
//!
//! Key types and their purpose.
```

**V11 fix (undocumented TML function):**
```tml
/// One-line summary of what the function does.
///
/// Optional elaboration if the function is non-trivial.
pub func example(param: Str) -> I64 {
```

**V12 fix (undocumented TML type/enum):**
```tml
/// One-line summary of the type's purpose.
///
/// When to use this type and key invariants.
type Example {
    /// Description of this field.
    field: I64,
}
```

**V13 fix (undocumented TML behavior):**
```tml
/// Description of the contract this behavior defines.
///
/// Implementors must provide...
behavior Example {
    /// What this method should do.
    func method(this) -> I64
}
```

**V2/V3 fix (C++ class/method):**
```cpp
/// One-line summary — WHAT it does.
///
/// # Arguments
/// * `param` — description.
///
/// # Returns
/// What is returned on success. What on failure.
```

**V7 fix (wrong style):**
Replace `/* */` or `//!` before a class/struct with `///`.

## Phase 3: Verify

1. Re-run the audit on fixed files to confirm violations are resolved
2. For TML files: run `mcp__tml__check` on each modified file
3. For C++ files: build the compiler to verify no syntax errors
4. Report before/after violation counts

## Important Rules

- **Do NOT add docs to test files** (`*.test.tml`) — tests are self-documenting
- **Do NOT add docs to private/internal helpers** unless >10 lines
- **Do NOT restate the function name** — `/// Returns the count` on `func count()` is useless
- **DO explain WHY**, not just WHAT — "Panics if index is out of bounds" is more useful than "Gets an element"
- **DO document edge cases** — null, empty, overflow, thread safety
- **Match the existing style** in each module — don't introduce a new convention
