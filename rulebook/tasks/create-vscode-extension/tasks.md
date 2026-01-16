# Tasks: Create VSCode Extension for TML

## Progress: 92% (67/73 tasks complete)

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
- [x] 3.10 Add markdown injection for ```tml code blocks

## 4. Language Configuration Phase ✅
- [x] 4.1 Create `language-configuration.json`
- [x] 4.2 Configure comments (line and block)
- [x] 4.3 Configure brackets (`()`, `[]`, `{}`)
- [x] 4.4 Configure auto-closing pairs
- [x] 4.5 Configure indentation rules
- [x] 4.6 Configure word patterns for double-click selection

## 5. Basic Language Server Phase ✅
- [x] 5.1 Implement Language Server Protocol (LSP) client
- [x] 5.2 Create LSP server in TypeScript
- [x] 5.3 Implement text document synchronization
- [x] 5.4 Set up communication protocol between extension and server

## 6. Syntax Validation Phase ✅
- [x] 6.1 Integrate TML parser for syntax validation
- [x] 6.2 Convert parser errors to LSP diagnostics
- [x] 6.3 Implement real-time error reporting
- [x] 6.4 Handle parser recovery and partial validation

## 7. Autocompletion Phase ✅
- [x] 7.1 Implement keyword completion provider
- [x] 7.2 Add standard library function completions
- [x] 7.3 Add type name completions
- [x] 7.4 Implement context-aware completions (OOP: class/interface context, base. member access)
- [x] 7.5 Add snippet completions for common patterns
- [x] 7.6 Add OOP snippets (override, virtual, abstract, class extends/implements, properties)
- [x] 7.7 Implement import statement completion (module path completion, wildcard imports)
- [x] 7.8 Add effect, capability, and contract completions

## 8. Hover Information Phase ✅
- [x] 8.1 Implement hover provider for types
- [x] 8.2 Add hover information for functions (signatures, effects)
- [x] 8.3 Display contract information on hover (requires, ensures, invariant, assert, assume)
- [x] 8.4 Show capability and effect declarations (Read, Write, Fs, Net, io, pure, async, etc.)
- [x] 8.5 Implement documentation display
- [x] 8.6 Add module hover information

## 9. Go-to-Definition Phase ⏸️ (requires compiler integration)
- [ ] 9.1 Implement definition provider
- [ ] 9.2 Resolve stable ID references (`@xxxxxxxx`)
- [ ] 9.3 Handle import resolution
- [ ] 9.4 Implement module navigation
- [ ] 9.5 Support trait and type navigation

## 10. Semantic Highlighting Phase ✅
- [x] 10.1 Implement semantic tokens provider
- [x] 10.2 Highlight types and type parameters
- [x] 10.3 Highlight functions and methods
- [x] 10.4 Highlight decorators (@test, @bench, etc.)
- [x] 10.5 Highlight capabilities and effects (pure, io, async, Read, Write, Fs, etc.)
- [x] 10.6 Integrate with VSCode themes
- [x] 10.7 Highlight contract keywords

## 11. Build Integration Phase ✅
- [x] 11.1 Create task provider for TML compilation
- [x] 11.2 Implement problem matcher for compiler errors
- [x] 11.3 Add build and run commands
- [x] 11.4 Configure workspace settings

## 12. Testing Phase ✅
- [x] 12.1 Write unit tests for extension components (server.test.ts, extension.test.ts)
- [x] 12.2 Test syntax highlighting accuracy (Syntax Highlighting Tests suite)
- [x] 12.3 Test autocompletion functionality (completion provider tests)
- [x] 12.4 Test language server integration (hover provider, semantic tokens tests)
- [x] 12.5 Test error reporting accuracy (Diagnostics Tests suite)
- [ ] 12.6 Perform cross-platform testing (Windows, macOS, Linux)

## 13. Documentation Phase ✅
- [x] 13.1 Create README.md for the extension
- [x] 13.2 Document installation and setup
- [x] 13.3 Create usage guide and examples
- [x] 13.4 Document extension features and limitations
- [x] 13.5 Create changelog and release notes

## Current Status (2026-01-15)

**Version**: 0.13.0

**Implemented Features**:
- ✅ Syntax highlighting for all TML keywords and constructs
- ✅ Language configuration (brackets, comments, indentation)
- ✅ VSIX packaging for local installation
- ✅ Support for .tml file extension
- ✅ Documentation and examples
- ✅ Markdown injection for ```tml code blocks
- ✅ Language Server Protocol (LSP) client/server
- ✅ Keyword, type, and function autocompletion
- ✅ Snippet completions for common patterns
- ✅ OOP snippets (override, virtual, abstract, class extends/implements, properties)
- ✅ Context-aware completions for OOP (class/interface detection, base. member access)
- ✅ Import statement completion (module path, wildcard, alias)
- ✅ Effect, capability, and contract completions
- ✅ Hover information for keywords, types, builtins, effects, capabilities, contracts, modules
- ✅ Semantic highlighting (types, functions, decorators, effects, capabilities, contracts)
- ✅ Build integration (commands, task provider, problem matchers)
- ✅ Keybindings (Ctrl+Shift+B for build, F5 for run)
- ✅ Context menu integration
- ✅ Syntax validation and diagnostics (compiler integration via JSON output)
- ✅ Real-time error reporting with debounced validation
- ✅ Comprehensive test suite (server unit tests + extension integration tests)

**Not Yet Implemented**:
- ⏸️ Go-to-definition and find references (requires compiler integration)

**Next Steps**:
1. Install dependencies: `cd vscode-tml && pnpm install`
2. Compile TypeScript: `pnpm run compile`
3. Test extension in development mode (F5)

**Files Added/Modified**:
- `src/client/extension.ts` - LSP client + command/task registration
- `src/client/commands.ts` - Build, run, test, clean commands
- `src/client/taskProvider.ts` - TML task provider
- `src/server/server.ts` - LSP server with completions, hover, semantic tokens, effects, capabilities, contracts, diagnostics
- `src/test/runTest.ts` - Test runner entry point
- `src/test/suite/index.ts` - Test suite loader (async glob)
- `src/test/suite/extension.test.ts` - Extension integration tests (6 test suites)
- `src/test/suite/server.test.ts` - Server unit tests (7 test suites)
- `tsconfig.json` - TypeScript configuration
- `syntaxes/tml.markdown.tmLanguage.json` - Markdown code block injection
- `package.json` - Commands, keybindings, task definitions, problem matchers, test deps, diagnostics settings
