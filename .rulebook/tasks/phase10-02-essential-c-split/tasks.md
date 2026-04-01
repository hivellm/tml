# Tasks: Split essential.c by Isolation Level

**Status**: Proposed — 0/12
**Priority**: Low
**Risk**: LOW (each extraction moves only private statics, no linkage changes)
**Target**: essential.c 1,667 → ~900 lines

## Phase 1: Extract Tracy profiler — 0/2

- [ ] 1.1 Create `compiler/runtime/core/essential_tracy.c`: move `tml_zone_stack[256]`, `tml_zone_top`, and all `tml_tracy_*` functions (~65 lines, gated by `#ifdef TML_PROFILE`)
- [ ] 1.2 Add `essential_tracy.c` to `TML_RUNTIME_SOURCES` in CMakeLists.txt, build, test

## Phase 2: Extract UTF-8 encoding — 0/2

- [ ] 2.1 Create `compiler/runtime/core/essential_utf8.c`: move `utf8_char_buffer[8]` + `utf8_2byte_to_string`, `utf8_3byte_to_string`, `utf8_4byte_to_string` (~60 lines)
- [ ] 2.2 Add to CMakeLists.txt, build, test

## Phase 3: Extract random seed — 0/2

- [ ] 3.1 Create `compiler/runtime/core/essential_random.c`: move `tml_seed_counter` + `tml_random_seed` (~30 lines)
- [ ] 3.2 Add to CMakeLists.txt, build, test

## Phase 4: Extract tml_str_free + image ranges — 0/2

- [ ] 4.1 Create `compiler/runtime/memory/str_free.c`: move `tml_image_ranges[128]`, `tml_image_range_count`, `tml_image_ranges_initialized`, `tml_str_free`, `tml_str_free_register_module`, `tml_is_image_ptr` (~140 lines)
- [ ] 4.2 Add to CMakeLists.txt, build, test

## Phase 5: Extract FFI utils — 0/2

- [ ] 5.1 Create `compiler/runtime/ffi/ffi_utils.c`: move `tml_str_from_cstr`, `tml_free` (~30 lines, zero statics)
- [ ] 5.2 Add to CMakeLists.txt, build, test

## Phase 6: Verify — 0/2

- [ ] 6.1 Verify essential.c is under 1000 lines
- [ ] 6.2 Add comment to essential.c header documenting that Group C statics (panic/crash/VEH) must stay in one translation unit
