# Tasks: File I/O Completeness

**Status**: Complete
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: Binary I/O
- [x] 1.1 `File::read_bytes(this, count: I64) -> Buffer` — read binary data
- [x] 1.2 `File::write_bytes(this, data: Buffer) -> I64` — write binary data
- [x] 1.3 `File::read_all_bytes(path: Str) -> Buffer` — static binary read
- [x] 1.4 `File::write_all_bytes(path: Str, data: Buffer) -> Bool` — static binary write
- [x] 1.5 Tests — `lib/std/tests/file/binary_io.test.tml` (3 tests passing)

## Phase 2: Directory ops
- [x] 2.1 `Dir::read_dir(path: Str) -> List[DirEntry]` — list directory
- [x] 2.2 `DirEntry` type — name, path, is_file, is_dir fields
- [x] 2.3 `Dir::remove_all(path: Str) -> Bool` — recursive delete
- [x] 2.4 Tests — `lib/std/tests/file/dir_ops.test.tml` (3 tests passing)

## Phase 3: Metadata
- [x] 3.1 `File::metadata(path: Str) -> FileMetadata` — size, modified, is_readonly
- [x] 3.2 `FileMetadata` type — size (I64), modified (I64 unix ts), is_readonly (Bool)
- [x] 3.3 `BufWriter` — already existed in `lib/std/src/file/bufio.tml`
- [x] 3.4 Tests — `lib/std/tests/file/metadata.test.tml` (2 tests passing)

## Implementation Notes
- C FFI functions added to `lib/std/runtime/file.c` and `file.h`
- Binary I/O uses temp `mem_alloc` buffer + `Buffer::write_from_ptr` pattern (same as HTTP module)
- Directory listing uses `FindFirstFile`/`FindNextFile` on Windows, `opendir`/`readdir` on POSIX
- `remove_all` is recursive via `path_remove_dir_all` in C runtime
- Metadata uses `stat()` on both platforms, `GetFileAttributesA` for Windows readonly check
- All new types re-exported from `lib/std/src/file/mod.tml`
