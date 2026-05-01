## 1. Diagnose
- [ ] 1.1 Add a debug-level log in `get_runtime_objects` that dumps every module the registry contains, just before the `uses_file_module` / `uses_*_module` checks.
- [ ] 1.2 Run `tml build compiler-tml/src/cc/bin/cc_driver.tml -o build/debug/bin/cc_driver.exe --stage=parser:cpp` with debug logging on and capture the registry contents.
- [ ] 1.3 Compare with the registry that `tml run .sandbox/file_smoke.tml --stage=parser:cpp` produces (where the link succeeds).

## 2. Fix
- [ ] 2.1 Restore the missing registry seeding step in `tml build` so `std::file` (and any other transitively imported runtime-bearing module) is registered before `get_runtime_objects` runs. Likely an import-walker pass or a `preload_all_meta_caches` invocation that exists in `tml run` but not the build path.
- [ ] 2.2 Verify the link command pulls `lib/std/runtime/file.c` (look for the `Compiling: file.c -> file_*.obj` log line during the build).
- [ ] 2.3 Build `cc_driver.exe` from a clean cache and confirm it runs (`./build/debug/bin/cc_driver.exe --help` exits 0).

## 3. Self-compile gate (was phase24b items 4.1 / 4.2)
- [ ] 3.1 `tml cc .sandbox/test_no_inc.c --emit=ast` exits 0 with `cc_driver: parsed`.
- [ ] 3.2 `tml cc compiler/runtime/core/essential.c -I compiler/runtime/include/c-stdlib --emit=ast` reaches the next limitation (no longer crashes at the typedef point — phase24b's value-pass crash already fixed).
- [ ] 3.3 Document any subsequent gaps as separate task entries.

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 4.1 Update or create documentation covering the implementation
- [ ] 4.2 Write tests covering the new behavior
- [ ] 4.3 Run tests and confirm they pass
