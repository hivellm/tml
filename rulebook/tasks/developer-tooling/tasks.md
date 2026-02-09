# Tasks: Developer Tooling (LSP + VSCode + Documentation)

**Status**: Not Started (0%)
**Priority**: Medium
**Consolidates**: `developer-tooling` (original) + `create-vscode-extension` + `implement-tml-doc`

## Phase 1: Doc Comment Preservation (Compiler)

- [ ] 1.1 Add `TokenKind::DocComment` and `TokenKind::ModuleDocComment` to lexer
- [ ] 1.2 Modify `skip_line_comment()` to detect and preserve `///` and `//!`
- [ ] 1.3 Store doc comment content in token
- [ ] 1.4 Add `std::optional<std::string> doc` field to `FuncDecl`, `TypeDecl`, `BehaviorDecl`, `ImplBlock`
- [ ] 1.5 Add `doc` field to `FieldDecl`, `VariantDecl`, `MethodDecl`
- [ ] 1.6 Add `std::vector<std::string> module_docs` to `Module` AST node
- [ ] 1.7 Parse and attach doc comments to following items in parser
- [ ] 1.8 Verify existing tests pass with doc comment preservation

## Phase 2: Documentation Model & Generator (`tml doc`)

- [ ] 2.1 Create `compiler/src/doc/doc_model.hpp` — DocItem, DocModule, DocIndex structs
- [ ] 2.2 Create `compiler/src/doc/doc_extractor.cpp` — Extract docs from AST
- [ ] 2.3 Create `compiler/src/doc/doc_index.cpp` — Searchable index
- [ ] 2.4 Create `compiler/src/doc/html_generator.cpp` — Rust-style HTML output with search and cross-refs
- [ ] 2.5 Create `compiler/src/doc/json_generator.cpp` — Machine-readable JSON output for MCP
- [ ] 2.6 Create `compiler/src/doc/markdown_generator.cpp` — Markdown output
- [ ] 2.7 Create `compiler/src/cli/cmd_doc.cpp` — CLI command registration
- [ ] 2.8 Implement `tml doc` — Generate HTML documentation site
- [ ] 2.9 Implement `tml doc --json` — Export as JSON
- [ ] 2.10 Implement `tml doc --serve` — Local documentation server
- [ ] 2.11 Implement `tml doc <symbol>` — Terminal lookup with colored output
- [ ] 2.12 Create HTML templates, CSS, JS in `compiler/runtime/doc_template/`
- [ ] 2.13 Register doc command in `dispatcher.cpp`
- [ ] 2.14 Verify doc generation works for lib/core and lib/std modules

## Phase 3: VSCode Extension

- [ ] 3.1 Create `tools/vscode-tml/` directory with extension scaffold
- [ ] 3.2 Create `package.json` extension manifest with language contribution
- [ ] 3.3 Create `language-configuration.json` (brackets, comments, auto-closing)
- [ ] 3.4 Create `syntaxes/tml.tmLanguage.json` — Complete TextMate grammar
- [ ] 3.5 Implement syntax highlighting for all 28 keywords
- [ ] 3.6 Implement highlighting for operators, generics `[]`, effects declarations
- [ ] 3.7 Implement proper scoping for comments (`///`, `//!`, `//`, `/* */`), strings, code blocks
- [ ] 3.8 Implement semantic highlighting: types, functions, stable IDs, capabilities
- [ ] 3.9 Create snippets for common patterns (func, type, behavior, when, loop, impl)
- [ ] 3.10 Create build task configurations for `tml build` and `tml test`
- [ ] 3.11 Verify theme-aware coloring works with dark and light themes
- [ ] 3.12 Test extension with sample TML files

## Phase 4: LSP Server

- [ ] 4.1 Create `compiler/src/lsp/` directory
- [ ] 4.2 Implement LSP protocol handler (JSON-RPC 2.0 over stdio)
- [ ] 4.3 Implement `textDocument/completion` — Keywords, stdlib functions, type names
- [ ] 4.4 Implement `textDocument/hover` — Type info, effect declarations, doc comments
- [ ] 4.5 Implement `textDocument/definition` — Go-to-definition, import navigation
- [ ] 4.6 Implement `textDocument/references` — Find all references to a symbol
- [ ] 4.7 Implement `textDocument/rename` — Rename symbol across files
- [ ] 4.8 Implement `textDocument/diagnostic` — Real-time error diagnostics
- [ ] 4.9 Implement `workspace/symbol` — Search symbols across workspace
- [ ] 4.10 Integrate with `tml doc` model for hover documentation content
- [ ] 4.11 Implement incremental document sync for fast response
- [ ] 4.12 Verify <100ms response time for common operations

## Phase 5: IDE Integration & Polish

- [ ] 5.1 Connect VSCode extension to LSP server (client-side TypeScript)
- [ ] 5.2 Create Neovim LSP configuration
- [ ] 5.3 Implement auto-detection of TML compiler binary path
- [ ] 5.4 Implement workspace configuration (tml.toml integration)
- [ ] 5.5 Publish extension to VSCode marketplace
- [ ] 5.6 Create extension README with setup and usage instructions
- [ ] 5.7 Verify extension passes VSCode marketplace validation

## Validation

- [ ] V.1 Doc comments preserved through lexer -> parser -> AST for all item types
- [ ] V.2 `tml doc` produces valid, browsable HTML documentation
- [ ] V.3 `tml doc --json` produces valid JSON parseable by MCP
- [ ] V.4 VSCode extension highlights all TML constructs correctly
- [ ] V.5 LSP autocomplete works for keywords and common patterns
- [ ] V.6 LSP hover shows type info and documentation
- [ ] V.7 LSP go-to-definition navigates to correct source location
- [ ] V.8 Real-time diagnostics appear within 100ms of typing
