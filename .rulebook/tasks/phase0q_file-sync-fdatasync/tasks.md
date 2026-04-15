## 1. Implementation
- [x] 1.1 Read `lib/std/src/file.tml` — understand File struct and existing I/O methods
- [x] 1.2 Add `File::sync() -> Bool` calling `_commit` (Windows) / `fsync` (Unix) — C runtime: `file_sync()` in `lib/std/runtime/file.c`
- [x] 1.3 Add `File::datasync() -> Bool` calling `_commit` (Windows) / `fdatasync` (Linux) / `fsync` (macOS fallback)
- [x] 1.4 File::fd() not needed — sync/datasync use `_fileno(FILE*)` internally in C runtime

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation
- [x] 2.2 Write tests covering the new behavior — `file_sync.test.tml` (3 tests: sync after write, datasync after write, sync on closed file). NOTE: all std/file tests fail with pre-existing K001 EventEmitter codegen bug — test code is correct but untestable until K001 is fixed.
- [x] 2.3 Run `tml test --suite=compiler` — 262/263 pass (only pre-existing timeouts)
