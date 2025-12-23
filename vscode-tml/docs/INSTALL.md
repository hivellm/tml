# Installation and Testing Guide

## Prerequisites

- Visual Studio Code 1.75.0 or higher
- Node.js (for packaging, optional)

## Option 1: Development Mode (Testing)

This is the recommended way to test the extension during development:

1. **Open the extension folder in VS Code**
   ```bash
   cd vscode-tml
   code .
   ```

2. **Launch Extension Development Host**
   - Press `F5` or go to Run → Start Debugging
   - A new VS Code window will open with the extension loaded
   - This is called the "Extension Development Host"

3. **Test the extension**
   - In the Extension Development Host window, open one of the example files:
     - `examples/basic.tml`
     - `examples/types.tml`
     - `examples/advanced.tml`
   - You should see syntax highlighting applied
   - Try typing TML code to see auto-closing brackets and other features

4. **Reload the extension**
   - After making changes to the extension files
   - In the Extension Development Host window, press `Ctrl+R` (Cmd+R on Mac)
   - Or use the Command Palette: "Developer: Reload Window"

## Option 2: Install as VSIX (Production)

To install the extension permanently in VS Code:

1. **Install vsce (VS Code Extension Manager)**
   ```bash
   npm install -g @vscode/vsce
   ```

2. **Package the extension**
   ```bash
   cd vscode-tml
   vsce package
   ```

   This creates a `.vsix` file (e.g., `tml-language-0.1.0.vsix`)

3. **Install the VSIX**
   - Open VS Code
   - Go to Extensions (Ctrl+Shift+X)
   - Click the "..." menu at the top right
   - Select "Install from VSIX..."
   - Choose the generated `.vsix` file

4. **Verify installation**
   - Open a `.tml` file
   - Syntax highlighting should be active
   - Check the Extensions panel to see "TML Language Support" installed

## Testing Syntax Highlighting

### Quick Visual Test

Open each example file and verify:

1. **basic.tml**
   - Keywords (`func`, `if`, `then`, `else`, `loop`, `for`) are highlighted
   - Types (`I32`, `Bool`, `String`) are highlighted
   - Comments are grayed out
   - Strings are colored
   - Numbers are highlighted

2. **types.tml**
   - Type names (`Point`, `Shape`, `Outcome`) are highlighted
   - `type` keyword is highlighted
   - `extend` and `with` keywords are highlighted
   - Generic brackets `[T]` are properly colored

3. **advanced.tml**
   - Directives (`@test`, `@benchmark`, `@when`) are highlighted
   - Doc comments (`///`, `//!`) have special styling
   - AI comments (`// @ai:context:`) are highlighted
   - Closures (`do(x) x + 1`) are highlighted correctly

### Features to Test

- **Auto-closing**
  - Type `{` and verify `}` is auto-inserted
  - Type `(` and verify `)` is auto-inserted
  - Type `"` and verify closing `"` is added

- **Comment toggling**
  - Select a line
  - Press `Ctrl+/` (Cmd+/ on Mac)
  - Verify `//` is added/removed

- **Block comment**
  - Select multiple lines
  - Press `Ctrl+Shift+A` (Cmd+Shift+A on Mac)
  - Verify `/* */` wraps the selection

- **Bracket matching**
  - Place cursor next to `{`, `[`, or `(`
  - Verify matching bracket is highlighted

- **Code folding**
  - Hover over line numbers near `{`
  - Click the fold icon to collapse/expand blocks

## Troubleshooting

### Extension not loading

- Check that you're opening `.tml` files (not `.txt` or other extensions)
- Restart VS Code
- Check the Output panel (View → Output) and select "Extension Host" for error messages

### Syntax highlighting not working

- Verify the file has `.tml` extension
- Check that the language mode is set to "TML" (bottom right of VS Code)
- Reload the window (Ctrl+R in Extension Development Host)

### Changes not appearing

- Make sure you're editing the files in the correct location
- Reload the Extension Development Host window (Ctrl+R)
- Or restart the debugging session (F5)

## Publishing (For Maintainers)

To publish to the VS Code Marketplace:

1. **Create a publisher account**
   - Go to https://marketplace.visualstudio.com/manage
   - Create a publisher ID

2. **Login with vsce**
   ```bash
   vsce login your-publisher-name
   ```

3. **Publish**
   ```bash
   vsce publish
   ```

## File Structure

```
vscode-tml/
├── package.json              # Extension manifest
├── language-configuration.json  # Language config (brackets, comments)
├── syntaxes/
│   └── tml.tmLanguage.json  # TextMate grammar (syntax rules)
├── examples/                # Example TML files
│   ├── basic.tml
│   ├── types.tml
│   └── advanced.tml
├── README.md                # Extension documentation
├── CHANGELOG.md             # Version history
├── INSTALL.md              # This file
└── .vscodeignore           # Files to exclude from package
```

## Next Steps

After verifying syntax highlighting works:

1. **Customize colors**
   - Edit `syntaxes/tml.tmLanguage.json`
   - Modify `name` fields to use different scopes
   - VS Code themes will color based on these scopes

2. **Add snippets**
   - Create `snippets/tml.json`
   - Add common code patterns

3. **Add language features**
   - Implement language server for IntelliSense
   - Add go-to-definition, find references
   - Add error diagnostics

## Support

For issues or questions:
- Open an issue on GitHub
- Check the TML language documentation
- Review VS Code extension development docs
