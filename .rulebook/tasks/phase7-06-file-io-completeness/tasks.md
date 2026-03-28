# Tasks: File I/O Completeness

**Status**: Proposed
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: Binary I/O
- [ ] 1.1 `File::read_bytes(this, count: I64) -> Buffer` — read binary data
- [ ] 1.2 `File::write_bytes(this, data: Buffer) -> I64` — write binary data
- [ ] 1.3 `File::read_all_bytes(path: Str) -> Buffer` — static binary read
- [ ] 1.4 `File::write_all_bytes(path: Str, data: Buffer) -> Bool` — static binary write
- [ ] 1.5 Tests

## Phase 2: Directory ops
- [ ] 2.1 `read_dir(path: Str) -> List[DirEntry]` — list directory
- [ ] 2.2 `DirEntry` type — name, path, is_file, is_dir
- [ ] 2.3 `remove_dir_all(path: Str) -> Bool` — recursive delete
- [ ] 2.4 Tests

## Phase 3: Metadata
- [ ] 3.1 `metadata(path: Str) -> FileMetadata` — size, modified, permissions
- [ ] 3.2 `FileMetadata` type
- [ ] 3.3 `BufWriter` — buffered file writer
- [ ] 3.4 Tests
