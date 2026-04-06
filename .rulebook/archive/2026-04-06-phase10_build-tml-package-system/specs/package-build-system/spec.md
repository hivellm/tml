# Spec Delta: Package Build System

## ADDED Requirements

### Requirement: Build Script Detection
The compiler SHALL detect a `build.tml` file at the root of any package being compiled. When present, the compiler MUST compile and execute the build script before compiling the package's `src/` directory.

#### Scenario: Package with build.tml
Given a package at `lib/postgresql/` with a `build.tml` file
When the compiler resolves imports from `postgresql::*`
Then the compiler SHALL compile `lib/postgresql/build.tml` to a temporary executable
And execute it as a subprocess
And parse its stdout for `tml:` directives

#### Scenario: Package without build.tml
Given a package at `lib/core/` without a `build.tml` file
When the compiler compiles the package
Then the compiler SHALL skip the build script phase entirely
And compilation SHALL proceed as before (no behavioral change)

### Requirement: Build Directive Protocol
The build script MUST communicate with the compiler via stdout lines prefixed with `tml:`. Each directive SHALL follow the format `tml:<directive-name>=<value>`. Lines without the `tml:` prefix SHALL be ignored (treated as normal program output).

#### Scenario: Parse link-lib directive
Given a build script that prints `tml:link-lib=pq`
When the compiler parses the build script output
Then the linker SHALL receive `-lpq` as a link flag

#### Scenario: Parse link-search directive
Given a build script that prints `tml:link-search=native/win-x64`
When the compiler parses the build script output
Then the linker SHALL receive `-L <package_dir>/native/win-x64` as a search path
And the path SHALL be resolved relative to the package root directory

#### Scenario: Parse copy-artifact directive
Given a build script that prints `tml:copy-artifact=native/win-x64/libpq.dll`
When linking succeeds
Then the compiler SHALL copy `<package_dir>/native/win-x64/libpq.dll` to the output directory where the executable was placed

#### Scenario: Parse warning directive
Given a build script that prints `tml:warning=libpq version 15 detected`
When the compiler parses the build script output
Then the warning message SHALL be printed to stderr during build

#### Scenario: Parse cfg directive
Given a build script that prints `tml:cfg=HAS_LIBPQ`
When the compiler compiles the package source
Then `HAS_LIBPQ` SHALL be defined as a conditional compilation symbol (usable with `#ifdef HAS_LIBPQ`)

#### Scenario: Parse rerun-if-changed directive
Given a build script that prints `tml:rerun-if-changed=native/`
When the compiler checks the build cache
Then the build script SHALL be re-executed if any file under `native/` has a newer modification time than the cached result

#### Scenario: Unknown directive
Given a build script that prints `tml:unknown-directive=value`
When the compiler parses the build script output
Then the unknown directive SHALL be ignored without error

#### Scenario: Normal stdout output
Given a build script that prints `Detecting platform...` (no `tml:` prefix)
When the compiler parses the build script output
Then the line SHALL be ignored (not treated as a directive)

### Requirement: Linker Search Path Resolution
The linker MUST support search paths (`-L <path>`) in addition to library names (`-l<name>`). Search paths from build scripts SHALL be added before library flags in the linker invocation.

#### Scenario: Link with search path and library name
Given a build script emits `tml:link-search=native/win-x64` and `tml:link-lib=libpq`
When the linker is invoked
Then the link command SHALL include `-L <pkg>/native/win-x64 -llibpq` in that order

#### Scenario: Absolute vs relative search paths
Given a build script emits `tml:link-search=native/win-x64`
When the compiler resolves the search path
Then the path SHALL be made absolute by prepending the package root directory

### Requirement: Post-Link Artifact Copying
After successful linking, the compiler SHALL copy all files declared via `tml:copy-artifact=` to the output directory. If a declared artifact file does not exist, the compiler SHALL emit a warning but SHALL NOT fail the build.

#### Scenario: DLL copied to output
Given `tml:copy-artifact=native/win-x64/libpq.dll` and linking produces `build/debug/bin/app.exe`
When linking succeeds
Then `lib/postgresql/native/win-x64/libpq.dll` SHALL be copied to `build/debug/bin/libpq.dll`

#### Scenario: Missing artifact
Given `tml:copy-artifact=native/win-x64/missing.dll` and the file does not exist
When linking succeeds
Then the compiler SHALL emit a warning: `build script artifact not found: native/win-x64/missing.dll`
And the build SHALL NOT fail

### Requirement: Build Script Caching
Build script results SHALL be cached. The build script SHALL only be re-executed when files listed in `tml:rerun-if-changed=` directives have been modified since the last execution.

#### Scenario: Cached build script
Given a build script was previously executed and its output cached
And no files listed in `tml:rerun-if-changed=` have been modified
When the package is compiled again
Then the compiler SHALL use the cached directives without re-executing the build script

#### Scenario: Invalidated cache
Given a build script emitted `tml:rerun-if-changed=native/`
And a file in `native/` has been modified since the last build
When the package is compiled again
Then the compiler SHALL re-execute the build script

### Requirement: Native Directory Convention
Packages with native dependencies SHALL follow the directory convention `native/<platform>/` where platform MUST be one of: `win-x64`, `linux-x64`, `linux-arm64`, `macos-x64`, `macos-arm64`. Build scripts MUST use `#if WINDOWS`/`#if LINUX`/`#if MACOS` conditional compilation to select the correct platform subdirectory.

#### Scenario: Platform-specific native libraries
Given a package with `native/win-x64/libpq.lib` and `native/linux-x64/libpq.a`
And a build script that uses `#if WINDOWS` to select `native/win-x64`
When compiled on Windows
Then only `native/win-x64/` SHALL be added to the linker search path
