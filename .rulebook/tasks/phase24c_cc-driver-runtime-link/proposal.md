# Proposal: phase24c_cc-driver-runtime-link

## Why

`tml build compiler-tml/src/cc/bin/cc_driver.tml -o build/debug/bin/cc_driver.exe`
fails at link time with:

```
lld-link: error: undefined symbol: file_read_all
lld-link: error: undefined symbol: file_seek_from
lld-link: error: undefined symbol: file_read_line
lld-link: error: undefined symbol: path_exists
lld-link: error: undefined symbol: path_parent
lld-link: error: undefined symbol: path_join
```

`cc_driver.tml` imports `use std::file::File`. The C implementations of
those symbols exist in `lib/std/runtime/file.c`:

- `file_read_all` — line 107
- `file_read_line` — line 132
- `file_seek_from` — line 299
- `path_exists` — line 712
- `path_join` — line 830
- `path_parent` — line 860

`compiler/src/cli/builder/builder_helpers_runtime.cpp::get_runtime_objects`
gates the file.c link on `registry->has_module("std::file")` (line 421)
or any submodule path starting with `std::file::` (line 424). For
`cc_driver.tml` neither check fires for the `tml build` flow even though
the same import resolves correctly through `tml run` (a smoke
`.sandbox/file_smoke.tml` calling `File::read_all` runs successfully —
file.c gets linked as expected when invoked through `tml run --stage=parser:cpp`).

The phase24b session traced the divergence to the build path: the
incremental cache GREEN path (`incr/incr.bin` reuse) reuses a
codegen-only result that did not seed the registry with `std::file`
when the `tml build` was first run on cc_driver.tml. Even after wiping
`incr/incr.bin` the link error reproduces, so the gap is in the
registry / runtime-collection step, not the cache.

This blocks phase24b items 4.1 and 4.2 (end-to-end `tml cc` verification)
and any further work on the self-compiled cc_driver.exe path.

## What Changes

1. Add a build-side diagnostic that logs which `std::*` modules
   `get_runtime_objects` sees in the registry for the binary being
   built. Run that diagnostic on the failing `tml build cc_driver.tml`
   to identify whether `std::file` is missing from the registry, or
   present under a different path/key.
2. Repair the registration so `tml build` populates the same module
   set that `tml run` does — likely a missing call to
   `preload_all_meta_caches` or an import-walker step in the build
   pipeline.
3. Verify by building `cc_driver.exe` end to end and running
   `tml cc .sandbox/test_no_inc.c --emit=ast` (= phase24b item 4.1).
4. Run `tml cc compiler/runtime/core/essential.c` (= phase24b item 4.2)
   and document the next limitation surfaced by the larger source.

## Impact

- Affected specs: none (build-pipeline correctness, no language change).
- Affected code:
  - `compiler/src/cli/builder/builder_helpers_runtime.cpp` —
    `uses_file_module` (and likely the other `uses_*_module` helpers)
    or the registry initialization path that feeds it.
  - `compiler/src/cli/builder/build.cpp` — possibly the registry
    seeding before `get_runtime_objects` runs.
- Breaking change: NO.
- User benefit: re-enables building `cc_driver.exe` from
  `cc_driver.tml`, which unblocks `tml cc <file.c>` end-to-end and
  the larger phase24 self-compile pipeline (essential.c, mem.c, …).
