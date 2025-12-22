# Changelog - TML Standard Library

All notable changes to the TML standard library.

## [Unreleased]

### To Implement
- Complete Vec[T] implementation with dynamic resizing
- HashMap and HashSet implementations
- File system operations (fs module)
- Network operations (net module)
- Thread and sync primitives
- Process management
- String manipulation functions
- Path operations

## [0.1.0] - 2025-01-01

### Added
- Core types: `Maybe[T]`, `Outcome[T, E]`
- Error handling: `Error` trait, `IoError`, `ParseError`
- Collections: `Vec[T]` (partial), `Str`
- I/O: `Read`, `Write` traits, `print`, `println`
- Time: `Duration`, `Instant`
- Prelude with auto-imported types

### Implementation Status
- ✅ Package structure and organization
- ✅ Core types (Maybe, Outcome)
- ✅ Error types
- ✅ Basic collections types
- ✅ I/O traits
- ✅ Time types
- ⚠️ Vec implementation (partial)
- ⚠️ String operations (partial)
- ❌ HashMap/HashSet
- ❌ File system
- ❌ Networking
- ❌ Concurrency primitives
