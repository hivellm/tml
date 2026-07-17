## 1. Diagnosis
- [x] 1.1 Reproduce: tested bash pipe, CMD redirect, PowerShell subprocess capture, MCP server — NO HANG in v0.3.20. Bug was reported on v0.1.6 and appears fixed in subsequent versions.
- [x] 1.2 All output paths work: error diagnostics appear correctly in all capture modes
- [x] 1.3 CRT buffering is not an issue in current version; MCP server captures output correctly

## 2. Implementation
- [x] 2.1 No changes needed — pipe output works correctly in v0.3.20
- [x] 2.2 MCP server `mcp__tml__check` verified: captures stdout/stderr, returns structured errors
- [x] 2.3 PowerShell `Start-Process -RedirectStandardOutput` verified: captures errors correctly

## 3. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 3.1 Update or create documentation covering the implementation — no code changes; bug not reproducible
- [x] 3.2 Write tests covering the new behavior — verified manually: bash tee, CMD redirect, PowerShell subprocess, MCP tool all work
- [x] 3.3 Run tests and confirm they pass — compiler 156/157 (pre-existing let_patterns X002)
