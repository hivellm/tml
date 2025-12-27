# Editor Support Specification

## ADDED Requirements

### Requirement: VSCode Extension Architecture
The TML VSCode extension SHALL provide comprehensive IDE support for TML development.

#### Scenario: Extension Installation
Given a developer has VSCode installed
When they install the TML extension from marketplace
Then syntax highlighting works immediately
And autocompletion is available for keywords

#### Scenario: Language Server Connection
Given the extension is activated
When a `.tml` file is opened
Then the language server connects successfully
And diagnostics are provided in real-time

### Requirement: Syntax Highlighting
The extension SHALL provide accurate syntax highlighting for all TML language constructs.

#### Scenario: Keyword Highlighting
Given TML source code with keywords
When the file is displayed
Then all 28 keywords are highlighted as keywords
And operators are highlighted appropriately

#### Scenario: Literal Highlighting
Given code with various literals
When displayed in editor
Then string literals are highlighted as strings
And numeric literals are highlighted as numbers
And character literals are highlighted as characters

#### Scenario: Comment Highlighting
Given code with different comment types
When displayed
Then `// line comments` are highlighted as comments
And `/* block comments */` are highlighted as comments
And `/// doc comments` are highlighted as documentation

### Requirement: Autocompletion
The extension SHALL provide intelligent autocompletion for TML code.

#### Scenario: Keyword Completion
Given user types partial keyword
When requesting completion
Then matching keywords are suggested
And snippets for common patterns are available

#### Scenario: Standard Library Completion
Given user types standard library function prefix
When requesting completion
Then matching functions are suggested
And signatures are displayed with effects

### Requirement: Error Diagnostics
The extension SHALL report TML compilation errors with accurate positioning.

#### Scenario: Syntax Error Reporting
Given TML code with syntax error
When file is saved or modified
Then error is reported at correct line and column
And error message matches compiler output

#### Scenario: Semantic Error Reporting
Given code with type mismatch
When analyzed
Then error is reported with type information
And suggestions for fixes are provided

### Requirement: Hover Information
The extension SHALL display contextual information on hover.

#### Scenario: Type Hover
Given cursor over variable
When hovering
Then type information is displayed
And definition location is shown

#### Scenario: Function Signature Hover
Given cursor over function call
When hovering
Then function signature is displayed
And effect declarations are shown
And contract information is included

### Requirement: Navigation Features
The extension SHALL support code navigation within TML projects.

#### Scenario: Go-to-Definition
Given cursor over stable ID reference
When go-to-definition is invoked
Then jumps to definition location
And handles module boundaries correctly

#### Scenario: Import Navigation
Given cursor over import statement
When go-to-definition is invoked
Then navigates to imported module
And shows module contents

### Requirement: Build Integration
The extension SHALL integrate with TML build system.

#### Scenario: Task Provider
Given TML project is opened
When tasks are listed
Then TML build and run tasks are available
And configured with proper working directory

#### Scenario: Error Matching
Given TML compilation fails
When output is parsed
Then errors appear in Problems panel
And clicking error navigates to location

## MODIFIED Requirements

### Requirement: Language Server Protocol Compliance
The extension SHALL implement LSP correctly for TML-specific features.

#### Scenario: Stable ID Resolution
Given stable ID reference in code
When LSP definition request is made
Then resolves to correct definition
And handles ID transformations

#### Scenario: Effect Propagation Display
Given function with effects
When hover information requested
Then shows effect hierarchy
And capability requirements

## REMOVED Requirements

None - this is a new component.

---

*Specification follows OpenSpec format for TML editor support requirements.*




