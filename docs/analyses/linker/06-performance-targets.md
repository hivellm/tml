# Performance Targets & Optimization Strategy

## 1. Baseline Measurements (Current State)

### What TML Has Today

TML uses a two-layer in-process backend with no subprocesses on the happy path:

1. **LLVM backend** (`compiler/src/backend/llvm_backend.cpp`): Compiles LLVM IR text to
   COFF `.obj` files using `LLVMParseIRInContext`, `LLVMRunPasses`, and
   `LLVMTargetMachineEmitToFile`. Each compilation unit writes to disk via
   `LLVMTargetMachineEmitToFile`, even though `compile_ir_to_buffer` (which uses
   `LLVMTargetMachineEmitToMemoryBuffer`) is already implemented and unused in the main path.

2. **Embedded LLD** (`compiler/src/backend/lld_linker.cpp`): Links `.obj` files to `.exe`
   and `.dll` in-process using `lld::lldMain()`. LLD is vendored in `src/llvm-project/`
   and linked statically. The call is serialized (LLD is not re-entrant) and guarded by a
   15-second deadlock timeout.

The critical bottleneck is the `.obj` round-trip: LLVM emits to disk, LLD reads from disk,
both making unnecessary I/O syscalls for data that could stay in memory.

### Estimated Baseline Times

These are representative estimates based on TML's pipeline and published LLD benchmarks.
They should be validated against actual measurement before Phase 2 work begins.

| Scenario | LLVM codegen | LLD link | Total | Output size |
|----------|-------------|---------|-------|-------------|
| Hello world (1 CU) | ~80ms | ~50ms | ~130ms | ~50 KB |
| Medium app (10 CUs) | ~400ms | ~100ms | ~500ms | ~300 KB |
| Large test suite (50 CUs) | ~2000ms | ~300ms | ~2300ms | ~2 MB |
| XLarge synthetic (200 CUs) | ~8000ms | ~1200ms | ~9200ms | ~10 MB |

All times assume debug build (O0), NVMe SSD, Ryzen/Core i7 class CPU. Measurements are
wall-clock from first `lld::lldMain` call to output file visible on disk.

### Where Time Is Spent in the Link Phase

Profiling data from the LLD project and the Mold linker paper (Rui Ueyama, 2021) give
realistic breakdowns for COFF linking of medium-sized programs:

| Phase | Fraction of link time | Notes |
|-------|-----------------------|-------|
| File I/O: read `.obj` from disk | 25-35% | Eliminated by in-memory pass |
| Symbol table construction | 10-15% | Hash table, archive extraction |
| Section layout computation | 8-12% | Alignment, VMA assignment |
| Section data copy (merge) | 20-30% | Largest single cost |
| Relocation processing | 10-15% | Address patching |
| PE header + directory write | 5-8% | Fixed-size, not input-dependent |
| LLD framework overhead | 8-12% | Argument parsing, driver dispatch, global init |

For TML's use case the LLD framework overhead is disproportionately large because TML
programs are small relative to the applications LLD was designed for.

### Known Bottlenecks in the Current Pipeline

1. **`LLVMTargetMachineEmitToFile` disk round-trip**: Each compilation unit writes a
   temporary `.obj` file. On Windows, `CreateFile`/`WriteFile`/`CloseHandle` is followed
   immediately by LLD's `OpenFile`/`ReadFile`/`CloseHandle`. The data never needs to leave
   memory. For a 50-CU build this means 100 unnecessary syscall sequences.

2. **LLD mutex serialization**: `g_lld_mutex` ensures only one link runs at a time.
   This is necessary due to LLD's global mutable state (`CommonLinkerContext`). A custom
   linker with no global state removes this constraint entirely.

3. **LLD `canRunAgain` poisoning**: If LLD returns `canRunAgain=false`, all subsequent
   compilations fall back to subprocess linking. This can silently degrade an entire test
   run from ~100ms per suite to ~500ms per suite.

4. **LLD initialization overhead**: `lld::lldMain` reinitializes internal state on each
   call because the cleanup path in LLD-COFF destroys all allocators. For a build with
   many small DLLs (like TML's test suite, which compiles one DLL per test suite), this
   overhead is paid repeatedly.

---

## 2. Phase-by-Phase Performance Targets

### Phase 2: In-Memory Object Passing

**What changes**: Switch the main codegen path from `compile_ir_to_object` (disk) to
`compile_ir_to_buffer` (memory). Pass `LLVMMemoryBufferRef` objects to LLD using
`lld::coff::link` with `MemoryBuffer` inputs instead of file paths.

The `compile_ir_to_buffer` implementation already exists at line 338 of `llvm_backend.cpp`
and works correctly — it uses `LLVMTargetMachineEmitToMemoryBuffer`. The gap is on the LLD
side: LLD-COFF accepts `MemoryBufferRef` inputs when called via the C++ API directly,
bypassing argument parsing.

| Metric | Before (file I/O) | After (in-memory) | Change |
|--------|-------------------|-------------------|--------|
| Disk reads per CU | 1 (LLD reads .obj) | 0 | -100% |
| Disk writes per CU | 1 (LLVM writes .obj) | 0 | -100% |
| Syscalls per CU | ~8 (open+read+close x2) | 0 | -100% |
| Link time, hello world | ~50ms | ~35ms | 1.4x |
| Link time, medium (10 CU) | ~100ms | ~70ms | 1.4x |
| Link time, large (50 CU) | ~300ms | ~200ms | 1.5x |
| Peak RSS (hello) | ~80 MB | ~90 MB | +12% |
| Peak RSS (large) | ~120 MB | ~150 MB | +25% |

The RSS increase is acceptable: in-memory `.obj` data replaces disk I/O, and the OS no
longer needs to buffer file pages. The net effect is fewer page faults and better cache
locality.

**Acceptance criteria**: Hello world links in under 40ms on a warm cache. No temporary
`.obj` files appear in `build/debug/` during a normal compilation run.

### Phase 3: Custom PE/COFF Writer (tml-link)

**What changes**: Replace LLD-COFF with a purpose-built PE/COFF linker for TML output.
LLD supports dozens of COFF quirks inherited from decades of MSVC compatibility. TML only
needs to produce valid PE executables and DLLs from LLVM-generated COFF objects. The
custom linker handles exactly that subset with a fraction of the code.

| Metric | LLD (embedded) | tml-link Phase 3 | Improvement |
|--------|---------------|-----------------|-------------|
| Link time, hello world | ~35ms | ~5ms | 7x |
| Link time, medium (10 CU) | ~70ms | ~12ms | 6x |
| Link time, large (50 CU) | ~200ms | ~40ms | 5x |
| Link time, XLarge (200 CU) | ~800ms | ~150ms | 5x |
| Peak RSS | ~100 MB | ~30 MB | 3.3x less |
| Startup overhead per link call | ~10ms (LLD global init) | ~0.5ms | 20x |
| Re-entrant | No (mutex required) | Yes | eliminates serial bottleneck |

Why a custom writer is faster than LLD for this workload:

- **No framework overhead**: LLD parses arguments, dispatches through driver layers, and
  initializes multiple subsystems even for a 50 KB binary. tml-link receives structured
  input and starts work immediately.
- **Direct in-memory pipeline**: Input is `std::vector<uint8_t>` from Phase 2; output is
  written directly to `LLVMMemoryBufferRef` (or mapped file). No `MemoryBuffer`
  conversion step.
- **Simpler symbol table**: LLVM-generated COFF uses a small, predictable symbol set.
  No COMDAT resolution ambiguity, no weak externals from Fortran, no ARM thumb interwork
  stubs. The symbol table fits in a single open-addressed hash map.
- **Arena allocator**: All linker data structures (section descriptors, symbol entries,
  relocation arrays) allocate from a monotonic arena. Teardown is a single `free`.
- **No LLD global state**: tml-link is stateless between calls. Multiple link operations
  can run concurrently on separate threads without any mutex.

**Reference: mold linker numbers**

Rui Ueyama's mold linker (2021, ELF only) achieves 1.5 seconds for linking Chromium
(2.1 GB of input) on 8 cores. For comparison, LLD takes 12 seconds (8x slower) and GNU ld
takes 53 seconds (35x slower). Mold's key insight: most link time is spent copying section
data, which is embarrassingly parallel. tml-link targets the same insight for PE/COFF.

**Acceptance criteria**: Hello world links in under 8ms. The linker passes the full TML
test suite (all test DLLs link correctly). Produced PE files pass `dumpbin /headers` and
load correctly via `LoadLibraryW`.

### Phase 4: Incremental Linking

**What changes**: Instead of relinking the entire output binary when a function changes,
tml-link patches the existing binary in-place. Functions are padded to the next 16-byte
boundary at their first compilation; subsequent recompiles overwrite the old bytes directly
(using the NOP sled between the end of the function and the next boundary). Symbols that
grow beyond their original allocation require a full relink.

This matches the approach used by the Zig self-hosted linker (described in
`01-zig-linker-architecture.md`), which achieves sub-millisecond relink times for
small changes by memory-mapping the output binary and writing changed sections directly.

| Change type | Full relink | Incremental | Speedup |
|-------------|-------------|------------|---------|
| 1 function body, same size | ~12ms | ~1ms | 12x |
| 5 function bodies, same size | ~12ms | ~3ms | 4x |
| New function added | ~12ms | ~12ms (full) | 1x |
| New symbol exported | ~12ms | ~12ms (full) | 1x |
| Large project, 1 fn change | ~150ms | ~3ms | 50x |
| Large project, 5 fn change | ~150ms | ~8ms | 19x |

The incremental path only applies when: (a) the symbol existed in the previous link, (b)
the new machine code fits within the allocated padding, and (c) no new external symbols are
referenced. When any condition fails, tml-link falls back to a full relink automatically.

**Persistence**: The incremental state (symbol table + section layout + padding map) is
stored in `.incr-cache/link-state.bin` alongside the existing incremental compilation
cache. The format uses the same fingerprint-based invalidation as the existing
`.incr-cache/incr.bin`.

**Acceptance criteria**: A one-function change in a medium project (10 CUs) relinks in
under 3ms. The incremental binary is byte-for-byte identical in behavior to a full relink
(validated by running the test suite against both outputs).

---

## 3. Benchmark Suite Design

### Test Projects

The benchmark suite uses four synthetic projects that span the realistic range of TML
program sizes.

**hello** (1 CU, ~80 symbols, ~40 KB PE output)
- Purpose: measure irreducible overhead — LLD init, argument parsing, PE header write
- Functions: main + 5 helper functions, one import from tml_runtime
- Expected tml-link time (Phase 3): 2-5ms

**medium** (10 CUs, ~800 symbols, ~250 KB PE output)
- Purpose: measure symbol resolution and section merge scalability
- Functions: 80 functions spread across 10 modules, 3 imported DLLs
- Expected tml-link time (Phase 3): 10-15ms

**large** (50 CUs, ~4000 symbols, ~1.5 MB PE output)
- Purpose: measure parallel section processing and hash table performance
- Functions: 400 functions, matches the TML standard library test suite scale
- Expected tml-link time (Phase 3): 35-50ms

**xlarge** (200 CUs, ~40000 symbols, ~8 MB PE output, synthetic)
- Purpose: worst-case stress test; validates memory budget and scalability
- Functions: 1600 functions, comparable to a medium-sized application
- Expected tml-link time (Phase 3): 120-180ms

### Benchmark Harness

```
build/debug/bin/tml-bench link <project> [--iterations=N] [--warmup=M] [--linker=lld|tml]
```

The harness reports:
- Median wall-clock time over N iterations (default: 20) after M warmup runs (default: 3)
- P95 and P99 wall-clock time (jitter indicator)
- Peak RSS via Windows `GetProcessMemoryInfo` Job Object tracking
- User + kernel CPU time via `GetProcessTimes`
- Total bytes read + written via ETW I/O counters (when available)
- Output file size in bytes

Results are written to `.benchmark-history.json` in NDJSON format (one record per run),
enabling trend visualization across commits.

### Regression Gate

A benchmark regression check runs as part of the CI pipeline on every commit that touches
`compiler/src/backend/` or `compiler/include/backend/`. The check fails if:

- Median link time increases by more than 10% vs the rolling 7-day baseline
- Peak RSS increases by more than 20% vs the rolling 7-day baseline
- P99 link time exceeds 3x the median (jitter regression)

The `.benchmark-history.json` is stored as a CI artifact and fetched by the regression
check job.

---

## 4. Optimization Techniques

### Memory-Mapped I/O for Input Files

When linking against static libraries (`.lib` archives), map the file into virtual memory
using `CreateFileMapping` + `MapViewOfFile` (Windows) or `mmap` (POSIX) instead of
reading with `ReadFile`/`read`. The OS loads only the pages that are actually accessed;
for thin archives where most member objects are not pulled in, this avoids reading the
entire file.

Expected benefit: 20-30% reduction in link time for builds with large static libraries.
No benefit for linking only compiler-generated `.obj` objects (already in memory after
Phase 2).

### Monotonic Arena Allocator

All linker data structures — section descriptors, symbol table entries, relocation arrays,
string table entries — are allocated from a pre-committed arena (1 MB initial commit,
growing in 1 MB slabs on overflow). Deallocation is a single `VirtualFree` / `munmap` at
link completion, eliminating per-object `free` overhead and fragmentation.

This mirrors the approach used by the Zig linker's `ArenaAllocator` and mold's
`MallocAllocator` with a pool. Both report 20-40% allocation speedup vs `malloc`/`free`
for linker workloads where all allocations have the same lifetime as the link operation.

Expected benefit: 15-30% fewer CPU cycles in symbol resolution and section merge phases.

### Parallel Section Merging

Output sections (`.text`, `.data`, `.rdata`, `.bss`, `.pdata`) are independent of each
other after section layout is computed. Each output section can be filled in parallel:

```
Layout phase (serial):
  Compute VMA for all sections → complete dependency graph

Merge phase (parallel, one task per output section):
  .text worker:  copy+patch all .text input sections → output buffer
  .data worker:  copy+patch all .data input sections → output buffer
  .rdata worker: copy+patch all .rdata input sections → output buffer
  .pdata worker: build RUNTIME_FUNCTION table → output buffer

Write phase (serial):
  Assemble PE headers → write output file
```

Relocation processing is parallelized within each worker: each worker owns its output
section exclusively, so no synchronization is needed during the patch phase. The thread
pool uses the same `std::thread` pool already present in the TML test coordinator.

Expected benefit: 2-4x speedup on the merge phase for large projects on 4+ core machines.
For hello world (single section), no benefit.

### Open-Addressed Symbol Table

LLD uses `llvm::DenseMap<StringRef, Symbol*>` for symbol resolution. For TML's use case
(LLVM-generated symbols with predictable naming patterns), a Robin Hood open-addressed hash
table provides better cache behavior:

- 8-byte key: pointer into the string table (intern comparison is pointer equality)
- 8-byte value: pointer to symbol descriptor in the arena
- Load factor target: 0.7 (rehash when 70% full)
- Lookup: 1-2 cache lines for typical table sizes up to 50K symbols

String interning: all symbol names are deduplicated into a single `std::vector<char>`
string pool on first encounter. Subsequent lookups compare pointers, not string content.

Expected benefit: 15-25% faster symbol resolution phase. For hello world, negligible.

### SIMD Relocation Processing

The most common relocation types in LLVM-generated COFF are:
- `IMAGE_REL_AMD64_ADDR64` (8 bytes, add 64-bit VA)
- `IMAGE_REL_AMD64_REL32` (4 bytes, add 32-bit relative offset)

For code sections where the majority of relocations are `REL32` (function calls and
data references), the patch loop can be vectorized: load 4 relocation targets at once
using SSE2, add the base address delta, and store. This applies when the linker is
performing a base relocation (rebase), not during the initial link where each relocation
target has a unique addend.

Expected benefit: 10-20% faster relocation phase for large `.text` sections with many
calls. Not measurable for hello world.

### Zero-Copy Output for In-Memory Consumers

When the TML test coordinator immediately loads the linked DLL (via `LoadLibraryW`), the
output binary does not need to be written to disk first. tml-link can produce output into
a `VirtualAlloc`ed buffer and pass it directly to `LoadLibraryW`'s in-memory variant
(`RtlCreateUserProcess` with a custom image section, or the `CreateFileMappingFromApp`
trick). This eliminates the final output write for the most common TML use case (compiling
and immediately executing test DLLs).

This is an advanced optimization that requires Windows-specific implementation. Defer to
Phase 5 or later.

### Profile-Guided Section Ordering (Runtime Benefit)

At link time, tml-link can reorder functions within `.text` based on call frequency data
from a previous profile run. Hot functions are packed together, reducing instruction cache
misses during execution. This does not improve link time but improves the runtime
performance of linked binaries by 5-15% on call-intensive workloads.

Input format: a JSON file mapping mangled symbol names to call counts, generated by the
TML profiler (`mcp__tml__profile`). The linker reads this file if `--profile-order=<file>`
is specified.

---

## 5. Comparison Benchmarks

### Linkers Tracked

| Linker | Platform | Strategy | Published speed |
|--------|----------|----------|-----------------|
| LLD-COFF (embedded) | Windows | Current baseline, in-process | ~50ms hello world |
| LLD-COFF (subprocess) | Windows | Fallback path | ~150ms hello world |
| MSVC link.exe | Windows | Traditional | ~200-400ms hello world |
| mold | Linux/ELF | Parallel, lock-free | 1.5s Chromium (2.1GB input) |
| Zig linker | All | Incremental, in-process | sub-ms incremental relink |
| GNU gold | Linux/ELF | Reference | 12s Chromium |
| GNU ld | Linux/ELF | Traditional | 53s Chromium |

The mold and Zig numbers are from their respective published benchmarks (mold: GitHub
repository README, 2023; Zig: Andrew Kelley's "A Practical Guide to Applying Data-Oriented
Design", Handmade Seattle 2021). The MSVC and LLD numbers are from measured TML pipeline
runs and corroborated by the LLVM linker benchmark suite.

### Where tml-link Should Land

| Milestone | Hello world link | Medium link | Large link | Incremental |
|-----------|-----------------|------------|-----------|------------|
| Today (LLD embedded) | ~50ms | ~100ms | ~300ms | not supported |
| Phase 2 (in-memory obj) | ~35ms | ~70ms | ~200ms | not supported |
| Phase 3 (tml-link) | ~5ms | ~12ms | ~40ms | not supported |
| Phase 4 (incremental) | ~5ms | ~12ms | ~40ms | ~2ms (1 fn) |

On the Chromium-scale comparison (to contextualize against mold): if tml-link achieves the
same 5x speedup over LLD that mold achieves over LLD on ELF, a hypothetical xlarge build
(200 CUs, ~8 MB output) would link in ~160ms vs LLD's ~800ms.

---

## 6. Profiling Strategy

### Instrumentation

tml-link uses the same `TML_ZONE` / `TML_TIMER` macros already present in the compiler:

```cpp
auto link(const LinkInput& input) -> LinkResult {
    TML_ZONE("tml_link::total");

    TML_ZONE_BEGIN("tml_link::parse");
    auto sections = parse_all_objects(input.objects);
    TML_ZONE_END("tml_link::parse");

    TML_ZONE_BEGIN("tml_link::resolve");
    auto symtab = resolve_symbols(sections);
    TML_ZONE_END("tml_link::resolve");

    TML_ZONE_BEGIN("tml_link::layout");
    auto layout = compute_layout(sections, symtab);
    TML_ZONE_END("tml_link::layout");

    TML_ZONE_BEGIN("tml_link::merge");
    auto output = merge_sections(sections, layout, symtab);
    TML_ZONE_END("tml_link::merge");

    TML_ZONE_BEGIN("tml_link::write");
    write_pe(output, input.output_path);
    TML_ZONE_END("tml_link::write");
}
```

When `TML_PROFILING=1` is set, the zone data is written to the same profiler output as
the rest of the compiler pipeline, making it visible in the flamegraph viewer.

### What to Measure on Each Phase

**Phase 2 gate (before merging)**:
- `tml-bench link hello --linker=lld` median < 40ms
- No `.obj` files created in `build/debug/` during compilation
- `tml-bench link medium --linker=lld` median < 75ms

**Phase 3 gate (before merging)**:
- `tml-bench link hello --linker=tml` median < 8ms
- `tml-bench link medium --linker=tml` median < 15ms
- All test suites pass with tml-link output
- `dumpbin /headers` reports valid PE signature and correct section counts

**Phase 4 gate (before merging)**:
- Incremental relink of 1-function change in medium project: median < 3ms
- Full relink triggered correctly when function grows beyond padding
- Incremental output passes the same test suite as full relink output

### Profiling Decision Rule

No optimization is implemented without a profile showing it is the bottleneck. The
workflow is:

1. Run `tml-bench link large --iterations=50` and collect profiler output
2. Identify the phase consuming more than 15% of total link time
3. Implement the optimization targeting that phase
4. Re-run the benchmark to confirm improvement
5. If improvement is less than 10%, revert the optimization (complexity not worth it)

This prevents premature optimization of phases that are not actually bottlenecks.

---

## 7. Memory Budget

### Per-Project Targets (Peak RSS, tml-link Phase 3)

| Project | Input obj bytes | Peak RSS target | Notes |
|---------|----------------|----------------|-------|
| Hello world | ~15 KB | < 12 MB | Mostly tml-link binary + stack |
| Medium (10 CU) | ~150 KB | < 40 MB | All sections in arena |
| Large (50 CU) | ~750 KB | < 100 MB | Parallel merge buffers |
| XLarge (200 CU) | ~3 MB | < 300 MB | Arena + output buffer |

For comparison, LLD's embedded mode uses ~80-120 MB for a hello world link due to LLVM
infrastructure initialization (pass manager, TargetRegistry, global string interners).
tml-link targets under 12 MB for the same workload because it carries none of that.

### Memory Allocation Strategy

| Data | Allocator | Lifetime |
|------|-----------|---------|
| Input objects (Phase 2) | Passed from LLVM backend (already allocated) | Until merge complete |
| Section descriptors | Arena | Entire link |
| Symbol table entries | Arena | Entire link |
| String pool (names) | Arena (single `vector<char>`) | Entire link |
| Relocation arrays | Arena | Until relocations processed |
| Output buffer (merged sections) | `VirtualAlloc` (aligned) | Until PE written |
| PE header scratch | Stack (`std::array<uint8_t, 4096>`) | Stack frame |

After the merge phase, input object memory is released (if owned by the linker) before
the output write. For a 50 CU large build, this keeps peak RSS under 100 MB even when
input and output are simultaneously in memory.

### When to Stream Instead of Loading

For XLarge builds (200+ CUs, multi-MB output), holding all section data simultaneously
in memory approaches 300 MB. If the output section is larger than 4 MB, tml-link switches
to a streaming write strategy: sections are written incrementally to the output file using
`WriteFile` with 64 KB chunks, and the relocations for each chunk are processed before the
chunk is written. This trades slight link time increase (~5%) for a significant RSS
reduction (~40%).

The streaming threshold is configurable: `--stream-threshold=<bytes>` (default: 4 MB).

---

## 8. Open Questions and Risks

### Correctness Risks

**Relocation coverage**: LLD's COFF backend handles 30+ relocation types accumulated over
years of MSVC compatibility work. tml-link's initial implementation covers the 6 relocation
types that LLVM actually generates for x86-64:

- `IMAGE_REL_AMD64_ADDR64` — absolute 64-bit VA
- `IMAGE_REL_AMD64_ADDR32NB` — RVA (used in exception tables)
- `IMAGE_REL_AMD64_REL32` — 32-bit PC-relative (function calls)
- `IMAGE_REL_AMD64_REL32_1` through `REL32_5` — PC-relative with addend
- `IMAGE_REL_AMD64_SECTION` — section index (debug info)
- `IMAGE_REL_AMD64_SECREL` — section-relative offset (debug info)

Any COFF object from a non-LLVM compiler (MSVC, MinGW) may use relocation types outside
this set. tml-link rejects such objects with a clear error rather than silently mishandling
them. This is acceptable because TML only needs to link its own output.

**COMDAT deduplication**: LLD implements COMDAT section selection (pick-any, exact-match,
largest, newest, same-size). LLVM generates COMDAT groups for template instantiations and
`linkonce_odr` functions. tml-link must implement at minimum `IMAGE_COMDAT_SELECT_ANY` and
`IMAGE_COMDAT_SELECT_EXACT_MATCH`. Getting this wrong produces multiply-defined symbol
errors or incorrect deduplication.

**Import library generation**: When producing a DLL, tml-link must also produce a `.lib`
import library for consumers. This is a separate COFF archive with a specific structure
(short import objects, one per exported symbol). LLD's implementation is ~800 lines;
tml-link must match it exactly or TML DLLs will not link against other TML code.

### Performance Risks

**Thread contention in parallel merge**: If output sections have very different sizes
(e.g., `.text` is 10x larger than `.data`), parallel merge workers finish unevenly. The
write phase must wait for the slowest worker. Mitigations: work stealing, or
sub-partitioning `.text` into multiple parallel tasks.

**Arena fragmentation at scale**: A single monotonic arena for a 200 CU build may allocate
several hundred MB in small chunks. The arena's 1 MB slab size means the last slab is
almost always partially wasted. For XLarge builds, switch to a per-phase arena (reset after
parse, reset after resolve) to reclaim memory between phases.

**LLD fallback behavior**: During Phase 3 development, tml-link will not handle every edge
case. The linker must detect unsupported inputs early (unrecognized COFF machine type,
debug info formats it cannot handle) and fall back to LLD automatically. The fallback
must be transparent to the caller — same `LLDLinkResult` return type, same output file.
