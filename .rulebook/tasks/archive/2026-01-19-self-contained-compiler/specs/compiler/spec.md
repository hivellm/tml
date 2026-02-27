# Compiler Backend Specification

## ADDED Requirements

### Requirement: Built-in LLVM Backend
The compiler SHALL provide a built-in LLVM backend that compiles LLVM IR to native object files without requiring external tools.

#### Scenario: Compile IR to object file
Given a valid LLVM IR file (.ll)
When the compiler processes the IR with the built-in backend
Then a native object file (.obj/.o) is produced without spawning clang

#### Scenario: Optimization levels
Given an LLVM IR file and an optimization level (-O0, -O1, -O2, -O3)
When the compiler compiles the IR
Then the specified optimization passes are applied to the generated code

#### Scenario: Debug info emission
Given an LLVM IR file with debug info and the --debug flag
When the compiler compiles the IR
Then the object file contains DWARF/CodeView debug information

### Requirement: Built-in LLD Linker
The compiler SHALL provide a built-in LLD linker that links object files into executables and shared libraries without requiring external linkers.

#### Scenario: Link executable on Windows
Given a set of object files and the Windows target
When the linker produces an executable
Then a valid PE/COFF executable is created without spawning link.exe or lld-link

#### Scenario: Link executable on Unix
Given a set of object files and a Unix target (Linux/macOS)
When the linker produces an executable
Then a valid ELF/Mach-O executable is created without spawning ld or lld

#### Scenario: Link shared library
Given a set of object files and a library target
When the linker produces a shared library
Then a valid DLL/SO/dylib is created with exported symbols

### Requirement: Pre-compiled C Runtime
The compiler MUST bundle pre-compiled C runtime objects that were compiled at build time, not at user runtime.

#### Scenario: Runtime available without clang
Given a fresh system without clang installed
When the user runs any tml command that requires runtime
Then the compiler uses bundled runtime objects without attempting to compile C files

#### Scenario: No C compilation warnings
Given a user running any tml command
When code is compiled and linked
Then no warnings from C file compilation appear in the output

### Requirement: Complete CLI Self-Containment
The compiler MUST be fully self-contained for ALL CLI commands after installation, requiring no external tools.

#### Scenario: tml build without external tools
Given a system with only tml.exe installed (no clang, no Visual Studio)
When the user runs `tml build` on a TML project
Then the project compiles to a working executable

#### Scenario: tml run without external tools
Given a system with only tml.exe installed
When the user runs `tml run` on a TML file
Then the code executes successfully

#### Scenario: tml test without external tools
Given a system with only tml.exe installed
When the user runs `tml test` on a TML project
Then all tests compile, execute, and report results

#### Scenario: tml lint without external tools
Given a system with only tml.exe installed
When the user runs `tml lint` on TML source files
Then linting runs using built-in analyzer only

#### Scenario: tml format without external tools
Given a system with only tml.exe installed
When the user runs `tml format` on TML source files
Then formatting runs using built-in formatter only

#### Scenario: Fresh Windows installation
Given a Windows system with only tml.exe installed
When the user runs any tml command (build, run, test, lint, format)
Then the command executes successfully without errors about missing tools

#### Scenario: Fresh Linux installation
Given a Linux system with only the tml binary installed
When the user runs any tml command (build, run, test, lint, format)
Then the command executes successfully without errors about missing tools

#### Scenario: No subprocess spawning for compilation
Given any TML compilation operation
When the compiler generates object files and links them
Then no external processes (clang, gcc, link.exe, ld) are spawned
