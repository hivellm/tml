# Proposal: Create VSCode Extension for TML

## Why

The TML programming language needs comprehensive IDE support to be practically usable by developers. While the language specification is complete, developers currently lack syntax highlighting, autocompletion, error detection, and other essential IDE features. A VSCode/Cursor extension will provide the first-class development experience that TML deserves, making it accessible to developers who want to try the language. This is crucial for adoption and community building around TML.

## What Changes

### New Components

1. **VSCode Extension Package** (`tools/vscode-tml/`)
   - Extension manifest (`package.json`)
   - Language configuration (`language-configuration.json`)
   - Syntax highlighting grammar (`syntaxes/tml.tmLanguage.json`)
   - TypeScript source code for language server features

2. **Language Server Protocol (LSP) Implementation**
   - Syntax validation using TML parser
   - Basic autocompletion for keywords and built-ins
   - Error reporting with accurate spans
   - Go-to-definition for stable IDs
   - Hover information for types and effects

3. **Syntax Highlighting**
   - Complete TextMate grammar for all TML constructs
   - Support for all keywords, operators, and literals
   - Proper scoping for comments, strings, and code blocks
   - Theme-aware coloring

4. **Development Tools Integration**
   - Integration with TML compiler for error reporting
   - Build task configurations
   - Debug adapter protocol support (future)

### Features to Implement

Based on `docs/specs/` and TML's unique characteristics:

- **Syntax Highlighting**: All 28 keywords, operators, generics with `[]`, effects declarations
- **Semantic Highlighting**: Types, functions, stable IDs, capabilities
- **Autocompletion**: Keywords, standard library functions, type names
- **Error Diagnostics**: Parser errors with precise location reporting
- **Hover Information**: Type information, effect declarations, contract specifications
- **Go-to-Definition**: Stable ID resolution, import navigation
- **Snippets**: Common patterns like function definitions, type declarations

## Impact

- **Affected specs**: 02-LEXICAL.md, 03-GRAMMAR.md, 09-CLI.md, 13-BUILTINS.md
- **Affected code**: New `tools/vscode-tml/` directory
- **Breaking change**: NO (new tooling component)
- **User benefit**: Enables practical TML development with modern IDE features

## Dependencies

- Node.js and npm for extension development
- VSCode Extension Development tools
- Access to TML compiler binaries for language server features

## Success Criteria

1. Syntax highlighting works for all TML constructs
2. Extension passes VSCode marketplace validation
3. Autocompletion provides all keywords and common patterns
4. Error reporting matches compiler output format
5. Performance: <100ms response time for common operations
6. Documentation includes setup and usage instructions
