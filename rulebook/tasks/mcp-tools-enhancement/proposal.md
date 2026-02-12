# Proposal: MCP Tools Enhancement

## Context

The TML compiler's MCP server currently has 14 tools focused on **TML source file operations** (compile, check, test, docs, etc.). However, there are zero tools for **project-level operations** — building the compiler itself, structured error output, coverage reports, and project exploration. These are the operations where Bash fallback is most error-prone, especially on Windows (path escaping, PowerShell vs cmd vs Git Bash differences, ANSI codes in output).

## Problem

Common operations that currently require Bash and have high error rates:

1. **Building the compiler** — Requires `cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat --no-tests" 2>&1` with Windows-specific escaping
2. **Test suite with coverage** — Missing `no_cache` and `fail_fast` params; output is raw ANSI text
3. **Coverage reports** — Requires manually reading and parsing `coverage_report.txt`
4. **Project exploration** — `ls`, `find`, `tree` behave differently across shells
5. **Error parsing** — All tools return ANSI-colored unstructured text, wasting tokens
6. **Error explanations** — `cmd_explain.cpp` exists but isn't exposed via MCP

## Proposed Tools

### P0 (Critical)

#### 1. `project/build` — Build the TML Compiler from C++ Sources

Wraps `scripts/build.bat` (or future cross-platform build script) with structured output.

**Parameters:**
- `mode` (string, optional): `"debug"` (default) or `"release"`
- `clean` (boolean, optional): Clean build directory first
- `tests` (boolean, optional): Build test executable (default: true)
- `timeout_ms` (number, optional): Build timeout (default: 300000)

**Returns:** `{ success: bool, duration_ms: number, output_path: string, warnings: number, errors: [...] }`

**Why:** Eliminates ~80% of Windows path/escaping errors. Uses existing `execute_command()` and `find_tml_root()` infrastructure.

**Implementation:** ~80 lines in `mcp_tools.cpp`. Invoke build script via `_popen`/`popen` with proper working directory.

#### 2. `test` Enhancement — Add Missing Parameters

The existing `test` tool needs:
- `no_cache` (boolean): Force recompilation of all tests
- `fail_fast` (boolean): Stop on first failure
- `structured` (boolean): Return JSON-structured results

**Returns (structured mode):** `{ total: number, passed: number, failed: number, duration_ms: number, coverage?: { functions: number, lines: number }, failures: [{ file, test, error }] }`

**Why:** `TestOptions` struct already supports these flags. Only ~15 lines needed to wire them through.

#### 3. ANSI Stripping — All Tool Output

Strip ANSI escape codes (`\033[...m`) from ALL tool output by default. MCP communicates via JSON-RPC — color codes waste tokens and confuse parsing.

**Implementation:** ~30-line `strip_ansi()` helper applied to all `execute_command()` results.

### P1 (High Impact)

#### 4. `project/coverage` — Parse Coverage Reports

Read and return structured coverage data from the last test run.

**Parameters:**
- `module` (string, optional): Filter to specific module (e.g., `"core::str"`)
- `sort` (string, optional): `"lowest"`, `"name"`, `"highest"` (default: `"lowest"`)
- `limit` (number, optional): Max modules to return

**Returns:** `{ total_pct: number, modules: [{ name, functions_covered, functions_total, pct }] }`

**Implementation:** ~120 lines. Parse `coverage_report.txt` format.

#### 5. `explain` — Error Code Explanation

Expose the existing `cmd_explain.cpp` functionality via MCP.

**Parameters:**
- `code` (string, required): Error code (e.g., `"E0308"`)

**Returns:** Error description, common causes, fix examples.

**Implementation:** ~40 lines. Delegate to existing explain infrastructure.

### P2 (Quality of Life)

#### 6. `project/structure` — Module Layout Explorer

Enumerate TML modules with file counts, test files, and source structure.

**Parameters:**
- `module` (string, optional): Module path to explore
- `include_tests` (boolean, optional): Include test files (default: true)
- `depth` (number, optional): Traversal depth (default: 3)

**Returns:** Tree of modules with metadata.

**Implementation:** ~100 lines using `std::filesystem`.

#### 7. `project/affected-tests` — Change-Aware Test Execution

Identify tests affected by file changes (via git diff).

**Parameters:**
- `files` (array, optional): Changed files (default: auto-detect from git diff)
- `run` (boolean, optional): Run affected tests (default: false)

**Returns:** List of affected test files, optionally with results.

**Implementation:** ~200 lines. Git diff parsing + dependency heuristics.

### P3 (Nice to Have)

#### 8. `project/artifacts` — Build Artifact Inspector

List build artifacts with metadata (size, date, staleness).

**Parameters:**
- `category` (string, optional): `"all"`, `"executables"`, `"cache"`, `"coverage"`, `"tests"`

**Returns:** Artifact listing with file sizes and modification times.

**Implementation:** ~80 lines using `std::filesystem::directory_iterator`.

## Implementation Strategy

All new tools use existing infrastructure:
- `execute_command()` (line ~412 in mcp_tools.cpp) — subprocess execution
- `find_tml_root()` (line ~946) — project root discovery
- `get_tml_executable()` (line ~444) — find built `tml.exe`
- `register_compiler_tools()` — tool registration pattern

Total estimated effort: ~665 lines of C++ across all 8 items.

## Impact Summary

| Tool | Error Reduction | Time Savings per Use |
|------|----------------|---------------------|
| `project/build` | ~80% of Windows shell errors | 2-5 min per failed attempt |
| `test` enhancement | Eliminates ANSI parsing | 1-3 min per test cycle |
| ANSI stripping | All tools cleaner | Saves tokens every call |
| `project/coverage` | No manual file parsing | 1-2 min per coverage check |
| `explain` | Reduces debugging guesswork | 1-2 min per error |
| `project/structure` | No Windows find/ls issues | 30s per exploration |
| `project/affected-tests` | New capability | 2-5 min per change cycle |
| `project/artifacts` | Reduce diagnostic calls | 30s per inspection |
