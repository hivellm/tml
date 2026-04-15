## 1. Implementation
- [ ] 1.1 Read `lib/std/src/file.tml` — understand File struct and existing I/O methods
- [ ] 1.2 Add `File::sync() -> Outcome[Unit, IOError]` calling `_commit` (Windows) / `fsync` (Unix)
- [ ] 1.3 Add `File::datasync() -> Outcome[Unit, IOError]` calling `_commit` (Windows) / `fdatasync` (Unix)
- [ ] 1.4 Expose `File::fd() -> I64` if not already present (needed for direct C FFI calls)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update CHANGELOG.md and bump VERSION
- [ ] 2.2 Write tests: write to file, call sync(), verify Outcome is Ok
- [ ] 2.3 Run `tml test --suite=compiler` and `tml test --suite=std` — confirm no regressions
