# Spec Delta: MCP Integration

## ADDED Requirements

### Requirement: Documentation System (Context7-like)
The compiler SHALL provide a complete, queryable documentation system that:
- Extracts documentation from source code doc comments (`///` and `//!`)
- Indexes all public items from core, std, and user libraries
- Provides full-text search with relevance scoring
- Returns structured documentation in machine-readable format
- Supports cross-references between related items

#### Scenario: Documentation Search
Given an indexed TML project with standard library
When the client calls docs/search with query="iterate over slice"
Then the response SHALL include:
  - results: ranked list of matching items
  - Each result SHALL have: id, score, name, kind, snippet
  - Results SHALL be sorted by relevance score descending

#### Scenario: Get Documentation by ID
Given a documented function `core::slice::Slice::get`
When the client calls docs/get with id="core::slice::Slice::get"
Then the response SHALL include:
  - id: "core::slice::Slice::get"
  - kind: "method"
  - signature: "func get(this, index: U64) -> Maybe[ref T]"
  - doc: full markdown documentation
  - params: [{ name: "index", type: "U64", doc: "..." }]
  - returns: { type: "Maybe[ref T]", doc: "..." }
  - examples: [{ code: "...", description: "..." }]
  - see_also: ["core::slice::Slice::get_unchecked", ...]

#### Scenario: List Module Contents
Given the module `core::iter`
When the client calls docs/list with module="core::iter"
Then the response SHALL include:
  - module: { name: "iter", doc: "..." }
  - items: list of all public items with { id, kind, name, signature, summary }
  - submodules: list of child modules

#### Scenario: Resolve Symbol
Given a symbol name "Vec" in context of "std::collections"
When the client calls docs/resolve with symbol="Vec"
Then the response SHALL include:
  - id: "std::collections::Vec"
  - kind: "type"
  - alternatives: [] (other matches if ambiguous)

#### Scenario: Get Examples
Given a function with documented examples
When the client calls docs/examples with id="core::iter::Iterator::map"
Then the response SHALL include:
  - examples: [{ code: "...", description: "...", runnable: true }]
  - Each example SHALL be syntactically valid TML code

#### Scenario: Get Related Items
Given a documented item
When the client calls docs/related with id="core::slice::Slice::get"
Then the response SHALL include:
  - related: [{ id, reason, relevance }]
  - Reasons include: "see_also", "same_module", "similar_signature", "implements"

### Requirement: Doc Comment Syntax
The compiler SHALL parse documentation comments with:
- `///` for item documentation (functions, types, fields)
- `//!` for module-level documentation
- Markdown formatting support (headers, lists, code blocks, links)
- Structured tags: `@param`, `@returns`, `@throws`, `@example`, `@see`

#### Scenario: Parse Doc Comment
Given a function with doc comment:
```tml
/// Returns the element at the given index.
///
/// @param index The zero-based index
/// @returns The element wrapped in Just, or Nothing if out of bounds
/// @example
/// ```tml
/// let slice = [1, 2, 3].as_slice()
/// assert_eq(slice.get(1), Just(2))
/// ```
func get(this, index: U64) -> Maybe[ref T]
```
When the compiler parses the file
Then the documentation model SHALL include:
  - summary: "Returns the element at the given index."
  - params: [{ name: "index", doc: "The zero-based index" }]
  - returns: "The element wrapped in Just, or Nothing if out of bounds"
  - examples: [{ code: "let slice = ..." }]

### Requirement: Documentation Database
The compiler SHALL maintain a searchable documentation database that:
- Indexes all public items from all loaded libraries
- Supports incremental updates when source changes
- Persists to disk for fast startup
- Provides sub-100ms search response times

#### Scenario: Incremental Update
Given an indexed project
When a source file is modified
Then only affected items SHALL be re-indexed
And unaffected items SHALL retain their documentation

#### Scenario: Database Persistence
Given a project that was previously indexed
When the MCP server starts
Then it SHALL load cached documentation
And respond to queries without full re-indexing

### Requirement: Structured Diagnostics
The compiler SHALL emit all diagnostics in a structured format containing:
- A stable diagnostic code (E001-E999 for errors, W001-W999 for warnings, H001-H999 for hints)
- A human-readable message
- A severity level (error, warning, hint, note)
- A source span with file, line, column, and byte offset
- Optional related spans for multi-location diagnostics
- Optional fix-it suggestions with confidence scores

#### Scenario: Diagnostic with Fix-It
Given a source file with an unused variable `x`
When the compiler analyzes the file
Then the diagnostic SHALL include:
  - code: "W001"
  - message: "unused variable: x"
  - severity: "warning"
  - span: { file, line, column, length }
  - fixits: [{ title: "remove unused variable", edits: [...], confidence: 0.95 }]

#### Scenario: Diagnostic Explain
Given a diagnostic code "E042"
When the client requests explanation
Then the server SHALL return:
  - cause: why this error occurs
  - rule: the language rule being violated
  - example: code showing the error and the fix
  - related_docs: links to documentation

### Requirement: File Overlay System
The compiler daemon SHALL support in-memory file overlays that:
- Override disk content for specified files
- Track changes incrementally
- Invalidate dependent caches when overlays change
- MUST NOT modify the filesystem

#### Scenario: Apply Overlay
Given an open workspace with file "src/main.tml"
When the client calls files/applyOverlay with new content
Then subsequent analyze operations SHALL use the overlay content
And the disk file SHALL remain unchanged

#### Scenario: Remove Overlay
Given an active overlay on "src/main.tml"
When the client calls files/removeOverlay
Then subsequent operations SHALL use the disk content

### Requirement: MCP Tool Interface
The MCP server SHALL expose tools following the MCP specification with:
- JSON Schema for all input parameters
- Structured output in predictable format
- Error responses with diagnostic details

#### Scenario: Tool Listing
Given a connected MCP client
When the client requests tools/list
Then the server SHALL return all available tools with their schemas

#### Scenario: Tool Invocation
Given a valid tool call with correct parameters
When the server executes the tool
Then the response SHALL match the tool's output schema
And errors SHALL include diagnostic details

### Requirement: Analysis Tools
The MCP server SHALL provide analysis tools that operate without side effects:
- `analyze/parse` - Parse and return AST summary
- `analyze/typecheck` - Type check and return type information
- `analyze/lint` - Run lints and return findings
- `analyze/hover` - Get hover information at position
- `analyze/definition` - Find definition of symbol
- `analyze/references` - Find all references to symbol

#### Scenario: Parse Analysis
Given a source file path
When the client calls analyze/parse
Then the response SHALL include:
  - success: boolean
  - ast_summary: { functions: [...], types: [...], imports: [...] }
  - diagnostics: [...]

#### Scenario: Type Check Analysis
Given a source file with type errors
When the client calls analyze/typecheck
Then the response SHALL include:
  - success: false
  - diagnostics: [{ code: "E...", message: "...", fixits: [...] }]

### Requirement: Refactoring Tools
The MCP server SHALL provide refactoring tools that:
- Operate on AST/IR level (not regex)
- Return edits in TextEdit format
- Support preview mode without applying changes

#### Scenario: Rename Symbol
Given a symbol "oldName" used in multiple files
When the client calls refactor/rename with newName="newName"
Then the response SHALL include edits for all occurrences
And the edits SHALL be semantically correct (not renaming string literals)

#### Scenario: Extract Function
Given a selected code range
When the client calls refactor/extract with type="function"
Then the response SHALL include:
  - new function definition
  - replacement call at original location
  - parameter inference for captured variables

### Requirement: Build Tools
The MCP server SHALL provide build tools with structured output:
- `build/compile` - Full compilation with artifacts
- `build/check` - Type check only (fast)
- `build/clean` - Clean build artifacts

#### Scenario: Compile Success
Given a valid project
When the client calls build/compile
Then the response SHALL include:
  - success: true
  - artifacts: [{ path, type, size }]
  - timings: { parse_ms, typecheck_ms, codegen_ms, link_ms }
  - cache_stats: { hits, misses }

#### Scenario: Compile Failure
Given a project with errors
When the client calls build/compile
Then the response SHALL include:
  - success: false
  - diagnostics: [...] with fix-its where applicable
  - partial_artifacts: [] (if any)

### Requirement: Test Tools
The MCP server SHALL provide test execution tools:
- `test/run` - Run tests with optional filter
- `test/list` - List available tests
- `test/coverage` - Run with coverage collection

#### Scenario: Run Tests
Given a project with tests
When the client calls test/run with filter="*"
Then the response SHALL include:
  - passed: number
  - failed: number
  - skipped: number
  - results: [{ name, status, duration_ms, output?, failure_message? }]

### Requirement: Debug Integration
The MCP server SHALL provide debug tools wrapping DAP:
- `debug/launch` - Start debug session
- `debug/breakpoints` - Manage breakpoints
- `debug/continue` - Continue execution
- `debug/step` - Step in/over/out
- `debug/evaluate` - Evaluate expression
- `debug/variables` - Get variable values

#### Scenario: Debug Session
Given a compiled binary with debug info
When the client calls debug/launch
Then a debug session SHALL start
And the response SHALL include session_id for subsequent commands

### Requirement: Profile Tools
The MCP server SHALL provide profiling tools with normalized output:
- `profile/run` - Execute with profiling
- `profile/report` - Get analysis report
- `profile/hotspots` - Get hot functions

#### Scenario: Profile Execution
Given a compiled binary
When the client calls profile/run
Then the response SHALL include:
  - duration_ms: total runtime
  - samples: number of samples collected
  - report_id: identifier for detailed analysis

#### Scenario: Hotspot Analysis
Given a completed profile session
When the client calls profile/hotspots
Then the response SHALL include:
  - functions: [{ name, self_time_pct, total_time_pct, call_count }]
  - sorted by self_time_pct descending

### Requirement: Canonical Formatting
The compiler SHALL enforce a single canonical formatting style:
- No configuration options
- Formatting MUST be idempotent: format(format(x)) == format(x)
- All code MUST be representable in canonical form

#### Scenario: Format Document
Given a source file with inconsistent formatting
When the client calls format/document
Then the response SHALL include edits to canonical form
And applying the edits twice SHALL produce no additional changes

### Requirement: Workspace Management
The MCP server SHALL support workspace lifecycle:
- `workspace/open` - Open project, return workspace ID
- `workspace/close` - Close workspace and free resources
- `workspace/files` - List project files
- `workspace/symbols` - Search workspace symbols

#### Scenario: Open Workspace
Given a project directory path
When the client calls workspace/open
Then the response SHALL include:
  - workspace_id: unique identifier
  - files: list of source files
  - diagnostics: initial project diagnostics

### Requirement: Security Constraints
The MCP server SHALL enforce security boundaries:
- No arbitrary shell command execution
- File operations limited to workspace scope
- Execution sandboxed with resource limits
- Capability-based access control

#### Scenario: Blocked Command
Given an MCP request attempting shell execution
When the server processes the request
Then the server SHALL reject with error
And no shell command SHALL be executed

#### Scenario: Sandbox Limits
Given a test or profile execution
When the execution exceeds timeout or memory limit
Then the server SHALL terminate execution
And return partial results with limit_exceeded flag

## MODIFIED Requirements

### Requirement: CLI Integration
The CLI SHALL support daemon mode for faster operation:
- `tml --daemon` uses running daemon
- Auto-starts daemon if not running
- Falls back to standalone if daemon fails

#### Scenario: Daemon Mode Build
Given a running daemon
When the user runs `tml build --daemon`
Then the command SHALL communicate with daemon
And build results SHALL match standalone mode
