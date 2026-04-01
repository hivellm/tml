# Use MCP debug/profile tools proactively when debugging TML code

When writing or debugging TML code, ALWAYS use the runtime debug tools instead of guessing.

## Rules

1. **After implementing any TML module** — run `mcp__tml__debug(file, check_leaks=true)` to verify no leaks
2. **When a test crashes** — run `mcp__tml__debug(file, backtrace=true)` BEFORE reading source code
3. **When debugging logic** — add `use std::console` + `console.log(value)` to the TML code, run, observe
4. **When investigating performance** — run `mcp__tml__profile(file, flamegraph=true)` for ASCII flame graph
5. **NEVER guess at crash causes** — always get the backtrace first

## Available Tools

| Tool | Purpose |
|------|---------|
| `mcp__tml__debug(file, check_leaks=true)` | Run with memory leak detection |
| `mcp__tml__debug(file, backtrace=true)` | Run with stack traces on panic |
| `mcp__tml__profile(file, flamegraph=true)` | CPU profile + ASCII flame graph |
| `mcp__tml__inspect(file, port=N)` | Start WebSocket inspector |
| `std::console` in TML code | `log`, `warn`, `error`, `time`, `count`, `assert`, `group`, `table` |

## Why

These tools exist specifically for LLM-assisted debugging. Using them saves hours of guessing.
A backtrace tells you exactly where the crash happened. A leak check tells you exactly what leaked.
Console.log tells you exactly what value was computed. Don't hypothesize — observe.