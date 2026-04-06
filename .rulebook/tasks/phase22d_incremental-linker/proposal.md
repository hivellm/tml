# Proposal: Incremental Linker — Sub-10ms Re-link in TML

**Task**: phase22d_incremental-linker
**Status**: Planned
**Priority**: P0
**Estimated effort**: 4–5 weeks
**Risk**: Medium
**LOC estimate**: ~5,500 LOC TML

## Problem

After phases 22a, 22b, and 22c, TML has its own linker on all three platforms. However,
even the fastest full link of a large project takes hundreds of milliseconds — comparable
to LLD's current performance. The TML development loop (edit → compile → link → run) is
bottlenecked by linking time when only one or a few source files change.

The TML compiler already has incremental compilation (`.incr-cache/incr.bin`) that
recompiles only changed TML source files. An incremental linker completes the loop: if
only one `.obj` file changed, only the affected sections and relocations should be
reprocessed, and the output binary should be patched in place rather than rewritten.

Target: single-file change → re-link in < 10ms. Full link remains the fallback.

## Proposed Solution

After each full link, write a `.link-state` file alongside the output binary recording
the complete state of the link: input fingerprints, the resolved symbol table, section
layout (RVAs and file offsets), and a log of every relocation applied. On the next link
invocation, load this state, fingerprint each input `.obj`, and route to the incremental
path if only a subset of inputs changed.

**Link state database** — A compact binary-encoded file stores `HashMap[Str, U64]`
fingerprints for all inputs, the symbol table, section layout, and relocation log. The
relocation log is the key data structure: it records which bytes in the output binary were
written by which relocation, so the incremental engine knows exactly which bytes to recheck
after a change.

**Change detection** — xxHash3 fingerprinting (non-cryptographic, extremely fast) determines
which input `.obj` files changed since the last link. If a file's fingerprint matches the
stored value, it is guaranteed unchanged. Changed files are re-parsed; unchanged files use
their stored symbol and section information.

**Delta relocation** — For each changed `.obj`, only the sections that actually changed
(determined by content hashing each section) need relocation reprocessing. Additionally,
any relocation in an unchanged section that references a symbol whose address changed must
also be reprocessed ("transitive relocation reapplication"). This set is computed by
querying the relocation log.

**In-place patching** — Rather than rewriting the entire output binary, `apply_patches()`
opens the existing binary for read-write and writes only the changed bytes at their exact
file offsets. For a single-file change affecting one `.text` section, this means writing
perhaps 4KB of data, which takes under 1ms on any modern SSD.

**Fallback conditions** — The incremental path is only valid when section sizes don't
change (the binary layout remains stable). If a function grows, all downstream sections
shift, requiring a full link. Similarly, adding or removing exported symbols, or changing
the set of linked libraries, requires a full link. These fallbacks are detected quickly
and produce a correct result.

Target: ~5,500 LOC TML across five files.

## Key Decisions

- **xxHash3 for fingerprinting** — SHA-256 is too slow for fingerprinting thousands of
  files before deciding whether to use the incremental path. xxHash3 runs at memory
  bandwidth speed (~20 GB/s on modern hardware) and has no known collisions for file data.
  It is available in `lib/std/src/hash/xxhash.tml`.

- **`.link-state` file alongside output binary** — Not inside `.incr-cache/` because the
  state is specific to one output binary, not to the source tree. Moving the output binary
  invalidates the state naturally (the `.link-state` file stays at the old path).

- **Section-level granularity, not symbol-level** — Symbol-level tracking would allow
  patching only the bytes for changed functions, but requires tracking intra-section
  positions for every symbol. Section-level granularity is simpler, still fast, and
  correct.

- **LLD fully eliminated after this phase** — Phases 22a–22d together cover all linking
  on all three platforms (Windows: PE/COFF, Linux: ELF, macOS: Mach-O) with incremental
  support. After phase 22d, `lld_linker.cpp` can be removed from the compiler source.

- **Correctness test: bit-for-bit identity** — The incremental link must produce a binary
  that is bit-for-bit identical to a fresh full link of the same inputs (except for fields
  that embed timestamps, which should be zero or deterministic). This test catches any
  relocation recomputation bugs.

## Files to Create

| File | LOC | Purpose |
|------|-----|---------|
| `compiler-tml/src/link/incr/state.tml` | ~1,000 | LinkState type, save/load |
| `compiler-tml/src/link/incr/fingerprint.tml` | ~400 | xxHash3 file fingerprinting |
| `compiler-tml/src/link/incr/delta_reloc.tml` | ~1,200 | Delta relocation computation |
| `compiler-tml/src/link/incr/patch.tml` | ~800 | In-place binary patching |
| `compiler-tml/src/link/incr/mod.tml` | ~600 | Orchestration, fallback logic |
| `compiler-tml/src/link/incr/error.tml` | ~300 | Typed incremental link errors |

## Success Criteria

- Single `.obj` change in a 1,000-file project re-links in < 10ms (measured wall clock).
- The incrementally-linked binary is bit-for-bit identical to a fresh full link for at
  least 20 distinct single-file change scenarios.
- All fallback conditions (symbol added/removed, section grew, library set changed) trigger
  a correct full link, and the next incremental link after the fallback succeeds.
- After completing phase 22d, `lld_linker.cpp` is removed and LLD is no longer a dependency
  of `tml.exe` on any platform. The binary size drops by ~350 MB.

## Dependencies

- **Depends on**: phase22a — PE/COFF linker (Windows incremental support).
- **Depends on**: phase22b — ELF linker (Linux incremental support).
- **Depends on**: phase22c — Mach-O linker (macOS incremental support).
- **Reference**: mold linker's incremental linking notes, the LLVM ThinLTO cache (similar
  fingerprint-based approach), Microsoft's LINK.EXE incremental linking (`.ilk` files).
