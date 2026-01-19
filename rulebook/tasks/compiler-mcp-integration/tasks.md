# Tasks: MCP Integration for Compiler-as-a-Service

**Status**: In Progress (35%) - Phase 4 Enhanced

## Phase 4: MCP Server Implementation (ENHANCED)

### 4.1 MCP Protocol Core
- [x] 4.1.1 Implement MCP initialization handshake
- [x] 4.1.2 Implement tool listing endpoint
- [x] 4.1.3 Implement tool invocation handler
- [ ] 4.1.4 Add JSON Schema validation for all tools

### 4.2 Compiler Tools (Implemented)
- [x] 4.2.1 `compile` - Compile TML file (IR generation only)
- [x] 4.2.2 `check` - Type check without compiling
- [x] 4.2.3 `emit-ir` - Emit LLVM IR with chunking support (function filter, offset, limit)
- [x] 4.2.4 `emit-mir` - Emit MIR (Mid-level IR)
- [x] 4.2.5 `test` - Run TML tests with filter
- [x] 4.2.6 `docs/search` - Search documentation
- [x] 4.2.7 `run` - Build and execute TML file, return output
- [x] 4.2.8 `build` - Full build with options (output, optimize, release, crate_type)

### 4.3 Claude Code Integration
- [x] 4.3.1 Create `.mcp.json` configuration file
- [x] 4.3.2 Configure `enabledMcpjsonServers` in settings
- [x] 4.3.3 Verify MCP server connects successfully
- [x] 4.3.4 Test all tools via Claude Code

### 4.4 Implementation Files
- [x] `compiler/include/mcp/mcp_types.hpp` - Protocol types
- [x] `compiler/include/mcp/mcp_server.hpp` - Server interface
- [x] `compiler/src/mcp/mcp_types.cpp` - Type implementations
- [x] `compiler/src/mcp/mcp_server.cpp` - Server with stdio transport
- [x] `compiler/src/mcp/mcp_tools.cpp` - Tool handlers
- [x] `compiler/src/cli/commands/cmd_mcp.cpp` - CLI command

## Phase 0: Documentation System (PENDING)

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
- [ ] 0.3.5 Generate signature strings from AST

### 0.4 Documentation Database
- [ ] 0.4.1 Serialize doc index to JSON/MessagePack for fast loading
- [ ] 0.4.2 Implement full-text search with relevance scoring
- [ ] 0.4.3 Implement symbol search by path (exact and fuzzy)
- [ ] 0.4.4 Implement type-based search (find functions returning X)
- [ ] 0.4.5 Cache documentation database, invalidate on source change

### 0.5 MCP Documentation Tools (Context7-style)
- [ ] 0.5.1 `docs/get` - Get full documentation for an item by ID
- [ ] 0.5.2 `docs/resolve` - Resolve a symbol name to its full path
- [ ] 0.5.3 `docs/list` - List all items in a module
- [ ] 0.5.4 `docs/examples` - Get runnable examples for an item
- [ ] 0.5.5 `docs/related` - Get related items

## Phase 1: Diagnostic Infrastructure (PENDING)

### 1.1 Structured Diagnostics
- [ ] 1.1.1 Define DiagnosticCode enum with stable codes
- [ ] 1.1.2 Create Diagnostic struct with fix-its
- [ ] 1.1.3 Implement JSON serialization
- [ ] 1.1.4 Add explain/diagnostic endpoint

### 1.2 Fix-It Generation
- [ ] 1.2.1 Add fix-its for common type errors
- [ ] 1.2.2 Add fix-its for borrow checker errors
- [ ] 1.2.3 Calculate confidence scores for fixes

## Phase 2: Analysis Tools (PENDING)

### 2.1 LSP-like Tools
- [ ] 2.1.1 `analyze/hover` - Get hover info at position
- [ ] 2.1.2 `analyze/definition` - Go to definition
- [ ] 2.1.3 `analyze/references` - Find all references
- [ ] 2.1.4 `analyze/symbols` - Document symbols

## Phase 3: Editing Tools (PENDING)

### 3.1 File Overlays
- [ ] 3.1.1 `files/applyOverlay` - Apply in-memory changes
- [ ] 3.1.2 `files/removeOverlay` - Remove overlay

### 3.2 Refactoring
- [ ] 3.2.1 `refactor/rename` - Rename symbol across files
- [ ] 3.2.2 `refactor/extract` - Extract function/variable
- [ ] 3.2.3 `fix/apply` - Apply fix-it by ID

## Phase 5: Workspace Tools (PENDING)

### 5.1 Workspace Management
- [ ] 5.1.1 `workspace/open` - Open project
- [ ] 5.1.2 `workspace/files` - List project files
- [ ] 5.1.3 `workspace/symbols` - Search symbols
- [ ] 5.1.4 `workspace/dependencies` - Get dependency graph

## Validation

### Integration Tests
- [x] V.1 MCP server responds to initialization
- [x] V.2 All tools return valid JSON responses
- [ ] V.3 Diagnostics include fix-its where applicable
- [ ] V.4 File overlays work without touching disk

### LLM Integration Tests
- [x] V.5 Claude can successfully call compile
- [x] V.6 Claude can successfully call check
- [x] V.7 Claude can successfully call emit-ir
- [x] V.8 Claude can successfully call emit-mir
- [x] V.9 Claude can successfully call test
- [x] V.10 Claude can successfully call docs/search
- [x] V.11 emit-ir chunking works (offset, limit parameters)
- [x] V.12 emit-ir function filter works
- [ ] V.13 Claude can successfully call run (needs server restart)
- [ ] V.14 Claude can successfully call build (needs server restart)
