# Proposal: phase0r_compiler-output-pipe-hang

## Why

`tml check`, `tml run`, and `tml build` hang indefinitely when stdout/stderr are
redirected to a pipe (`tml check file.tml 2>&1 | cat`, subprocesses, IDE tooling,
CI runners, and the TML MCP server which captures output via pipe). The process
starts but never writes output and never exits.

This is the highest-priority UX bug in TML. Reported by the UzDB AI agent who
spent hours trying to see type errors from `spike_01_btreemap.tml` and never
could — the compiler was silently hanging. The MCP server (`mcp__tml__check`) is
also affected because it captures stdout via pipe. The irony: TML was designed for
AI agent integration, but AI agents cannot compile anything.

Root cause hypothesis: Windows CRT full-buffering on non-TTY stdout/stderr causes
the output buffer to never flush when the process is waiting for more I/O.
Alternatively, the compiler calls a Windows console API (e.g., `GetConsoleMode`)
that blocks on non-console handles.

## What Changes

1. Force unbuffered output on non-TTY handles early in `main()`:
   `setvbuf(stdout, nullptr, _IONBF, 0)` and `setvbuf(stderr, nullptr, _IONBF, 0)`.
2. If CRT buffering is not the root cause, audit all diagnostic output paths for
   blocking console API calls and replace with direct `WriteFile` or `fputs`.
3. Add regression test: `tml check file.tml 2>&1 | cat` must produce output and
   exit within 10 seconds regardless of TTY state.

## Impact

- Affected specs: compiler CLI, diagnostic output, MCP tool output
- Affected code: compiler entry point (`main.cpp`), diagnostic formatters,
  log output paths
- Breaking change: NO (output content unchanged; only buffering behavior)
- User benefit: IDE integrations, AI agents, CI pipelines, and the MCP server
  all work correctly.
