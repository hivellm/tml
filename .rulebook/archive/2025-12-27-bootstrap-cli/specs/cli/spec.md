# CLI Specification

## Purpose

The CLI provides the user-facing interface to the TML compiler, handling command-line arguments, project configuration, and compilation orchestration.

## ADDED Requirements

### Requirement: Subcommand Handling
The CLI SHALL support subcommands for different operations.

#### Scenario: Build command
Given `tml build`
When the CLI processes the command
Then it compiles the project and produces output

#### Scenario: Run command
Given `tml run`
When the CLI processes the command
Then it compiles and executes the project

#### Scenario: Check command
Given `tml check`
When the CLI processes the command
Then it type-checks without code generation

#### Scenario: Test command
Given `tml test`
When the CLI processes the command
Then it compiles and runs tests

#### Scenario: New command
Given `tml new project_name`
When the CLI processes the command
Then it creates a new project directory with scaffolding

#### Scenario: Fmt command
Given `tml fmt`
When the CLI processes the command
Then it formats TML source files

### Requirement: Flag Handling
The CLI MUST support common flags.

#### Scenario: Release build
Given `tml build --release`
When the CLI processes the command
Then it builds with optimization enabled

#### Scenario: Verbose output
Given `tml build --verbose`
When the CLI processes the command
Then it shows detailed compilation progress

#### Scenario: Help flag
Given `tml --help` or `tml build --help`
When the CLI processes the command
Then it displays usage information

#### Scenario: Version flag
Given `tml --version`
When the CLI processes the command
Then it displays version information

### Requirement: Option Handling
The CLI SHALL support options with values.

#### Scenario: Target specification
Given `tml build --target x86_64-unknown-linux-gnu`
When the CLI processes the command
Then it cross-compiles for the specified target

#### Scenario: Output path
Given `tml build --output ./bin/myapp`
When the CLI processes the command
Then it places the output at the specified path

#### Scenario: Optimization level
Given `tml build -O2`
When the CLI processes the command
Then it uses O2 optimization level

### Requirement: Project Configuration
The CLI SHALL read project configuration from tml.toml.

#### Scenario: Project name
Given tml.toml with `name = "myproject"`
When the CLI reads configuration
Then it uses "myproject" as the project name

#### Scenario: Project version
Given tml.toml with `version = "1.0.0"`
When the CLI reads configuration
Then it uses the specified version

#### Scenario: Dependencies
Given tml.toml with dependencies section
When the CLI reads configuration
Then it resolves and includes dependencies

#### Scenario: Build options
Given tml.toml with build options
When the CLI reads configuration
Then it applies the specified build settings

#### Scenario: Missing tml.toml
Given directory without tml.toml
When the CLI is run
Then it reports error about missing configuration

### Requirement: File Discovery
The CLI MUST discover TML source files correctly.

#### Scenario: Source directory
Given project with src/ directory
When the CLI discovers files
Then it finds all *.tml files in src/

#### Scenario: Main entry point
Given project with src/main.tml
When building executable
Then it uses main.tml as entry point

#### Scenario: Library entry point
Given project with src/lib.tml
When building library
Then it uses lib.tml as entry point

#### Scenario: Module discovery
Given project with nested directories
When the CLI discovers files
Then it correctly maps directories to modules

### Requirement: Compilation Pipeline
The CLI SHALL orchestrate the compilation pipeline.

#### Scenario: Single file compilation
Given single TML source file
When compiling
Then all stages run: lex → parse → typecheck → borrow check → IR → codegen

#### Scenario: Multi-file compilation
Given project with multiple files
When compiling
Then files are compiled in dependency order

#### Scenario: Incremental compilation
Given previously compiled project with one changed file
When rebuilding
Then only changed file and dependents are recompiled

#### Scenario: Parallel compilation
Given multi-file project
When compiling with multiple cores
Then independent files are compiled in parallel

### Requirement: Error Reporting
All errors MUST be reported with helpful context.

#### Scenario: Syntax error
Given source file with syntax error
When compiling
Then error shows file, line, column, and context

#### Scenario: Type error
Given source file with type error
When compiling
Then error shows expected vs found types

#### Scenario: Multiple errors
Given source file with multiple errors
When compiling
Then all errors are reported (not just first)

#### Scenario: Colored output
Given terminal supporting colors
When errors are displayed
Then they use colors for readability

#### Scenario: Error summary
Given compilation with errors
When compilation finishes
Then summary shows "N errors, M warnings"

### Requirement: Exit Codes
The CLI MUST use meaningful exit codes.

#### Scenario: Success
Given successful compilation
When CLI exits
Then exit code is 0

#### Scenario: Compilation error
Given compilation with errors
When CLI exits
Then exit code is 1

#### Scenario: Invalid arguments
Given invalid command-line arguments
When CLI exits
Then exit code is 2

#### Scenario: Internal error
Given internal compiler error
When CLI exits
Then exit code is 101

### Requirement: Build Modes
The CLI SHALL support different build modes.

#### Scenario: Debug mode
Given `tml build` (default)
When building
Then debug info is included, minimal optimization

#### Scenario: Release mode
Given `tml build --release`
When building
Then full optimization, no debug assertions

#### Scenario: Test mode
Given `tml test`
When building
Then test harness is included, test functions are exported

### Requirement: Progress Reporting
The CLI SHOULD report compilation progress.

#### Scenario: Compiling message
Given compilation in progress
When a file starts compiling
Then "Compiling filename.tml" is shown

#### Scenario: Linking message
Given linking in progress
When linking starts
Then "Linking project_name" is shown

#### Scenario: Finished message
Given successful compilation
When compilation completes
Then "Finished in X.XXs" is shown

#### Scenario: Quiet mode
Given `--quiet` flag
When compiling
Then only errors and warnings are shown
