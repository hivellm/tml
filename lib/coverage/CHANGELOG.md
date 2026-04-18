# Changelog — TML Coverage Library (`lib/coverage`)

All notable changes to the TML coverage library are recorded here.

## [0.1.0] — 2026-04-17

### Added
- Library scaffold: `package.toml`, `tml.toml`, `README.md`, `LICENSE` (Apache-2.0), `CHANGELOG.md`.
- Directory layout: `src/{ingest,emit,template,bin}/`, `tests/fixtures/`, `docs/`.
- Public API surface declared in `src/mod.tml` (signatures only, no implementations yet).
- Core types declared in `src/types.tml`: `LineHit`, `FuncHit`, `BranchHit`, `FileCoverage`, `CoverageReport`, `Summary`, `IngestError`, `EmitError`, `CliArgs`.

### Notes
- Corresponds to phase 1 of rulebook task `phase0w_coverage-tml-library`.
- Implementations will land incrementally across phases 2–11.
