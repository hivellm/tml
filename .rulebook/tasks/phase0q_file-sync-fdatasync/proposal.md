# Proposal: phase0q_file-sync-fdatasync

## Why

`std::file::File` has no way to call `fsync` or `fdatasync` on an open file. This
is a blocker for any code that requires durability guarantees — databases (WAL),
configuration file writes, and any safety-critical persistence. Without `fsync`,
`File::write_all` + `close` only writes to the OS page cache; a power failure before
the OS flushes can lose committed data. Reported by an external AI agent (UzDB
author) as a P2 gap for their database WAL implementation. Standard in every
systems language (Rust: `File::sync_all`, Go: `File.Sync`, C: `fsync(fd)`).

## What Changes

- `lib/std/src/file.tml`: add `File::sync()` (calls `fsync`) and
  `File::datasync()` (calls `fdatasync`; falls back to `fsync` on Windows which
  has no `fdatasync`).
- C runtime: add `tml_file_sync(fd: I64) -> I32` and `tml_file_datasync(fd: I64) -> I32`
  to the file runtime if needed, or use `@extern("c")` directly on `_commit`/`fsync`.
- Both return `Outcome[Unit, IOError]`.

## Impact

- Affected specs: `lib/std/src/file.tml`
- Affected code: file stdlib + possibly a thin C runtime shim
- Breaking change: NO (additive)
- User benefit: Database WAL, config file writes, and any durability-sensitive
  code can guarantee data reaches disk.
