# Contributing to TML

Thank you for your interest in contributing to TML! This guide covers how to set up your development environment, submit changes, and follow project conventions.

## Getting Started

### Prerequisites

- **Zig 0.14+** (preferred C/C++ toolchain — see [ADR-007](docs/decisions/))
- **CMake 3.20+**
- **Ninja** (required for Zig CC builds)
- **LLVM 15+** (pre-built static libraries)

### Building the Compiler

```bash
# Windows
scripts\build.bat              # Debug build (default)
scripts\build.bat release      # Release build (2.93x faster DLL compilation)

# Linux/Mac
./scripts/build.sh debug
./scripts/build.sh release
```

**Never run `cmake` directly** — always use the build scripts. `CMakeLists.txt` expects variables set by the scripts.

### Running Tests

```bash
tml test --suite=compiler      # Compiler test suite
tml test --suite=core          # Core library tests
tml test --suite=std           # Standard library tests
tml test --coverage            # With coverage report
```

## How to Contribute

### Reporting Issues

- Use GitHub Issues for bug reports and feature requests.
- Include the TML version (`tml --version`), OS, and a minimal reproduction.
- For compiler crashes, include the `.tml` source file and the full error output.

### Submitting Pull Requests

1. Fork the repository and create a branch from `main`.
2. Make your changes following the conventions below.
3. Ensure all tests pass: `tml test --suite=compiler` + `tml test --suite=core`.
4. Run formatting: `tml fmt <files>`.
5. Submit a PR with a clear description of what and why.

### Commit Message Format

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <description>

[optional body]
```

**Types**: `feat`, `fix`, `refactor`, `docs`, `test`, `perf`, `chore`, `build`, `ci`
**Scopes**: `std`, `core`, `compiler`, `codegen`, `mcp`, `cli`, `test`, `docs`

Examples:
- `feat(std): add File::sync() and File::datasync()`
- `fix(codegen): resolve i1/i32 type mismatch in bool comparisons`
- `perf(compiler): emit LLVM select for branchless if-else`

### Code Style

**TML code** (`.tml` files):
- Use `tml fmt` before committing.
- Use `when` for pattern matching (not `match`).
- Use `for i in 0 to N` instead of manual index loops.
- Use `let-else` for flat Maybe unwrapping instead of nested `when`.
- Use `?.` optional chaining for Maybe method calls.

**C++ code** (compiler):
- C++20 standard.
- `clang-format` enforced via pre-commit hook.
- Minimize new C/C++ code — prefer pure TML implementations.

### Architecture

See [docs/ARCHITECTURE-MAP.md](docs/ARCHITECTURE-MAP.md) for the full compiler architecture.

Key directories:
- `compiler/src/` — C++ compiler source
- `lib/core/src/` — Core library (traits, primitives, collections)
- `lib/std/src/` — Standard library (file, net, crypto, json, etc.)
- `compiler-tml/src/` — Self-hosted compiler modules (in TML)

### Testing Guidelines

- All new features need tests.
- Tests use the `@test` decorator and `assert_eq`/`assert_true` from the `test` module.
- Test files go in the appropriate `tests/` directory next to the source.
- Temporary/scratch files during testing go in `.sandbox/` (never in project root).

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).
