# Tasks: std::env Module

**Status**: Proposed
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: Environment variable access and process info
- [ ] 1.1 Add `tml_getenv`, `tml_setenv`, `tml_unsetenv` to C runtime
- [ ] 1.2 Add `tml_getcwd`, `tml_temp_dir` to C runtime
- [ ] 1.3 Create `lib/std/src/env.tml`
- [ ] 1.4 `var(name: Str) -> Maybe[Str]` — get env var
- [ ] 1.5 `set_var(name: Str, value: Str)` — set env var
- [ ] 1.6 `remove_var(name: Str)` — unset env var
- [ ] 1.7 `current_dir() -> Str` — get CWD
- [ ] 1.8 `temp_dir() -> Str` — get temp directory
- [ ] 1.9 `args() -> List[Str]` — command line arguments
- [ ] 1.10 Tests
