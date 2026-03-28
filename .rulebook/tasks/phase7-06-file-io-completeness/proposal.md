# Proposal: File I/O Completeness

## Why
The current file module only handles text reads and writes. Binary file I/O, directory listing, recursive deletion, and file metadata are all absent, which blocks any file-processing or build-tool program.

## What Changes
Add binary read/write methods to `File`, a `DirEntry` type with `read_dir`, `remove_dir_all`, a `FileMetadata` type, and a `BufWriter` abstraction, backed by new C runtime helpers where needed.

## Impact
- Affected specs: std::file
- Affected code: lib/std/src/file/, lib/std/runtime/file.c
- Breaking change: NO
- User benefit: Programs can process binary files, list directories, and query file metadata without manual FFI
