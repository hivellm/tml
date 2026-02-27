# Proposal: Developer Tooling (LSP + VSCode + Documentation)

## Status: PROPOSED

**Consolidates**: `developer-tooling` (original) + `create-vscode-extension` + `implement-tml-doc`

## Why

TML needs a complete developer experience to be practically usable. Three separate tasks were covering overlapping ground:

- `developer-tooling`: LSP server + doc generator (high-level outline)
- `create-vscode-extension`: VSCode extension with syntax highlighting, LSP client, snippets
- `implement-tml-doc`: Documentation generation system (`tml doc`, HTML/JSON/Markdown output)

These are tightly coupled: the VSCode extension depends on the LSP server, the LSP server benefits from the documentation model, and all three share the same compiler integration points (parser, type checker, module system). Consolidating eliminates duplication and ensures a coherent developer experience.

### Current State

- Doc comments (`///`, `//!`) exist in ~50 library modules but are **discarded** during lexing
- No LSP server exists
- No VSCode extension exists
- No `tml doc` command exists
- Documentation is only readable by opening source files

## What Changes

### Phase 1: Doc Comment Preservation (Compiler Changes)

Lexer and parser changes to preserve documentation through the pipeline.

- Add `TokenKind::DocComment` and `TokenKind::ModuleDocComment` to lexer
- Modify `skip_line_comment()` to detect and preserve `///` and `//!`
- Add `std::optional<std::string> doc` field to AST item nodes (FuncDecl, TypeDecl, BehaviorDecl, etc.)
- Add `std::vector<std::string> module_docs` to Module AST node
- Parse and attach doc comments to following items

### Phase 2: Documentation Model & Generator (`tml doc`)

Extract documentation from AST and generate multiple output formats.

- `DocItem` struct: id, kind, name, path, signature, doc, examples, see_also
- `DocModule` struct: name, path, doc, items, submodules
- `DocIndex` for searchable documentation database
- HTML output: Rust-style documentation site with search and cross-references
- JSON output: Machine-readable format for MCP integration
- Markdown output: For inclusion in other docs
- Terminal output: `tml doc <symbol>` for quick lookup
- `tml doc --serve` for local documentation server

### Phase 3: VSCode Extension

TextMate grammar, language configuration, and extension packaging.

- Extension manifest (`package.json`) and language configuration
- Complete TextMate grammar for all TML constructs (28 keywords, operators, generics with `[]`, effects)
- Semantic highlighting: types, functions, stable IDs, capabilities
- Snippets for common patterns (function definitions, type declarations, when/loop)
- Build task configurations
- Theme-aware coloring

### Phase 4: LSP Server

Language Server Protocol implementation for IDE features.

- LSP protocol handler (JSON-RPC 2.0 over stdio)
- `textDocument/completion` - Keywords, standard library functions, type names
- `textDocument/hover` - Type information, effect declarations, doc comments
- `textDocument/definition` - Go-to-definition, import navigation, stable ID resolution
- `textDocument/references` - Find all references to a symbol
- `textDocument/rename` - Rename symbol across files
- `textDocument/diagnostic` - Real-time error diagnostics from parser/typechecker
- `workspace/symbol` - Search symbols across workspace
- Integration with `tml doc` model for hover documentation

### Phase 5: IDE Integration & Polish

Connect all components for a seamless experience.

- VSCode extension connects to LSP server
- Neovim configuration for LSP
- Debug adapter protocol support (future)
- Performance: <100ms response time for common operations
- Extension published to VSCode marketplace

## Impact

- Affected specs: `02-LEXICAL.md`, `03-GRAMMAR.md`, `09-CLI.md`, `13-BUILTINS.md`
- Affected code: `compiler/include/lexer/`, `compiler/include/parser/`, `compiler/src/cli/`, new `compiler/src/doc/`, new `compiler/src/lsp/`, new `tools/vscode-tml/`
- Breaking change: NO (all additive; AST internal changes only)
- User benefit: Full IDE experience, browsable documentation, real-time diagnostics

## Dependencies

- Core compiler infrastructure (completed)
- Type checker (completed)
- Module system (completed)
- Node.js/npm for VSCode extension development

## Success Criteria

1. Doc comments preserved through lexer -> parser -> AST
2. `tml doc` generates browsable HTML documentation site
3. `tml doc --json` produces machine-readable output for MCP
4. `tml doc <symbol>` shows documentation in terminal
5. VSCode extension provides syntax highlighting for all TML constructs
6. LSP autocomplete provides keywords and common patterns
7. LSP hover shows type info and documentation
8. LSP go-to-definition navigates to symbol source
9. Real-time diagnostics match compiler output
10. Performance: <100ms response for common IDE operations
11. Extension passes VSCode marketplace validation
