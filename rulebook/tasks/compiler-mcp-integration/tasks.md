# Tasks: MCP Integration for Compiler-as-a-Service

**Status**: Not started - Strategic Enhancement

## Phase 0: Documentation System (PRIORITY - Context7-like)

The documentation system is the foundation for LLM integration. Before any other MCP tools,
we need a complete, queryable documentation database that LLMs can search and retrieve.

### 0.1 Doc Comment Parser
- [ ] 0.1.1 Define doc comment syntax (`///` for items, `//!` for modules)
- [ ] 0.1.2 Parse doc comments attached to: functions, types, behaviors, impls, modules
- [ ] 0.1.3 Support markdown in doc comments (code blocks, lists, links)
- [ ] 0.1.4 Parse `@param`, `@returns`, `@throws`, `@example`, `@see` tags
- [ ] 0.1.5 Extract examples from ```` ```tml ```` code blocks as runnable snippets

### 0.2 Documentation Model
- [ ] 0.2.1 Define DocItem struct: { id, kind, name, path, signature, doc, examples[], see_also[] }
- [ ] 0.2.2 Define DocModule struct: { name, path, doc, items[], submodules[] }
- [ ] 0.2.3 Define DocIndex: full-text searchable index of all documentation
- [ ] 0.2.4 Generate stable IDs for all items (e.g., `core::slice::Slice::get`)
- [ ] 0.2.5 Track cross-references between items

### 0.3 Documentation Extraction
- [ ] 0.3.1 Extract docs from lib/core/** (all core library modules)
- [ ] 0.3.2 Extract docs from lib/std/** (all std library modules)
- [ ] 0.3.3 Extract docs from lib/test/** (test framework)
- [ ] 0.3.4 Extract docs from docs/*.md (language specification)
- [ ] 0.3.5 Generate signature strings from AST (e.g., `func get(this, index: U64) -> Maybe[T]`)

### 0.4 Documentation Database
- [ ] 0.4.1 Serialize doc index to JSON/MessagePack for fast loading
- [ ] 0.4.2 Implement full-text search with relevance scoring
- [ ] 0.4.3 Implement symbol search by path (exact and fuzzy)
- [ ] 0.4.4 Implement type-based search (find functions returning X)
- [ ] 0.4.5 Cache documentation database, invalidate on source change

### 0.5 MCP Documentation Tools (Context7-style)
- [ ] 0.5.1 `docs/search` - Full-text search across all documentation
  ```json
  { "query": "slice iteration", "limit": 10 }
  → { "results": [{ "id": "core::slice::Slice::iter", "score": 0.95, "snippet": "..." }] }
  ```
- [ ] 0.5.2 `docs/get` - Get full documentation for an item by ID
  ```json
  { "id": "core::slice::Slice::get" }
  → { "signature": "func get(...)", "doc": "...", "examples": [...] }
  ```
- [ ] 0.5.3 `docs/resolve` - Resolve a symbol name to its full path
  ```json
  { "symbol": "Vec", "context": "std::collections" }
  → { "id": "std::collections::Vec", "kind": "type" }
  ```
- [ ] 0.5.4 `docs/list` - List all items in a module
  ```json
  { "module": "core::slice" }
  → { "items": [{ "id": "...", "kind": "func", "name": "..." }] }
  ```
- [ ] 0.5.5 `docs/examples` - Get runnable examples for an item
  ```json
  { "id": "core::iter::Iterator::map" }
  → { "examples": [{ "code": "...", "description": "..." }] }
  ```
- [ ] 0.5.6 `docs/related` - Get related items (see_also, same module, similar signature)
  ```json
  { "id": "core::slice::Slice::get" }
  → { "related": [{ "id": "core::slice::Slice::get_unchecked", "reason": "unsafe variant" }] }
  ```

### 0.6 CLI Documentation Commands
- [ ] 0.6.1 `tml doc` - Generate HTML documentation site
- [ ] 0.6.2 `tml doc --json` - Export documentation as JSON
- [ ] 0.6.3 `tml doc --serve` - Serve documentation locally
- [ ] 0.6.4 `tml doc <symbol>` - Show docs for symbol in terminal

### 0.7 Standard Library Documentation
- [ ] 0.7.1 Document all public items in lib/core/src/
- [ ] 0.7.2 Document all public items in lib/std/src/
- [ ] 0.7.3 Add examples to all major APIs
- [ ] 0.7.4 Cross-reference related functions
- [ ] 0.7.5 Document error conditions and edge cases

## Phase 1: Diagnostic Infrastructure (Foundation)

### 1.1 Structured Diagnostics
- [ ] 1.1.1 Define DiagnosticCode enum with stable codes (E001-E999, W001-W999, H001-H999)
- [ ] 1.1.2 Create Diagnostic struct: { code, message, severity, span, notes[], fixits[] }
- [ ] 1.1.3 Create FixIt struct: { title, edits:[{file, range, text}], confidence }
- [ ] 1.1.4 Implement JSON serialization for all diagnostic types
- [ ] 1.1.5 Add explain/diagnostic endpoint (cause, rule, example, fixes)

### 1.2 Span and Source Location
- [ ] 1.2.1 Ensure all AST nodes have accurate spans
- [ ] 1.2.2 Implement span-to-line/column conversion
- [ ] 1.2.3 Add span merging for multi-location errors
- [ ] 1.2.4 Support UTF-8 column offsets

### 1.3 Fix-It Generation
- [ ] 1.3.1 Add fix-its for common type errors
- [ ] 1.3.2 Add fix-its for borrow checker errors
- [ ] 1.3.3 Add fix-its for unused imports/variables
- [ ] 1.3.4 Add fix-its for formatting issues
- [ ] 1.3.5 Calculate confidence scores for fixes

## Phase 2: Compiler Core Library

### 2.1 Library Separation
- [ ] 2.1.1 Create `tml_core` library target in CMake
- [ ] 2.1.2 Move parser, type checker, codegen to library
- [ ] 2.1.3 Define stable C++ API for library consumers
- [ ] 2.1.4 Ensure library is stateless (no global state)

### 2.2 File Overlay Support
- [ ] 2.2.1 Implement VirtualFileSystem abstraction
- [ ] 2.2.2 Support in-memory file content overlays
- [ ] 2.2.3 Track overlay changes for incremental updates
- [ ] 2.2.4 Handle file dependencies with overlays

### 2.3 Incremental Compilation
- [ ] 2.3.1 Implement AST caching per file
- [ ] 2.3.2 Track dependency graph between files
- [ ] 2.3.3 Invalidate cache on file change
- [ ] 2.3.4 Support partial re-typecheck

## Phase 3: Compilation Daemon

### 3.1 Daemon Architecture
- [ ] 3.1.1 Create daemon process with stdio IPC
- [ ] 3.1.2 Implement JSON-RPC 2.0 message handling
- [ ] 3.1.3 Add workspace management (open/close)
- [ ] 3.1.4 Implement graceful shutdown

### 3.2 Workspace State
- [ ] 3.2.1 Maintain file index per workspace
- [ ] 3.2.2 Track open files and their overlays
- [ ] 3.2.3 Maintain symbol index for quick lookup
- [ ] 3.2.4 Support multiple concurrent workspaces

### 3.3 Request Handling
- [ ] 3.3.1 Implement request queuing
- [ ] 3.3.2 Add request cancellation support
- [ ] 3.3.3 Implement progress reporting
- [ ] 3.3.4 Handle concurrent requests safely

## Phase 4: MCP Server Implementation

### 4.1 MCP Protocol Core
- [ ] 4.1.1 Implement MCP initialization handshake
- [ ] 4.1.2 Implement tool listing endpoint
- [ ] 4.1.3 Implement tool invocation handler
- [ ] 4.1.4 Add JSON Schema validation for all tools

### 4.2 Analysis Tools
- [ ] 4.2.1 `analyze/parse` - Parse file, return AST summary + diagnostics
- [ ] 4.2.2 `analyze/typecheck` - Type check, return type info + diagnostics
- [ ] 4.2.3 `analyze/lint` - Run lints, return findings + fix-its
- [ ] 4.2.4 `analyze/hover` - Get hover info at position
- [ ] 4.2.5 `analyze/definition` - Go to definition
- [ ] 4.2.6 `analyze/references` - Find all references

### 4.3 Editing Tools
- [ ] 4.3.1 `files/applyOverlay` - Apply in-memory changes
- [ ] 4.3.2 `files/removeOverlay` - Remove overlay, use disk
- [ ] 4.3.3 `format/document` - Format file (idempotent)
- [ ] 4.3.4 `format/range` - Format selection
- [ ] 4.3.5 `refactor/rename` - Rename symbol across files
- [ ] 4.3.6 `refactor/extract` - Extract function/variable
- [ ] 4.3.7 `refactor/inline` - Inline function/variable
- [ ] 4.3.8 `fix/apply` - Apply fix-it by ID

### 4.4 Build Tools
- [ ] 4.4.1 `build/compile` - Compile with options { target, optimize, features }
- [ ] 4.4.2 `build/check` - Type check only (fast)
- [ ] 4.4.3 `build/clean` - Clean build artifacts

### 4.5 Test Tools
- [ ] 4.5.1 `test/run` - Run tests with filter
- [ ] 4.5.2 `test/list` - List available tests
- [ ] 4.5.3 `test/coverage` - Run with coverage

### 4.6 Debug Tools
- [ ] 4.6.1 `debug/launch` - Start debug session (DAP wrapper)
- [ ] 4.6.2 `debug/attach` - Attach to running process
- [ ] 4.6.3 `debug/breakpoints` - Set/remove breakpoints
- [ ] 4.6.4 `debug/continue` - Continue execution
- [ ] 4.6.5 `debug/step` - Step in/over/out
- [ ] 4.6.6 `debug/evaluate` - Evaluate expression
- [ ] 4.6.7 `debug/variables` - Get variable values

### 4.7 Profile Tools
- [ ] 4.7.1 `profile/run` - Run with profiling
- [ ] 4.7.2 `profile/report` - Get profile report (normalized format)
- [ ] 4.7.3 `profile/hotspots` - Get hot functions
- [ ] 4.7.4 `profile/allocations` - Get allocation stats

## Phase 5: Workspace Tools

### 5.1 Workspace Management
- [ ] 5.1.1 `workspace/open` - Open project, return workspace ID
- [ ] 5.1.2 `workspace/close` - Close workspace
- [ ] 5.1.3 `workspace/files` - List project files
- [ ] 5.1.4 `workspace/symbols` - Search symbols

### 5.2 Project Understanding
- [ ] 5.2.1 `workspace/dependencies` - Get dependency graph
- [ ] 5.2.2 `workspace/modules` - List modules and exports
- [ ] 5.2.3 `workspace/diagnostics` - Get all project diagnostics

## Phase 6: CLI Thin Client

### 6.1 Daemon Integration
- [ ] 6.1.1 Add `--daemon` flag to use daemon
- [ ] 6.1.2 Implement daemon auto-start
- [ ] 6.1.3 Implement daemon discovery (port file)
- [ ] 6.1.4 Fallback to standalone mode

### 6.2 CLI Commands
- [ ] 6.2.1 `tml daemon start` - Start daemon
- [ ] 6.2.2 `tml daemon stop` - Stop daemon
- [ ] 6.2.3 `tml daemon status` - Check daemon status

## Phase 7: Canonical Formatter

### 7.1 Formatting Rules
- [ ] 7.1.1 Define canonical style (single official style)
- [ ] 7.1.2 Implement AST-based formatter
- [ ] 7.1.3 Ensure idempotency (format(format(x)) == format(x))
- [ ] 7.1.4 No configuration options (opinionated)

### 7.2 Format Integration
- [ ] 7.2.1 Add `tml fmt` command
- [ ] 7.2.2 Add format-on-save support in daemon
- [ ] 7.2.3 Add format check in CI (`tml fmt --check`)

## Phase 8: Security and Sandboxing

### 8.1 Capability-Based Access
- [ ] 8.1.1 Define allowed operations per tool
- [ ] 8.1.2 No arbitrary shell command execution
- [ ] 8.1.3 Whitelist allowed file operations

### 8.2 Execution Sandboxing
- [ ] 8.2.1 Sandbox test/debug execution
- [ ] 8.2.2 Set resource limits (timeout, memory)
- [ ] 8.2.3 Control working directory

## Validation

### Integration Tests
- [ ] V.1 MCP server responds to initialization
- [ ] V.2 All tools have valid JSON Schema
- [ ] V.3 Diagnostics include fix-its where applicable
- [ ] V.4 File overlays work without touching disk
- [ ] V.5 Daemon handles concurrent requests
- [ ] V.6 CLI falls back gracefully when daemon unavailable

### LLM Integration Tests
- [ ] V.7 Claude can successfully call analyze/typecheck
- [ ] V.8 Claude can apply fix-its via fix/apply
- [ ] V.9 Claude can run tests and parse results
- [ ] V.10 Claude can perform rename refactoring

### Performance Tests
- [ ] V.11 Parse/typecheck responds < 100ms for small files
- [ ] V.12 Incremental updates respond < 50ms
- [ ] V.13 Daemon handles 100+ concurrent requests

### Documentation
- [ ] V.14 Document MCP tool schemas
- [ ] V.15 Document daemon protocol
- [ ] V.16 Add integration guide for LLM frameworks
- [ ] V.17 Add examples for common workflows
