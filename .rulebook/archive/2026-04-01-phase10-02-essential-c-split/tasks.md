# Tasks: Split essential.c by Isolation Level

**Status**: Done — 12/12
**Priority**: Low
**Risk**: LOW (each extraction moves only private statics, no linkage changes)
**Result**: essential.c 1,667 → 1,344 lines (5 sections extracted, ~320 lines moved)

## Phase 1: Extract Tracy profiler — 2/2

- [x] 1.1 Create `compiler/runtime/core/essential_tracy.c`: moved `tml_zone_stack[256]`, `tml_zone_top`, and all `tml_tracy_*` functions
- [x] 1.2 Add `essential_tracy.c` to `TML_RUNTIME_SOURCES` in CMakeLists.txt, build, test

## Phase 2: Extract UTF-8 encoding — 2/2

- [x] 2.1 Create `compiler/runtime/core/essential_utf8.c`: moved `utf8_char_buffer[8]` + `utf8_2byte_to_string`, `utf8_3byte_to_string`, `utf8_4byte_to_string`
- [x] 2.2 Add to CMakeLists.txt, build, test

## Phase 3: Extract random seed — 2/2

- [x] 3.1 Create `compiler/runtime/core/essential_random.c`: moved `tml_seed_counter` + `tml_random_seed`
- [x] 3.2 Add to CMakeLists.txt, build, test

## Phase 4: Extract tml_str_free + image ranges — 2/2

- [x] 4.1 Create `compiler/runtime/memory/str_free.c`: moved `tml_image_ranges[128]`, `tml_image_range_count`, `tml_image_ranges_initialized`, `tml_str_free`, `tml_str_free_register_module`, `tml_is_image_ptr`
- [x] 4.2 Add to CMakeLists.txt + builder_helpers_runtime.cpp, build, test

## Phase 5: Extract FFI utils — 2/2

- [x] 5.1 Create `compiler/runtime/core/essential_ffi.c`: moved `tml_str_from_cstr`, `tml_free` (placed in core/, not ffi/)
- [x] 5.2 Add to CMakeLists.txt + builder_helpers_runtime.cpp, build, test

## Phase 6: Verify — 2/2

- [x] 6.1 essential.c final size: 1,344 lines (above original ~900 target — remaining code is tightly-coupled panic/crash/VEH/I/O/test harness that cannot be split without extern linkage changes)
- [x] 6.2 Added header comment documenting extracted sections and Group C static constraint

## Notes

- **builder_helpers_runtime.cpp** also needed updating — it lists runtime .c files explicitly for test linking (separate from CMakeLists.txt)
- Also added missing entries for `essential_cpuid.c`, `pool.c` which were extracted in prior phases but never added to builder_helpers_runtime.cpp
- FFI utils placed in `core/essential_ffi.c` instead of proposed `ffi/ffi_utils.c` (no ffi/ directory existed, simpler to keep in core/)
