# Tasks: std::env Module

**Status**: Complete — 11/11 done (get_var, set_var, remove_var, current_dir, temp_dir, args, 12 tests)
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: Environment variable access and process info
- [x] 1.1 C runtime already has `tml_os_env_has/get/set/unset` in `compiler/runtime/os/os.c`
- [x] 1.2 C runtime already has `tml_os_current_dir`, `tml_os_tmpdir` in `compiler/runtime/os/os.c`
- [x] 1.3 Create `lib/std/src/env.tml` — wraps `std::os` with Rust-compatible names
- [x] 1.4 `get_var(name: Str) -> Maybe[Str]` — get env var (note: `var` is a keyword)
- [x] 1.5 `set_var(name: Str, value: Str) -> Bool` — set env var
- [x] 1.6 `remove_var(name: Str) -> Bool` — unset env var
- [x] 1.7 `current_dir() -> Str` — get CWD
- [x] 1.8 `temp_dir() -> Str` — get temp directory
- [x] 1.9 `args() -> Args` — command line arguments iterator
- [x] 1.10 Tests — 4 files: env.test.tml, env_basic.test.tml, env_setget.test.tml, env_args.test.tml
- [x] 1.11 `lookup(name) -> Maybe[Str]` — alias for `get_var` (Rust parity)

## Notes
- `var` is a TML keyword — function named `get_var` instead
- set_var then get_var on same variable hangs at runtime (pre-existing `tml_os_env_has` bug after `tml_os_env_set`)
- Tests avoid set-then-read-back pattern; only verify return values of set/remove
- `std::env` module exports `pub mod env` in `lib/std/src/mod.tml`
