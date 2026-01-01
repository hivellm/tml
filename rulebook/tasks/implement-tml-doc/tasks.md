# Tasks: Implement `tml doc` - Documentation Generation System

**Status**: Not started - High Priority

**Prerequisites**: None (can start immediately)

**Output**:
- HTML documentation site (like docs.rs)
- JSON documentation database (for MCP)
- `tml doc` CLI command

---

## Phase 1: Lexer - Preserve Doc Comments

### 1.1 Add Doc Comment Token Kinds
- [ ] 1.1.1 Add `TokenKind::DocComment` to `compiler/include/lexer/token.hpp`
- [ ] 1.1.2 Add `TokenKind::ModuleDocComment` for `//!` comments
- [ ] 1.1.3 Add `doc_content` field to Token struct (or use lexeme)
- [ ] 1.1.4 Update `token_kind_to_string()` for new token kinds

### 1.2 Modify Lexer to Capture Doc Comments
- [ ] 1.2.1 In `lexer_core.cpp`, detect `///` at start of comment
- [ ] 1.2.2 Detect `//!` for module-level docs
- [ ] 1.2.3 Collect consecutive doc comment lines into single token
- [ ] 1.2.4 Preserve markdown formatting and whitespace
- [ ] 1.2.5 Handle edge cases: `////` (not a doc comment), empty `///`

### 1.3 Lexer Tests
- [ ] 1.3.1 Test single-line doc comment `/// text`
- [ ] 1.3.2 Test multi-line doc comments (consecutive `///`)
- [ ] 1.3.3 Test module doc comment `//! text`
- [ ] 1.3.4 Test mixed doc and regular comments
- [ ] 1.3.5 Test `////` is NOT a doc comment (4+ slashes)

---

## Phase 2: Parser/AST - Attach Docs to Items

### 2.1 AST Structure Changes
- [ ] 2.1.1 Add `std::optional<std::string> doc` to `FuncDecl` in `ast.hpp`
- [ ] 2.1.2 Add `doc` field to `TypeDecl` (structs and enums)
- [ ] 2.1.3 Add `doc` field to `BehaviorDecl`
- [ ] 2.1.4 Add `doc` field to `ImplBlock`
- [ ] 2.1.5 Add `doc` field to `FieldDecl` (struct fields)
- [ ] 2.1.6 Add `doc` field to `EnumVariant`
- [ ] 2.1.7 Add `std::vector<std::string> module_docs` to `Module`
- [ ] 2.1.8 Add `doc` field to `ConstDecl` and `StaticDecl`

### 2.2 Parser Changes - Collect Leading Docs
- [ ] 2.2.1 Create `collect_doc_comments()` helper in parser
- [ ] 2.2.2 Before parsing `func`, collect preceding doc comments
- [ ] 2.2.3 Before parsing `type`, collect preceding doc comments
- [ ] 2.2.4 Before parsing `behavior`, collect preceding doc comments
- [ ] 2.2.5 Before parsing `impl`, collect preceding doc comments
- [ ] 2.2.6 Before parsing struct fields, collect preceding doc comments
- [ ] 2.2.7 Before parsing enum variants, collect preceding doc comments
- [ ] 2.2.8 At module start, collect `//!` comments into `module_docs`

### 2.3 Parser Tests
- [ ] 2.3.1 Test doc attached to function
- [ ] 2.3.2 Test doc attached to type (struct)
- [ ] 2.3.3 Test doc attached to type (enum)
- [ ] 2.3.4 Test doc attached to behavior
- [ ] 2.3.5 Test doc attached to impl block
- [ ] 2.3.6 Test doc attached to struct field
- [ ] 2.3.7 Test doc attached to enum variant
- [ ] 2.3.8 Test module-level docs (`//!`)
- [ ] 2.3.9 Test item without doc (should be nullopt)

---

## Phase 3: Documentation Model

### 3.1 Core Data Structures
- [ ] 3.1.1 Create `compiler/include/doc/doc_model.hpp`
- [ ] 3.1.2 Define `DocItem` struct:
  ```cpp
  struct DocItem {
      std::string id;           // "core::slice::Slice::get"
      std::string name;         // "get"
      std::string kind;         // "func", "type", "behavior", "field"
      std::string path;         // "core::slice::Slice"
      std::string signature;    // "func get(this, index: U64) -> Maybe[ref T]"
      std::string doc;          // Full markdown documentation
      std::string summary;      // First paragraph only
      std::vector<DocParam> params;
      std::optional<DocReturn> returns;
      std::vector<DocExample> examples;
      std::vector<std::string> see_also;
      SourceSpan span;
  };
  ```
- [ ] 3.1.3 Define `DocParam` struct: { name, type, doc }
- [ ] 3.1.4 Define `DocReturn` struct: { type, doc }
- [ ] 3.1.5 Define `DocExample` struct: { code, description, line }
- [ ] 3.1.6 Define `DocModule` struct:
  ```cpp
  struct DocModule {
      std::string name;
      std::string path;         // "core::slice"
      std::string doc;          // Module-level documentation
      std::vector<DocItem> items;
      std::vector<DocModule> submodules;
  };
  ```
- [ ] 3.1.7 Define `DocIndex` struct for full documentation database

### 3.2 Doc Comment Parsing
- [ ] 3.2.1 Create `compiler/src/doc/doc_parser.cpp`
- [ ] 3.2.2 Parse `@param name description` tags
- [ ] 3.2.3 Parse `@returns description` tag
- [ ] 3.2.4 Parse `@throws ErrorType description` tag
- [ ] 3.2.5 Parse `@example` tag (block until next tag or end)
- [ ] 3.2.6 Parse `@see symbol` tag for cross-references
- [ ] 3.2.7 Parse `@since version` tag
- [ ] 3.2.8 Parse `@deprecated message` tag
- [ ] 3.2.9 Extract code blocks (``` tml ... ```) as examples
- [ ] 3.2.10 Extract first paragraph as summary

### 3.3 Signature Generation
- [ ] 3.3.1 Create `generate_signature()` for functions
- [ ] 3.3.2 Generate signature for methods (with `this`)
- [ ] 3.3.3 Generate signature for types (with generics)
- [ ] 3.3.4 Generate signature for behaviors
- [ ] 3.3.5 Generate signature for type aliases
- [ ] 3.3.6 Handle generic parameters `[T, U]`
- [ ] 3.3.7 Handle where clauses

---

## Phase 4: Documentation Extractor

### 4.1 AST to DocModel Conversion
- [ ] 4.1.1 Create `compiler/src/doc/doc_extractor.cpp`
- [ ] 4.1.2 Implement `extract_module(Module&) -> DocModule`
- [ ] 4.1.3 Implement `extract_function(FuncDecl&) -> DocItem`
- [ ] 4.1.4 Implement `extract_type(TypeDecl&) -> DocItem`
- [ ] 4.1.5 Implement `extract_behavior(BehaviorDecl&) -> DocItem`
- [ ] 4.1.6 Implement `extract_impl(ImplBlock&) -> std::vector<DocItem>`
- [ ] 4.1.7 Generate stable IDs: `module::Type::method`
- [ ] 4.1.8 Resolve cross-references between items

### 4.2 Multi-File Extraction
- [ ] 4.2.1 Extract from single file
- [ ] 4.2.2 Extract from directory (lib/core/src/**)
- [ ] 4.2.3 Merge submodules into parent modules
- [ ] 4.2.4 Handle re-exports (`pub use`)
- [ ] 4.2.5 Track visibility (`pub`, `pub(crate)`, private)

### 4.3 Standard Library Extraction
- [ ] 4.3.1 Extract all of `lib/core/src/`
- [ ] 4.3.2 Extract all of `lib/std/src/`
- [ ] 4.3.3 Extract all of `lib/test/src/`
- [ ] 4.3.4 Build complete DocIndex

---

## Phase 5: Documentation Index & Search

### 5.1 Index Building
- [ ] 5.1.1 Create `compiler/src/doc/doc_index.cpp`
- [ ] 5.1.2 Build ID → DocItem lookup map
- [ ] 5.1.3 Build name → DocItem[] lookup (for ambiguous names)
- [ ] 5.1.4 Build module tree structure
- [ ] 5.1.5 Build full-text search index

### 5.2 Search Implementation
- [ ] 5.2.1 Implement exact ID lookup
- [ ] 5.2.2 Implement fuzzy name search
- [ ] 5.2.3 Implement full-text search with relevance scoring
- [ ] 5.2.4 Implement type-based search ("functions returning Maybe")
- [ ] 5.2.5 Implement module listing

### 5.3 Serialization
- [ ] 5.3.1 Serialize DocIndex to JSON
- [ ] 5.3.2 Deserialize DocIndex from JSON
- [ ] 5.3.3 Cache to `build/doc-cache.json`
- [ ] 5.3.4 Invalidate cache on source file change
- [ ] 5.3.5 Incremental update (re-index only changed files)

---

## Phase 6: Output Generators

### 6.1 JSON Generator (for MCP)
- [ ] 6.1.1 Create `compiler/src/doc/json_generator.cpp`
- [ ] 6.1.2 Generate `docs.json` with full DocIndex
- [ ] 6.1.3 Generate per-module JSON files
- [ ] 6.1.4 Generate search index JSON
- [ ] 6.1.5 Schema documentation for JSON format

### 6.2 HTML Generator (Rust-style)
- [ ] 6.2.1 Create `compiler/src/doc/html_generator.cpp`
- [ ] 6.2.2 Create HTML template system (or use embedded strings)
- [ ] 6.2.3 Generate index.html (crate root)
- [ ] 6.2.4 Generate module pages (one per module)
- [ ] 6.2.5 Generate item pages (function, type, behavior detail)
- [ ] 6.2.6 Generate sidebar navigation
- [ ] 6.2.7 Generate breadcrumb navigation
- [ ] 6.2.8 Render markdown to HTML
- [ ] 6.2.9 Syntax highlight code blocks
- [ ] 6.2.10 Generate search.js for client-side search
- [ ] 6.2.11 Cross-reference links between items
- [ ] 6.2.12 Copy static assets (CSS, JS, fonts)

### 6.3 HTML Styling
- [ ] 6.3.1 Create `compiler/runtime/doc_template/style.css`
- [ ] 6.3.2 Light theme (default)
- [ ] 6.3.3 Dark theme support
- [ ] 6.3.4 Responsive design (mobile-friendly)
- [ ] 6.3.5 Print stylesheet
- [ ] 6.3.6 Code syntax highlighting CSS

### 6.4 Markdown Generator
- [ ] 6.4.1 Create `compiler/src/doc/markdown_generator.cpp`
- [ ] 6.4.2 Generate single markdown file per module
- [ ] 6.4.3 Generate combined API reference markdown
- [ ] 6.4.4 GitHub-compatible markdown

### 6.5 Terminal Output
- [ ] 6.5.1 Create `compiler/src/doc/terminal_generator.cpp`
- [ ] 6.5.2 Format docs for terminal display
- [ ] 6.5.3 Syntax highlighting with ANSI colors
- [ ] 6.5.4 Pagination for long docs
- [ ] 6.5.5 Width-aware line wrapping

---

## Phase 7: CLI Command

### 7.1 Command Implementation
- [ ] 7.1.1 Create `compiler/src/cli/cmd_doc.cpp`
- [ ] 7.1.2 Register `doc` command in dispatcher
- [ ] 7.1.3 Parse command-line options

### 7.2 Command Options
- [ ] 7.2.1 `tml doc` - Generate HTML docs to `./target/doc/`
- [ ] 7.2.2 `tml doc --json` - Generate JSON to `./target/doc/docs.json`
- [ ] 7.2.3 `tml doc --markdown` - Generate markdown docs
- [ ] 7.2.4 `tml doc --open` - Open in browser after generation
- [ ] 7.2.5 `tml doc --serve` - Start local server on port 8000
- [ ] 7.2.6 `tml doc --serve --port <N>` - Custom port
- [ ] 7.2.7 `tml doc <symbol>` - Show docs in terminal
- [ ] 7.2.8 `tml doc --no-deps` - Skip dependencies, document only current crate
- [ ] 7.2.9 `tml doc --include-private` - Include private items

### 7.3 Error Handling
- [ ] 7.3.1 Report missing docs warning
- [ ] 7.3.2 Report broken cross-references
- [ ] 7.3.3 Report invalid doc comment syntax
- [ ] 7.3.4 Continue on non-fatal errors

---

## Phase 8: Standard Library Documentation

### 8.1 Core Library (`lib/core/`)
- [ ] 8.1.1 Verify all public items have `///` docs
- [ ] 8.1.2 Add missing docs to `clone.tml`
- [ ] 8.1.3 Add missing docs to `cmp.tml`
- [ ] 8.1.4 Add missing docs to `ops.tml`
- [ ] 8.1.5 Add missing docs to `fmt.tml`
- [ ] 8.1.6 Add missing docs to `convert.tml`
- [ ] 8.1.7 Add missing docs to `iter/mod.tml`
- [ ] 8.1.8 Add missing docs to `option.tml`
- [ ] 8.1.9 Add missing docs to `result.tml`
- [ ] 8.1.10 Add missing docs to all other core modules
- [ ] 8.1.11 Add examples to major APIs
- [ ] 8.1.12 Add `@see` cross-references

### 8.2 Standard Library (`lib/std/`)
- [ ] 8.2.1 Verify all public items have docs
- [ ] 8.2.2 Add missing docs to `collections/mod.tml`
- [ ] 8.2.3 Add missing docs to `file/mod.tml`
- [ ] 8.2.4 Add missing docs to `iter/mod.tml`
- [ ] 8.2.5 Add examples for file I/O
- [ ] 8.2.6 Add examples for collections

### 8.3 Test Library (`lib/test/`)
- [ ] 8.3.1 Document test framework usage
- [ ] 8.3.2 Document assertion functions
- [ ] 8.3.3 Document test attributes
- [ ] 8.3.4 Add examples for writing tests

---

## Phase 9: Integration & Polish

### 9.1 Build Integration
- [ ] 9.1.1 Add doc module to CMakeLists.txt
- [ ] 9.1.2 Build doc command with compiler
- [ ] 9.1.3 Include doc templates in distribution

### 9.2 CI Integration
- [ ] 9.2.1 Add `tml doc --check` for CI (verify docs build)
- [ ] 9.2.2 Add doc generation to CI pipeline
- [ ] 9.2.3 Deploy docs to GitHub Pages (optional)

### 9.3 MCP Integration Prep
- [ ] 9.3.1 Verify JSON format matches MCP spec requirements
- [ ] 9.3.2 Test JSON loading performance
- [ ] 9.3.3 Document JSON schema for MCP consumers

---

## Validation

### Functional Tests
- [ ] V.1 `tml doc` generates HTML without errors
- [ ] V.2 `tml doc --json` generates valid JSON
- [ ] V.3 `tml doc Slice::get` shows terminal docs
- [ ] V.4 `tml doc --serve` starts HTTP server
- [ ] V.5 HTML docs have working search
- [ ] V.6 Cross-reference links work

### Content Tests
- [ ] V.7 All lib/core public items documented
- [ ] V.8 All lib/std public items documented
- [ ] V.9 Examples are syntactically valid TML
- [ ] V.10 No broken cross-references

### Performance Tests
- [ ] V.11 Full doc generation < 10 seconds
- [ ] V.12 JSON loading < 100ms
- [ ] V.13 Search response < 50ms

### Compatibility Tests
- [ ] V.14 HTML works in Chrome, Firefox, Safari
- [ ] V.15 JSON parseable by standard tools
- [ ] V.16 Terminal output works on Windows, Linux, macOS
