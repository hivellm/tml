# Proposal: Implement `tml doc` - Documentation Generation System

## Why

TML has extensive documentation already written in source files using `///` and `//!` comments (50+ modules in lib/core, lib/std, lib/test), but this documentation is **completely discarded** during lexing. The lexer's `skip_line_comment()` and `skip_block_comment()` functions treat doc comments as regular comments.

**Current state:**
- Doc comments (`///`, `//!`) exist in ~50 library modules with markdown, examples, tables
- Lexer discards all comments - no `TokenKind` for doc comments
- AST nodes have no fields for documentation
- No `tml doc` command exists
- Documentation is only readable by opening source files

**Goals:**
1. Preserve doc comments through lexer → parser → AST
2. Generate Rust-style documentation (HTML site, JSON export)
3. Create machine-readable documentation database for MCP/LLM integration
4. Enable `tml doc <symbol>` for terminal lookup

**Why Rust-style:**
- Proven format that works well for systems languages
- Markdown-based, familiar to developers
- Searchable, cross-referenced, example-rich
- Industry standard that LLMs understand well

## What Changes

### 1. Lexer Changes (`compiler/src/lexer/`)
- Add `TokenKind::DocComment` and `TokenKind::ModuleDocComment`
- Modify `skip_line_comment()` to detect and preserve `///` and `//!`
- Store doc comment content in token

### 2. Parser/AST Changes (`compiler/include/parser/ast.hpp`)
- Add `std::optional<std::string> doc` field to all item nodes:
  - `FuncDecl`, `TypeDecl`, `BehaviorDecl`, `ImplBlock`
  - `FieldDecl`, `VariantDecl`, `MethodDecl`
- Add `std::vector<std::string> module_docs` to `Module`
- Parse and attach doc comments to following items

### 3. Documentation Model (`compiler/src/doc/`)
- New module for documentation extraction and generation
- `DocItem` struct with: id, kind, name, path, signature, doc, examples, see_also
- `DocModule` struct with: name, path, doc, items, submodules
- `DocIndex` for searchable documentation database

### 4. CLI Command (`compiler/src/cli/cmd_doc.cpp`)
- `tml doc` - Generate HTML documentation site
- `tml doc --json` - Export as JSON for MCP
- `tml doc --serve` - Local documentation server
- `tml doc <symbol>` - Terminal lookup

### 5. Output Formats
- **HTML**: Rust-style documentation site with search, cross-references
- **JSON**: Machine-readable format for MCP integration
- **Markdown**: For inclusion in other docs
- **Terminal**: Colored output for `tml doc <symbol>`

## Impact

### Files to Create
- `compiler/src/doc/doc_model.hpp` - Documentation data structures
- `compiler/src/doc/doc_extractor.cpp` - Extract docs from AST
- `compiler/src/doc/doc_index.cpp` - Searchable index
- `compiler/src/doc/html_generator.cpp` - HTML output
- `compiler/src/doc/json_generator.cpp` - JSON output
- `compiler/src/doc/markdown_generator.cpp` - Markdown output
- `compiler/src/cli/cmd_doc.cpp` - CLI command
- `compiler/runtime/doc_template/` - HTML templates, CSS, JS

### Files to Modify
- `compiler/include/lexer/token.hpp` - Add doc token kinds
- `compiler/src/lexer/lexer_core.cpp` - Preserve doc comments
- `compiler/include/parser/ast.hpp` - Add doc fields
- `compiler/src/parser/parser_*.cpp` - Parse doc comments
- `compiler/src/cli/dispatcher.cpp` - Register doc command
- `CMakeLists.txt` - Add doc module

### Breaking Changes
- None for users
- AST structure changes (internal)

### User Benefits
- Browse library documentation offline
- Search for functions, types, behaviors
- See examples inline with documentation
- LLMs can query documentation via MCP
- IDE hover documentation (future LSP integration)
