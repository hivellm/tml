# TML Language Completeness Specification

## ADDED Requirements

### Requirement: Milestone Gate System
The roadmap MUST enforce milestone gates where each milestone's prerequisites are met before proceeding to the next milestone.

#### Scenario: Milestone 1 gate enforcement
Given Milestone 1 requires library coverage >= 70% and stdlib-essentials working
When a developer attempts to start Milestone 3 (Async & Networking)
Then the system SHALL verify that M1 gate criteria are satisfied first

#### Scenario: Dependency ordering
Given Milestone 4 (Web & HTTP) depends on Milestone 3 (Async & Networking)
When Milestone 3 is not yet complete
Then Milestone 4 work SHALL NOT be started until M3 gate is passed

### Requirement: Foundation Standard Library
The TML standard library MUST provide essential utilities for standalone programs including collections, environment access, path manipulation, date/time, buffered I/O, and regular expressions.

#### Scenario: Collections completeness
Given the standard library has List[T] and HashMap[K,V]
When a developer needs ordered collections or sets
Then HashSet[T], BTreeMap[K,V], BTreeSet[T], and Deque[T] MUST be available

#### Scenario: Environment access
Given a CLI tool needs to read environment variables and command-line arguments
When the program imports std::env and std::args
Then the program SHALL be able to read env vars, parse arguments, and access the current directory

#### Scenario: File system operations
Given a program needs to manipulate file paths and directories
When the program imports std::path
Then Path and PathBuf types SHALL support join, parent, extension, exists, and cross-platform normalization

#### Scenario: Date and time
Given a program needs to work with dates and times
When the program imports std::time
Then DateTime, Instant, and SystemTime types SHALL be available with formatting, parsing, and arithmetic

### Requirement: Async Runtime
The TML runtime MUST provide a multi-threaded async executor capable of handling 10,000+ concurrent connections with sub-microsecond task switching.

#### Scenario: Async function compilation
Given a function is declared with `async func`
When the compiler processes this function
Then it SHALL generate a proper state machine that can be suspended and resumed at await points

#### Scenario: TCP echo server
Given an async TCP echo server is running
When 10,000 clients connect concurrently and send data
Then the server SHALL echo all data back correctly without deadlocks or data corruption

#### Scenario: Work-stealing scheduler
Given a multi-core system with N CPU cores
When async tasks are submitted to the executor
Then the scheduler SHALL distribute tasks across all cores with near-linear scaling

### Requirement: HTTP Framework
The TML HTTP framework MUST support HTTP/1.1 and HTTP/2 with a decorator-based routing system inspired by NestJS.

#### Scenario: Decorator-based routing
Given a controller class with @Get("/users/:id") decorated method
When an HTTP GET request arrives at /users/42
Then the framework SHALL route to the correct handler with id=42 extracted

#### Scenario: Middleware pipeline
Given middleware for logging, authentication, and CORS is configured
When a request passes through the pipeline
Then each middleware SHALL execute in order and be able to short-circuit the chain

#### Scenario: HTTPS support
Given TLS certificates are configured on the server
When a client connects via HTTPS
Then the TLS handshake SHALL complete successfully with ALPN negotiation for HTTP/2

### Requirement: IDE Integration
The TML ecosystem MUST provide a VSCode extension with Language Server Protocol support for professional development experience.

#### Scenario: Autocomplete
Given a developer types a variable name followed by a dot
When the LSP processes the completion request
Then it SHALL return available methods and fields for that type within 100ms

#### Scenario: Error diagnostics
Given a TML source file has type errors
When the file is saved in VSCode
Then red squiggly underlines SHALL appear at the error locations with descriptive messages

#### Scenario: Go-to-definition
Given a developer clicks on a function call while holding Ctrl
When the LSP processes the definition request
Then the editor SHALL navigate to the function's definition source, even in library modules

### Requirement: Cross-Compilation
The TML compiler MUST support cross-compilation to at least 6 target platforms from any host.

#### Scenario: Windows to Linux cross-compilation
Given the compiler runs on Windows x86_64
When `tml build --target x86_64-unknown-linux-gnu` is executed
Then the compiler SHALL produce a valid Linux ELF executable

#### Scenario: WebAssembly target
Given a TML source file with no platform-specific code
When `tml build --target wasm32-unknown-unknown` is executed
Then the compiler SHALL produce a valid WebAssembly module

### Requirement: Package Management
The TML package manager MUST support dependency resolution, version management, and a central package registry.

#### Scenario: Adding a dependency
Given a project with tml.toml manifest
When `tml add some-package@1.2.0` is executed
Then the package SHALL be downloaded, added to tml.toml, and its dependencies resolved transitively

#### Scenario: Reproducible builds
Given a project has a tml.lock file
When the project is built on a different machine
Then the exact same dependency versions SHALL be used as specified in the lock file

### Requirement: Overall Completeness Target
The TML language MUST achieve production-readiness by completing all 6 milestones with a total of 217 task items.

#### Scenario: Production readiness assessment
Given all 6 milestones are completed
When the language is evaluated for production use
Then it SHALL score at least 8/10 on compiler, library, runtime, tooling, and ecosystem dimensions
