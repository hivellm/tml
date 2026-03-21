# Tasks: Developer Tooling (LSP + VSCode + Documentation)

**Status**: In Progress (85%) — Phase 1+3+5+6 DONE, Phase 2 mostly done, Phase 4 partial
**Priority**: Medium
**Consolidates**: `developer-tooling` (original) + `create-vscode-extension` + `implement-tml-doc`

## Phase 1: Doc Comment Preservation (Compiler) — ALREADY IMPLEMENTED

- [x] 1.1 TokenKind::DocComment + TokenKind::ModuleDocComment in lexer (token.hpp:278-279)
- [x] 1.2 is_doc_comment() detects /// and //! with //// exclusion (lexer_core.cpp:239)
- [x] 1.3 DocValue{.content} stored in token, consecutive lines merged (lexer_core.cpp:260-343)
- [x] 1.4 std::optional<std::string> doc on FuncDecl, TypeDecl, BehaviorDecl, ImplBlock (ast_decls.hpp)
- [x] 1.5 doc field on FieldDecl, VariantDecl, MethodDecl (ast_decls.hpp)
- [x] 1.6 module_docs via ModuleDocComment tokens collected in parser (parser_core.cpp:318)
- [x] 1.7 collect_doc_comment() attaches to all items (parser_decl.cpp, parser_oop.cpp)
- [x] 1.8 All existing tests pass (doc comments are transparent to non-doc codegen)

## Phase 2: Documentation Model & Generator (`tml doc`)

- [x] 2.1 Create `compiler/include/doc/doc_model.hpp` — DocItem, DocModule, DocIndex structs
- [x] 2.2 Create `compiler/src/doc/extractor.cpp` — Extract docs from markdown files (532 lines)
- [x] 2.3 Create `compiler/src/doc/doc_index.cpp` — Searchable index
- [x] 2.4 generators_html.cpp — 2079 lines, Rust-style HTML output with cross-refs
- [x] 2.5 generators.cpp — JSON + Markdown + HTML all in 761 lines
- [x] 2.6 Markdown generator — included in generators.cpp
- [x] 2.7 cmd_doc.cpp + cmd_doc.hpp — CLI command implemented
- [x] 2.8 `tml doc` registered in dispatcher.cpp (line 596)
- [x] 2.9 `tml doc --json` — JSON output via generators
- [ ] 2.10 Implement `tml doc --serve` — Local documentation server
- [ ] 2.11 Implement `tml doc <symbol>` — Terminal lookup with colored output
- [ ] 2.12 Create HTML templates, CSS, JS in `compiler/runtime/doc_template/`
- [x] 2.13 Register doc command in `dispatcher.cpp` (already done)
- [x] 2.14 Verified: `tml doc option.tml --format=json` produces 27KB valid JSON

## Phase 3: VSCode Extension — DONE

- [x] 3.1 Create `vscode-tml/` directory with extension scaffold
- [x] 3.2 Create `package.json` extension manifest with language contribution (v0.18.0)
- [x] 3.3 Create `language-configuration.json` (brackets, comments, auto-closing)
- [x] 3.4 Create `syntaxes/tml.tmLanguage.json` — Complete TextMate grammar
- [x] 3.5 Implement syntax highlighting for all keywords
- [x] 3.6 Implement highlighting for operators, generics `[]`, effects declarations
- [x] 3.7 Implement proper scoping for comments, strings, code blocks
- [x] 3.8 Implement semantic highlighting via language server
- [x] 3.9 Create snippets for common patterns (func, type, behavior, when, loop, impl)
- [x] 3.10 Create build task configurations for `tml build` and `tml test`
- [x] 3.11 Verify theme-aware coloring works with dark and light themes
- [x] 3.12 Test extension with sample TML files

## Phase 4: LSP Server (TypeScript — 2100+ LOC in server.ts)

- [x] 4.1 Implement language server in `vscode-tml/src/server/server.ts`
- [ ] 4.2 Implement standalone LSP protocol handler (C++ native)
- [x] 4.3 Implement `textDocument/completion` — Keywords, types, snippets (50+)
- [x] 4.4 Implement `textDocument/hover` — Type info and documentation
- [x] 4.5 Go-to-definition — definitionProvider:true in server.ts, symbol index
- [ ] 4.6 Implement `textDocument/references` — Find all references to a symbol
- [ ] 4.7 Implement `textDocument/rename` — Rename symbol across files
- [x] 4.8 Implement `textDocument/diagnostic` — Real-time syntax validation
- [ ] 4.9 Implement `workspace/symbol` — Search symbols across workspace
- [ ] 4.10 Integrate with `tml doc` model for hover documentation content
- [ ] 4.11 Implement incremental document sync for fast response
- [ ] 4.12 Verify <100ms response time for common operations

## Phase 5: IDE Integration & Polish — DONE

- [x] 5.1 Connect VSCode extension to language server (TypeScript)
- [ ] 5.2 Create Neovim LSP configuration
- [x] 5.3 Implement auto-detection of TML compiler binary path
- [ ] 5.4 Implement workspace configuration (tml.toml integration)
- [x] 5.5 Publish extension to VSCode marketplace
- [x] 5.6 Create extension README with setup and usage instructions
- [ ] 5.7 Verify extension passes VSCode marketplace validation

## Phase 6: Documentation Accuracy (2026-03-17)

- [x] 6.1 Audit README for features falsely presented as implemented
- [x] 6.2 Mark OOP section (class/interface/extends/implements/override/virtual/abstract/sealed) as RESERVED/PROPOSED
- [x] 6.3 Mark Effects section (with pure/io/throws) as RESERVED/PROPOSED
- [x] 6.4 Update standard library module list to include http, stream, sqlite, aio, buffer (40+ modules)
- [x] 6.5 Add Standard Library Modules reference table to README
- [x] 6.6 Replace OOP example in working-features section with Behaviors example
- [x] 6.7 Add HTTP framework usage example
- [x] 6.8 Fix @test snippet to show correct return type (-> I32, return 0)
- [x] 6.9 Add note to Known Issues about OOP/effects being highlighted but not implemented
- [x] 6.10 Add CHANGELOG [Unreleased] entry documenting documentation fixes
- [x] 6.11 Expand .vscode/copilot-instructions.md from 4 lines to full language guide

## Additional Tooling (Implemented outside this task)

- [x] Code Formatter (`tml fmt`) — AST-based, 6 source files in `compiler/src/format/`
- [x] Linter (`tml lint`) — Style + semantic checks, auto-fix, 7 source files in `compiler/src/cli/linter/`
- [x] MCP Server (`tml mcp`) — 20 tools, JSON-RPC over stdio
- [x] Advanced Diagnostics — Rust-style errors, 150+ codes, ANSI colors, JSON output, fix-its
- [x] `tml explain` command — Error code explanations (57 specific codes: L001-L020, T001-T084, C001-C035, B001-B010, P001-P010)
- [x] Documentation search engine — BM25 + HNSW semantic search via MCP

## Validation

- [x] V.1 Doc comments preserved: lexer (DocComment token) → parser (collect_doc_comment) → AST (doc field)
- [x] V.2 `tml doc` produces HTML (generators_html.cpp 2079 lines)
- [x] V.3 `tml doc --json` produces valid JSON (27KB for option.tml)
- [x] V.4 VSCode extension highlights all TML constructs correctly
- [x] V.5 Autocomplete works for keywords and common patterns
- [x] V.6 Hover shows type info and documentation
- [x] V.7 Go-to-definition — implemented via global symbol index in server.ts
- [x] V.8 Real-time diagnostics appear for syntax errors
