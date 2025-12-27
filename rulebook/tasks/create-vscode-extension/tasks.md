# Tasks: Create VSCode Extension for TML

## Progress: 33% (14/42 tasks complete)

## 1. Setup Phase ✅
- [x] 1.1 Create `vscode-tml/` directory structure
- [x] 1.2 Initialize npm package with VSCode extension template
- [x] 1.3 Configure TypeScript and build tools
- [x] 1.4 Set up development environment with VSCode

## 2. Extension Manifest Phase ✅
- [x] 2.1 Create `package.json` with extension metadata
- [x] 2.2 Configure activation events for `.tml` files
- [x] 2.3 Define extension capabilities (syntax highlighting)
- [x] 2.4 Set up publishing configuration

## 3. Syntax Highlighting Phase ✅
- [x] 3.1 Create TextMate grammar file (`syntaxes/tml.tmLanguage.json`)
- [x] 3.2 Implement keyword patterns (32 keywords from spec)
- [x] 3.3 Implement operator patterns (arithmetic, comparison, logical)
- [x] 3.4 Implement literal patterns (strings, numbers, characters)
- [x] 3.5 Implement comment patterns (single-line `//`, multi-line `/* */`, doc `///`)
- [x] 3.6 Implement identifier and stable ID patterns (`@xxxxxxxx`)
- [x] 3.7 Implement generic syntax with `[]` brackets
- [x] 3.8 Implement behavior and effect declarations
- [x] 3.9 Test syntax highlighting with example files

## 4. Language Configuration Phase ✅
- [x] 4.1 Create `language-configuration.json`
- [x] 4.2 Configure comments (line and block)
- [x] 4.3 Configure brackets (`()`, `[]`, `{}`)
- [x] 4.4 Configure auto-closing pairs
- [x] 4.5 Configure indentation rules
- [x] 4.6 Configure word patterns for double-click selection

## 5. Basic Language Server Phase
- [ ] 5.1 Implement Language Server Protocol (LSP) client
- [ ] 5.2 Create LSP server in TypeScript
- [ ] 5.3 Implement text document synchronization
- [ ] 5.4 Set up communication protocol between extension and server

## 6. Syntax Validation Phase
- [ ] 6.1 Integrate TML parser for syntax validation
- [ ] 6.2 Convert parser errors to LSP diagnostics
- [ ] 6.3 Implement real-time error reporting
- [ ] 6.4 Handle parser recovery and partial validation

## 7. Autocompletion Phase
- [ ] 7.1 Implement keyword completion provider
- [ ] 7.2 Add standard library function completions
- [ ] 7.3 Add type name completions
- [ ] 7.4 Implement context-aware completions
- [ ] 7.5 Add snippet completions for common patterns
- [ ] 7.6 Implement import statement completion

## 8. Hover Information Phase
- [ ] 8.1 Implement hover provider for types
- [ ] 8.2 Add hover information for functions (signatures, effects)
- [ ] 8.3 Display contract information on hover
- [ ] 8.4 Show capability and effect declarations
- [ ] 8.5 Implement documentation display

## 9. Go-to-Definition Phase
- [ ] 9.1 Implement definition provider
- [ ] 9.2 Resolve stable ID references (`@xxxxxxxx`)
- [ ] 9.3 Handle import resolution
- [ ] 9.4 Implement module navigation
- [ ] 9.5 Support trait and type navigation

## 10. Semantic Highlighting Phase
- [ ] 10.1 Implement semantic tokens provider
- [ ] 10.2 Highlight types and type parameters
- [ ] 10.3 Highlight functions and methods
- [ ] 10.4 Highlight stable IDs and references
- [ ] 10.5 Highlight capabilities and effects
- [ ] 10.6 Integrate with VSCode themes

## 11. Build Integration Phase
- [ ] 11.1 Create task provider for TML compilation
- [ ] 11.2 Implement problem matcher for compiler errors
- [ ] 11.3 Add build and run commands
- [ ] 11.4 Configure workspace settings

## 12. Testing Phase
- [ ] 12.1 Write unit tests for extension components
- [ ] 12.2 Test syntax highlighting accuracy
- [ ] 12.3 Test autocompletion functionality
- [ ] 12.4 Test language server integration
- [ ] 12.5 Test error reporting accuracy
- [ ] 12.6 Perform cross-platform testing (Windows, macOS, Linux)

## 13. Documentation Phase ⏳ (80% complete)
- [x] 13.1 Create README.md for the extension
- [x] 13.2 Document installation and setup
- [x] 13.3 Create usage guide and examples
- [x] 13.4 Document extension features and limitations
- [x] 13.5 Create changelog and release notes (v0.3.1 - 2025-12-26)

## 14. Publishing Phase ⏸️
- [ ] 14.1 Prepare extension for marketplace submission
- [ ] 14.2 Test extension in clean VSCode environment
- [ ] 14.3 Create marketplace listing with screenshots
- [ ] 14.4 Submit to VSCode marketplace
- [ ] 14.5 Set up automated publishing pipeline

## Current Status (2025-12-26)

**Version**: 0.3.1

**Implemented Features**:
- ✅ Syntax highlighting for all TML keywords and constructs
- ✅ Language configuration (brackets, comments, indentation)
- ✅ VSIX packaging for local installation
- ✅ Support for .tml file extension
- ✅ Documentation and examples

**Not Yet Implemented**:
- ⏸️ Language Server Protocol (LSP) integration
- ⏸️ IntelliSense and autocompletion
- ⏸️ Syntax validation and diagnostics
- ⏸️ Go-to-definition and hover information
- ⏸️ Semantic highlighting
- ⏸️ Build task integration
- ⏸️ Marketplace publishing

**Next Steps**:
1. Implement LSP client/server (Phase 5)
2. Add syntax validation (Phase 6)
3. Implement autocompletion (Phase 7)
4. Publish to VSCode Marketplace (Phase 14)




