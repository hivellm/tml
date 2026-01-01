# Spec Delta: Documentation Generation System (`tml doc`)

## ADDED Requirements

### Requirement: Doc Comment Token Preservation
The lexer SHALL recognize and preserve documentation comments:
- `///` prefix indicates item documentation
- `//!` prefix indicates module documentation
- Consecutive doc comment lines SHALL be merged into a single token
- Doc comment content SHALL preserve markdown formatting

#### Scenario: Lex Item Doc Comment
Given source code:
```tml
/// Returns the sum of two numbers.
///
/// # Examples
/// ```tml
/// assert_eq(add(1, 2), 3)
/// ```
func add(a: I32, b: I32) -> I32
```
When the lexer tokenizes the file
Then it SHALL produce a `DocComment` token before the `func` keyword
And the token content SHALL preserve all lines and formatting

#### Scenario: Lex Module Doc Comment
Given source code starting with:
```tml
//! This module provides arithmetic operations.
//!
//! # Overview
//! Basic math functions for TML.
```
When the lexer tokenizes the file
Then it SHALL produce a `ModuleDocComment` token
And the token SHALL appear before any item tokens

#### Scenario: Regular Comment Not Captured
Given source code:
```tml
// This is a regular comment
//// This has 4 slashes
func example()
```
When the lexer tokenizes the file
Then no `DocComment` token SHALL be produced
And `////` SHALL be treated as a regular comment

### Requirement: AST Doc Attachment
The parser SHALL attach documentation to AST nodes:
- Each item node SHALL have an optional `doc` field
- Module nodes SHALL have a `module_docs` field
- Doc comments SHALL attach to the immediately following item

#### Scenario: Function Documentation
Given a function with preceding doc comment
When the parser builds the AST
Then the `FuncDecl` node SHALL have `doc` containing the comment text

#### Scenario: Type Documentation
Given a type definition with preceding doc comment
When the parser builds the AST
Then the `TypeDecl` node SHALL have `doc` containing the comment text
And each documented field SHALL have its own `doc` field

#### Scenario: Behavior Documentation
Given a behavior with preceding doc comment
When the parser builds the AST
Then the `BehaviorDecl` node SHALL have `doc` containing the comment text
And each documented method signature SHALL have its own `doc` field

#### Scenario: Module Documentation
Given a file starting with `//!` comments
When the parser builds the AST
Then the `Module` node SHALL have `module_docs` containing all `//!` lines

### Requirement: Doc Comment Syntax
The documentation system SHALL support structured doc comments:
- Markdown formatting (headers, lists, code blocks, links, tables)
- `@param name description` - Parameter documentation
- `@returns description` - Return value documentation
- `@throws ErrorType description` - Error documentation
- `@example` - Code example block
- `@see symbol` - Cross-reference to another item
- `@since version` - Version when added
- `@deprecated message` - Deprecation notice

#### Scenario: Parse @param Tag
Given doc comment:
```
/// Adds two numbers.
/// @param a The first operand
/// @param b The second operand
/// @returns The sum of a and b
```
When the doc parser processes this comment
Then it SHALL extract:
  - summary: "Adds two numbers."
  - params: [{ name: "a", doc: "The first operand" }, { name: "b", doc: "The second operand" }]
  - returns: "The sum of a and b"

#### Scenario: Parse Code Example
Given doc comment with code block:
```
/// # Examples
/// ```tml
/// let x = add(1, 2)
/// assert_eq(x, 3)
/// ```
```
When the doc parser processes this comment
Then it SHALL extract the code block as an example
And the example SHALL be marked as language "tml"

#### Scenario: Parse @see Cross-Reference
Given doc comment:
```
/// @see sub For subtraction
/// @see mul For multiplication
```
When the doc parser processes this comment
Then it SHALL extract see_also: ["sub", "mul"]

### Requirement: Documentation Model
The documentation system SHALL produce a structured documentation model:
- Each documented item becomes a `DocItem`
- Items are organized into `DocModule` hierarchy
- All items have stable, unique IDs

#### Scenario: DocItem Structure
Given a documented function `func get(this, index: U64) -> Maybe[ref T]` in module `core::slice::Slice`
When documentation is extracted
Then the DocItem SHALL have:
  - id: "core::slice::Slice::get"
  - name: "get"
  - kind: "method"
  - path: "core::slice::Slice"
  - signature: "func get(this, index: U64) -> Maybe[ref T]"
  - doc: full markdown text
  - summary: first paragraph only

#### Scenario: DocModule Structure
Given the module `core::iter` with submodules and items
When documentation is extracted
Then the DocModule SHALL have:
  - name: "iter"
  - path: "core::iter"
  - doc: module-level documentation
  - items: list of DocItems in this module
  - submodules: list of child DocModules

### Requirement: Signature Generation
The documentation system SHALL generate human-readable signatures:
- Include parameter names and types
- Include return type
- Include generic parameters
- Include where clauses

#### Scenario: Function Signature
Given: `pub func map[U](this, f: func(T) -> U) -> Iterator[U]`
When signature is generated
Then result SHALL be: `pub func map[U](this, f: func(T) -> U) -> Iterator[U]`

#### Scenario: Generic Type Signature
Given: `pub type Vec[T] { ... }`
When signature is generated
Then result SHALL be: `pub type Vec[T]`

### Requirement: JSON Output
The `tml doc --json` command SHALL produce machine-readable documentation:
- Complete DocIndex serialized to JSON
- Stable schema for MCP consumption
- Per-module JSON files for lazy loading

#### Scenario: JSON Generation
Given a documented TML project
When the user runs `tml doc --json`
Then `target/doc/docs.json` SHALL be created
And the JSON SHALL contain all DocModules and DocItems
And the JSON SHALL be valid according to the schema

#### Scenario: JSON Schema
The JSON output SHALL follow this structure:
```json
{
  "version": "1.0",
  "generated": "2024-01-01T00:00:00Z",
  "modules": [
    {
      "id": "core",
      "name": "core",
      "doc": "...",
      "items": [
        {
          "id": "core::clone::Duplicate",
          "name": "Duplicate",
          "kind": "behavior",
          "signature": "pub behavior Duplicate",
          "doc": "...",
          "summary": "...",
          "params": [],
          "examples": [],
          "see_also": []
        }
      ],
      "submodules": ["core::clone", "core::cmp", ...]
    }
  ]
}
```

### Requirement: HTML Output
The `tml doc` command SHALL generate a static documentation website:
- Index page with module list
- Per-module pages with item list
- Per-item detail pages
- Client-side search functionality
- Cross-reference links

#### Scenario: HTML Generation
Given a documented TML project
When the user runs `tml doc`
Then `target/doc/index.html` SHALL be created
And module pages SHALL be created at `target/doc/{module}/index.html`
And the site SHALL be viewable in a web browser

#### Scenario: HTML Search
Given generated HTML documentation
When the user types in the search box
Then matching items SHALL be displayed
And clicking a result SHALL navigate to the item

#### Scenario: Cross-Reference Links
Given a doc comment with `@see other_func`
When HTML is generated
Then the reference SHALL become a clickable link to `other_func`

### Requirement: Terminal Output
The `tml doc <symbol>` command SHALL display documentation in terminal:
- Formatted with ANSI colors
- Syntax highlighted code examples
- Readable at standard terminal width

#### Scenario: Terminal Lookup
Given a documented function `core::slice::Slice::get`
When the user runs `tml doc Slice::get`
Then the terminal SHALL display:
  - Function signature (highlighted)
  - Full documentation text
  - Examples (syntax highlighted)
  - See also references

#### Scenario: Ambiguous Symbol
Given multiple items named "get" in different modules
When the user runs `tml doc get`
Then the terminal SHALL list all matches
And prompt user to specify full path

### Requirement: Documentation Server
The `tml doc --serve` command SHALL start a local HTTP server:
- Serve generated HTML documentation
- Auto-reload on source changes (optional)
- Default port 8000

#### Scenario: Start Server
Given generated documentation
When the user runs `tml doc --serve`
Then an HTTP server SHALL start on port 8000
And navigating to http://localhost:8000 SHALL show the docs

#### Scenario: Custom Port
When the user runs `tml doc --serve --port 3000`
Then the server SHALL start on port 3000

### Requirement: Documentation Caching
The documentation system SHALL cache extracted documentation:
- Cache stored in `build/doc-cache.json`
- Invalidate on source file modification
- Incremental updates for changed files only

#### Scenario: Cache Hit
Given documentation was previously generated
And no source files have changed
When the user runs `tml doc`
Then cached documentation SHALL be used
And generation time SHALL be < 1 second

#### Scenario: Cache Invalidation
Given documentation was previously generated
When a source file is modified
Then the cache for that file SHALL be invalidated
And only that file SHALL be re-extracted

### Requirement: Standard Library Documentation
All public items in lib/core, lib/std, and lib/test SHALL be documented:
- Each public function MUST have a `///` doc comment
- Each public type MUST have a `///` doc comment
- Each public behavior MUST have a `///` doc comment
- Documentation MUST include at least a summary sentence

#### Scenario: Documentation Coverage
Given the TML standard library
When `tml doc` is run with coverage check
Then 100% of public items SHALL have documentation
And a warning SHALL be emitted for any undocumented public item

## MODIFIED Requirements

### Requirement: Lexer Comment Handling
The lexer SHALL distinguish between comment types:
- `//` - Regular comment (discarded)
- `///` - Item doc comment (preserved as token)
- `//!` - Module doc comment (preserved as token)
- `/* */` - Block comment (discarded)
- `/** */` - Block doc comment (preserved as token) [future]

#### Scenario: Comment Type Detection
Given mixed comment types in source
When the lexer processes the file
Then only `///` and `//!` lines SHALL produce tokens
And regular comments SHALL be skipped as before

### Requirement: CLI Command Registry
The CLI dispatcher SHALL support the `doc` command:
- `tml doc` registered alongside build, test, format
- Help text explains all doc options

#### Scenario: Help Text
When the user runs `tml doc --help`
Then usage information SHALL be displayed
And all options SHALL be documented
