# Proposal: Split essential.c by Isolation Level

## Status: PROPOSED

## Why

`essential.c` (1,667 lines) contains the C runtime's core functions. A previous attempt to split it by changing `static` → `extern` broke test executables because the runtime is a **static library** — `extern` globals get duplicated across each linked binary, causing `longjmp` to target stale stack frames.

## Root Cause

The file has ~50 static globals. Most are tightly coupled (panic/crash/VEH/I/O share state). But several sections use **private statics that no other section touches**:

| Section | Private statics | Lines |
|---------|----------------|-------|
| Tracy profiler | `tml_zone_stack`, `tml_zone_top` | ~65 |
| UTF-8 encoding | `utf8_char_buffer` | ~60 |
| Random seed | `tml_seed_counter` | ~30 |
| `tml_str_free` + image ranges | `tml_image_ranges*` | ~140 |
| FFI utils | none | ~30 |

These sections can be extracted because their statics remain `static` in the new file — no linkage change needed.

## What Changes

Extract 5 independent sections to new .c files. The tightly-coupled core (panic/crash/VEH/I/O/test harness) stays in `essential.c` as a ~900-line file.

**Key constraint**: The panic/crash statics (`tml_panic_jmp_buf`, `tml_catching_panic`, `tml_crash_*`) MUST stay `static` in one translation unit. They cannot be `extern` because the static library model would give each linked binary its own copy.
