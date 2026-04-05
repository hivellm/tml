# Proposal: LLM-Assisted Debugging Through Multi-Layer IR Exposure

## Why

LLMs spend excessive tool calls navigating source files and guessing at bug root causes when compiler error messages only show surface-level information. By exposing intermediate representations (HIR, MIR, LLVM IR) directly in diagnostic output, the LLM can pattern-match on formal, unambiguous IR data instead of inferring compiler internals from natural language errors. This research will produce empirical data on LLM debugging behavior and a practical `--debug-layers` feature for the TML compiler.

## What Changes

### Infrastructure (Phase 1 — DONE)
- MCP call logger: NDJSON recording of every tool invocation (tool name, params, timestamp, duration)
- Paper structure in `docs/papers/llm-ir-debugging/` with methodology, taxonomy, and protocol

### Compiler Feature (Phase 2)
- `--debug-layers` flag for `tml test`, `tml build`, `tml check`
- On failure, automatically emits scoped IR from the compilation layer where the error originates
- Layers: SOURCE → HIR → THIR → MIR → LLVM IR (selectable)

### MCP Integration (Phase 3)
- `debug_layers` parameter on `test`, `build`, `check` MCP tools
- Enhanced error output with structured multi-layer diagnostics

### Experiment (Phase 4)
- Curate 30 bugs from TML git history (5 per compilation layer)
- Run A/B/C sessions: baseline vs enhanced errors vs full debug-layers
- Analyze tool usage patterns, IR preference rate, fix efficiency

### Paper (Phase 5)
- Write research paper with empirical results and design guidelines

## Impact
- Affected specs: None (new feature, no breaking changes)
- Affected code: `compiler/src/mcp/mcp_server.cpp`, `compiler/src/cli/`, `compiler/src/testing/`, `compiler/src/codegen/`
- Breaking change: NO
- User benefit: Faster debugging for both LLMs and humans. Compiler produces richer diagnostics. Research contribution to LLM-assisted development tooling.
