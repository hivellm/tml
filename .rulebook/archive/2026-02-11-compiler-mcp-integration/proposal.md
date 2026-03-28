# Proposal: MCP Integration for Compiler-as-a-Service

## Why

The TML compiler is currently treated as a traditional CLI tool. For efficient integration with LLMs and AI agents, we need to expose the compiler as a service (compiler-as-a-service) with a stable, deterministic, and "machine-first" API. The MCP (Model Context Protocol) is the emerging standard for communication between LLMs and external tools.

**Current problems:**
- LLMs need to use unstable and fragile shell commands
- Unstructured text output is difficult to parse
- No support for in-memory editing (file overlays)
- No capability for safe refactoring via AST/IR
- Diagnostics are not machine-readable
- No auto-fix or automatically applicable fix-its

**Benefits of MCP integration:**
- Structured API with strict JSON Schema
- Predictable and deterministic output
- Capability-based security (no "execute arbitrary command")
- Support for file overlays (editing without touching filesystem)
- Automatic fix-its applicable by agents
- Safe refactoring via AST/IR manipulation

## What Changes

### 1. Compiler Library Refactoring
- Separate compiler core into reusable library
- Stable API for: parse, typecheck, lint, format, build
- Diagnostics with stable codes, spans, severity, fix-its
- Support for file overlays (in-memory text)

### 2. Compilation Daemon
- Persistent process that maintains workspace state
- Incremental cache and symbol index
- Accepts commands via IPC (stdio/socket)
- Fast response for common operations

### 3. MCP Server Implementation
- JSON-RPC 2.0 server compatible with MCP spec
- Exposed tools: analyze, lint, format, build, test, debug, profile
- Each tool with validated JSON Schema
- Structured output with diagnostics, fixits, artifacts

### 4. CLI Thin Client
- `tml build` now calls the daemon
- Fallback to standalone execution if daemon unavailable
- Consistency between IDE, CI, and MCP

## Impact

### Specs Affected
- `compiler/cli` - Refactor to use daemon
- `compiler/core` - Expose as library
- NEW: `compiler/daemon` - Compilation server
- NEW: `compiler/mcp` - MCP server implementation

### Code Affected
- `compiler/src/cli/` - Refactor to thin client
- `compiler/src/` - Separate into lib + bin
- NEW: `compiler/src/daemon/` - Daemon implementation
- NEW: `compiler/src/mcp/` - MCP protocol handler

### Breaking Changes
- None for CLI users
- New dependency: JSON library for MCP protocol

### User Benefits
- LLMs can interact with the compiler reliably
- Auto-fix of errors applicable automatically
- Safe refactoring (rename, extract, inline)
- AI agent-guided editing
- Integrated debug and profile via MCP
