# Proposal: std::env Module

## Why
There is currently no way to read environment variables, get the current working directory, or access command-line arguments from TML. Every real CLI program depends on at least one of these capabilities.

## What Changes
Add C runtime helpers (`tml_getenv`, `tml_setenv`, `tml_unsetenv`, `tml_getcwd`, `tml_temp_dir`) and create a new `lib/std/src/env.tml` module that exposes a clean TML API over them.

## Impact
- Affected specs: std::env (new)
- Affected code: lib/std/src/env.tml (new), compiler/runtime/core/essential.c or a new runtime/env.c
- Breaking change: NO
- User benefit: CLI programs can read env vars, CWD, temp dir, and argv without dropping to lowlevel FFI manually
