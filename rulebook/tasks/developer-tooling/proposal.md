# Proposal: Developer Tooling

## Overview

This task covers developer tooling enhancements for the TML language, specifically the Language Server Protocol (LSP) implementation and documentation generator.

## Motivation

With the core compiler infrastructure complete (Phases 0-14), the next step to improve developer experience is providing IDE integration and automated documentation generation.

### LSP Server Benefits
- Code completion in editors (VS Code, Neovim, etc.)
- Go-to-definition navigation
- Hover information for types and functions
- Real-time error diagnostics
- Find references and rename refactoring

### Documentation Generator Benefits
- Auto-generated API documentation from source code
- Consistent documentation format
- Cross-reference linking between modules
- Searchable documentation output

## Dependencies

- Core compiler infrastructure (completed)
- Type checker (completed)
- Module system (completed)

## Scope

### Phase 9: LSP Server
- LSP protocol handler
- textDocument/completion
- textDocument/hover
- textDocument/definition
- textDocument/references
- textDocument/rename
- textDocument/diagnostic
- workspace/symbol
- VS Code extension
- Neovim configuration

### Phase 10: Documentation Generator
- Doc comment format (`///`)
- Doc comment parser
- HTML output generator
- Markdown output generator
- `tml doc` command
- Cross-reference linking
- Search functionality

## Timeline

These are enhancement features with no specific deadline. They can be implemented incrementally as needed.
